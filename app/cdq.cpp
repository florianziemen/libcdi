#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "printinfo.h"

#include "tsio.h"
#include "cdi.h"
#include "cdi_cpp.h"
#include "cdi_query.h"

#include <iostream>
#include <optional>
#include <string>
#include <sstream>
#include <thread>
#include <vector>

namespace
{

void
print_usage(const char *prog)
{
  std::cerr << "Usage: " << prog
            << " --path <PATH> [--name a,b,c] "
               "[--kvstore file|s3|http] [--format zarr2|zarr3] [--endpoint <URL>] "
               "[--step <S/E>] [--consolidated] [--group <NAME>] "
               "[--out-path <PATH>] [--out-format zarr2|zarr3|n5] "
               "[--dtype float64|float32|float16|bfloat16] "
               "[--compressor none|blosc|gzip|zstd] "
               "[--step-chunk <N>] [--cell-chunk <N>] "
               "[--overwrite] "
               "[--data-copy-concurrency <N>] [--double-buffer] "
               "[--layer <S/E>] [--cell <S/E>] [--info] [--debug]\n"
            << "  --name: comma-separated array names.  When omitted, every\n"
            << "         data array in the dataset is selected; coordinate\n"
            << "         arrays are excluded (they are selected automatically\n"
            << "         alongside the data arrays).  A Zarr 3 root that only\n"
            << "         contains groups lists those groups instead.\n"
            << "  --group: name of a nested Zarr 3 group below --path.  Array\n"
            << "         metadata is served from the consolidated `zarr.json`\n"
            << "         of the store root (no recursive metadata reads).\n"
            << "         When --group is omitted and --path is a Zarr 3 root\n"
            << "         that only contains groups, all groups are listed.\n"
            << "  --kvstore: 'file', 's3' or 'http' (default: auto).\n"
            << "    Auto-detect: '://' in --path => http, else file.\n"
            << "  file:  --path is a filesystem directory\n"
            << "  http:  --path is a full \"https://host/base/group\" URL.  The array\n"
            << "         metadata is fetched and sanitized (invalid bare NaN/Infinity\n"
            << "         JSON tokens are quoted) before reading.\n"
            << "  s3:    --path is \"bucket/prefix\" or a full \"https://endpoint/bucket/prefix\" URL;\n"
            << "         --endpoint overrides the endpoint derived from the URL\n"
            << "  --format: force Zarr 2 or 3 as 'zarr2'/'zarr3' (default: auto-detect)\n"
            << "  --out-path: write the selected data arrays to a NEW Zarr store\n"
            << "         at PATH (one array per selected data array, float32);\n"
            << "         the used coordinate arrays are written alongside them,\n"
            << "         preserving their dtype.  When set, only the header\n"
            << "         tables are printed - no time series table is computed.\n"
            << "         The data is read via the same chunk window loops as the\n"
            << "         min/mean/max mode.\n"
            << "  --overwrite: replace an existing store at --out-path (it is\n"
            << "         deleted first).  Without it, an existing --out-path is\n"
            << "         rejected.  Only valid with --out-path.\n"
            << "  --out-format: output store format: zarr2 or zarr3 (Zarr) or n5 (default zarr3).\n"
            << "  --dtype: output data type of the float data arrays for --out-format\n"
            << "         zarr3: float64, float32, float16 or bfloat16 (default float32).\n"
            << "         Zarr 2 and N5 outputs always use float32.\n"
            << "  --compressor: output compressor for --out-path: 'none' disables\n"
            << "         compression, else blosc/gzip/zstd (default: reuse the input\n"
            << "         compression).  Applies to the data arrays and the coordinate\n"
            << "         arrays.\n"
            << "  --step-chunk, --cell-chunk: for --out-path, override the output\n"
            << "         chunk length of the step (time) resp. cell (last) dimension;\n"
            << "         the values are clamped to the selected extent.  Other\n"
            << "         dimensions keep the clamped source chunks (default).\n"
            << "  --data-copy-concurrency: cap the number of chunks that are\n"
            << "         decompressed concurrently (default: unbounded).  Useful for\n"
            << "         very large arrays, e.g. with many chunks on local disk.\n"
            << "  --consolidated: read Zarr 2 metadata from the group-level\n"
            << "         `.zmetadata` file instead of the per-array metadata\n"
            << "  --double-buffer: use a double-buffered chunk cache.  While one\n"
            << "         time-window is reduced, the next window is already being\n"
            << "         filled in the background (doubles the cached data size).\n"
            << "  Hypercube extraction, 1-based inclusive START/END ranges\n"
            << "  (default: the whole extent of the dimension):\n"
            << "  --step <S/E>:  timesteps of the time dim\n"
            << "  --layer <S/E>: layers of dim 1, rank >= 3\n"
            << "  --cell <S/E>:  grid points of the last dim\n"
            << "         Only the chunks overlapping the hypercube are read, so the\n"
            << "         cache is sized exactly to the requested data.\n"
            << "  --info: print the TensorStore version (works without --path).\n"
            << "         With --path, print only the present array names (the\n"
            << "         Selected/Coordinate arrays tables) and no measurements.\n"
            << "  --debug: write detection details to stderr: one line with\n"
            << "         the Zarr format of the dataset, and a further line\n"
            << "         when consolidated metadata was found.\n";
}

// into `range`.  Both bounds must be positive and END must not be smaller than
// START; the whole value must be consumed.
bool
parse_index_range(const std::string &opt, const std::string &value, tsRange &range)
{
  const auto invalid = [&]() -> bool
  {
    std::cerr << "Invalid " << opt << " value: " << value << " (expected START/END, both positive 1-based indices, END >= START)\n";
    return false;
  };

  const std::size_t slash = value.find('/');
  if (slash == std::string::npos || value.find('/', slash + 1) != std::string::npos) return invalid();

  const auto parse_bound = [&](const std::string &text, std::ptrdiff_t &out) -> bool
  {
    if (text.empty()) return false;
    errno = 0;
    char *endp = nullptr;
    const long long n = std::strtoll(text.c_str(), &endp, 10);
    if (errno != 0 || endp != text.c_str() + text.size() || n <= 0) return false;
    out = static_cast<std::ptrdiff_t>(n);
    return true;
  };

  std::ptrdiff_t start = 0, end = 0;
  if (!parse_bound(value.substr(0, slash), start)) return invalid();
  if (!parse_bound(value.substr(slash + 1), end)) return invalid();
  if (end < start) return invalid();

  range.start = start;
  range.end = end;
  return true;
}

bool
parse_args(int argc, char **argv, tsConfig &cfg)
{
  for (int i = 1; i < argc; ++i)
  {
    const std::string arg = argv[i];
    auto need_value = [&](const char *name) -> const char *
    {
      if (i + 1 >= argc)
      {
        std::cerr << "Missing value for " << name << "\n";
        return nullptr;
      }
      return argv[++i];
    };

    if (arg == "--format")
    {
      const char *v = need_value("--format");
      if (!v) return false;
      const std::string s = v;
      if (s == "zarr2")
        cfg.format = 2;
      else if (s == "zarr3")
        cfg.format = 3;
      else
      {
        std::cerr << "Invalid zarr format: " << s << " (must be zarr2 or zarr3)\n";
        return false;
      }
    }
    else if (arg == "--kvstore")
    {
      const char *v = need_value("--kvstore");
      if (!v) return false;
      const std::string s = v;
      if (s != "file" && s != "s3" && s != "http")
      {
        std::cerr << "Invalid kvstore: " << s << " (must be file, s3 or http)\n";
        return false;
      }
      cfg.kvstore = s;
    }
    else if (arg == "--path")
    {
      const char *v = need_value("--path");
      if (!v) return false;
      cfg.path = v;
    }
    else if (arg == "--name")
    {
      const char *v = need_value("--name");
      if (!v) return false;
      std::stringstream ss(v);
      std::string item;
      while (std::getline(ss, item, ','))
      {
        if (!item.empty()) cfg.arrays.push_back(item);
      }
    }
    else if (arg == "--endpoint")
    {
      const char *v = need_value("--endpoint");
      if (!v) return false;
      cfg.endpoint = v;
    }
    else if (arg == "--step" || arg == "--layer" || arg == "--cell")
    {
      const char *v = need_value(arg.c_str());
      if (!v) return false;
      tsRange &range = (arg == "--step") ? cfg.step : (arg == "--layer" ? cfg.layer : cfg.cell);
      if (!parse_index_range(arg, v, range)) return false;
    }
    else if (arg == "--step-chunk" || arg == "--cell-chunk")
    {
      const char *v = need_value(arg.c_str());
      if (!v) return false;
      errno = 0;
      char *endp = nullptr;
      const long long n = std::strtoll(v, &endp, 10);
      if (errno != 0 || endp != v + std::strlen(v) || n <= 0)
      {
        std::cerr << "Invalid " << arg << " value: " << v << " (must be a positive integer)\n";
        return false;
      }
      std::optional<std::int64_t> &slot = (arg == "--step-chunk") ? cfg.step_chunk : cfg.cell_chunk;
      slot = n;
    }
    else if (arg == "--consolidated") { cfg.consolidated = true; }
    else if (arg == "--double-buffer") { cfg.double_buffer = true; }
    else if (arg == "--overwrite") { cfg.overwrite = true; }
    else if (arg == "--info") { cfg.info = true; }
    else if (arg == "--debug") { cfg.debug = true; }
    else if (arg == "--out-path")
    {
      const char *v = need_value("--out-path");
      if (!v) return false;
      cfg.out_path = v;
    }
    else if (arg == "--out-format")
    {
      const char *v = need_value("--out-format");
      if (!v) return false;
      const std::string s = v;
      if (s == "zarr2" || s == "zarr3" || s == "n5")
        cfg.out_format = s;
      else
      {
        std::cerr << "Invalid --out-format value: " << v << " (must be zarr2, zarr3 or n5)\n";
        return false;
      }
    }
    else if (arg == "--dtype")
    {
      const char *v = need_value("--dtype");
      if (!v) return false;
      const std::string s = v;
      if (s == "float64" || s == "float32" || s == "float16" || s == "bfloat16")
        cfg.out_dtype = s;
      else
      {
        std::cerr << "Invalid --dtype value: " << v << " (must be float64, float32, float16 or bfloat16)\n";
        return false;
      }
    }
    else if (arg == "--compressor")
    {
      const char *v = need_value("--compressor");
      if (!v) return false;
      const std::string s = v;
      if (s == "none" || s == "blosc" || s == "gzip" || s == "zstd")
        cfg.compressor = s;
      else
      {
        std::cerr << "Invalid --compressor value: " << v << " (must be none, blosc, gzip or zstd)\n";
        return false;
      }
    }
    else if (arg == "--group")
    {
      const char *v = need_value("--group");
      if (!v) return false;
      cfg.group = v;
    }
    else if (arg == "--data-copy-concurrency")
    {
      const char *v = need_value("--data-copy-concurrency");
      if (!v) return false;
      const long n = std::strtol(v, nullptr, 10);
      if (n <= 0)
      {
        std::cerr << "Invalid --data-copy-concurrency value: " << v << " (must be positive)\n";
        return false;
      }
      cfg.data_copy_concurrency = static_cast<int>(n);
    }
    else
    {
      auto numArgsLeft = argc - i;
      if (numArgsLeft == 1 && cfg.path.empty()) { cfg.path = argv[i]; }
      else if (numArgsLeft == 2 && cfg.path.empty())
      {
        cfg.path = argv[i];
        cfg.out_path = argv[++i];
      }
      else
      {
        std::cerr << "Unknown argument: " << arg << "\n";
        return false;
      }
    }
  }

  if (cfg.path.empty() && !cfg.info)
  {
    std::cerr << "Missing required argument(s).\n";
    return false;
  }
  return true;
}

}  // namespace

int vlistInqVarMissvalUsed(int vlistID, int varID);
#ifndef DBL_IS_NAN
#if defined(HAVE_DECL_ISNAN)
#define DBL_IS_NAN(x) (isnan(x))
#elif defined(FP_NAN)
#define DBL_IS_NAN(x) (fpclassify(x) == FP_NAN)
#else
#define DBL_IS_NAN(x) ((x) != (x))
#endif
#endif

#ifndef DBL_IS_EQUAL
/*#define DBL_IS_EQUAL(x,y) (!(x < y || y < x)) */
#define DBL_IS_EQUAL(x, y) (DBL_IS_NAN(x) || DBL_IS_NAN(y) ? (DBL_IS_NAN(x) && DBL_IS_NAN(y) ? 1 : 0) : !(x < y || y < x))
#endif

#ifndef IS_NOT_EQUAL
#define IS_NOT_EQUAL(x, y) (x < y || y < x)
#endif

static void
printInfo(CdiDateTime vdatetime, char *varname, double level, size_t datasize, int number, size_t nmiss, double missval,
          const double *data, int vardis)
{
  static int rec = 0;
  size_t ivals = 0, imiss = 0;
  double arrmean, arrmin, arrmax;

  if (!rec)
  {
    if (vardis)
      fprintf(stdout,
              "   Rec :       Date     Time   Level Gridsize    Miss :     Minimum        Mean     Maximum : Parameter name\n");
    //         ----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+
    else
      fprintf(stdout,
              "   Rec :       Date     Time   Level Gridsize    Miss :     Minimum        Mean     Maximum : Parameter ID\n");
    //         ----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+
  }

  char vdatestr[32], vtimestr[32];
  date2str(vdatetime.date, vdatestr, sizeof(vdatestr));
  time2str(vdatetime.time, vtimestr, sizeof(vtimestr));

  fprintf(stdout, "%6d :%s %s %7g ", ++rec, vdatestr, vtimestr, level);
  fprintf(stdout, "%8zu ", datasize);
  fprintf(stdout, "%7zu :", nmiss);

  if (number == CDI_REAL)
  {
    if (nmiss > 0)
    {
      arrmean = 0;
      arrmin = 1.e300;
      arrmax = -1.e300;
      for (size_t i = 0; i < datasize; i++)
      {
        if (!DBL_IS_EQUAL(data[i], missval))
        {
          if (data[i] < arrmin) arrmin = data[i];
          if (data[i] > arrmax) arrmax = data[i];
          arrmean += data[i];
          ivals++;
        }
      }
      imiss = datasize - ivals;
      datasize = ivals;
    }
    else
    {
      arrmean = data[0];
      arrmin = data[0];
      arrmax = data[0];
      for (size_t i = 1; i < datasize; i++)
      {
        if (data[i] < arrmin) arrmin = data[i];
        if (data[i] > arrmax) arrmax = data[i];
        arrmean += data[i];
      }
    }

    if (datasize > 0) arrmean /= (double) datasize;

    fprintf(stdout, "%#12.5g%#12.5g%#12.5g", arrmin, arrmean, arrmax);
  }
  else
  {
    size_t nvals_r = 0, nvals_i = 0;
    double arrsum_r = 0, arrsum_i = 0, arrmean_r = 0, arrmean_i = 0;

    for (size_t i = 0; i < datasize; i++)
    {
      if (!DBL_IS_EQUAL(data[i * 2], missval))
      {
        arrsum_r += data[i * 2];
        nvals_r++;
      }
      if (!DBL_IS_EQUAL(data[i * 2 + 1], missval))
      {
        arrsum_i += data[i * 2 + 1];
        nvals_i++;
      }
    }

    imiss = datasize - nvals_r;

    if (nvals_r > 0) arrmean_r = arrsum_r / (double) nvals_r;
    if (nvals_i > 0) arrmean_i = arrsum_i / (double) nvals_i;
    fprintf(stdout, "  -  (%#12.5g,%#12.5g)  -", arrmean_r, arrmean_i);
  }

  fprintf(stdout, " : %-14s\n", varname);

  if (imiss != nmiss && nmiss > 0) fprintf(stdout, "Found %zu of %zu missing values!\n", imiss, nmiss);
}

static int
handle_error(int cdiErrno, const char *fmt, ...)
{
  va_list args;

  va_start(args, fmt);

  printf("\n");
  vfprintf(stderr, fmt, args);
  fprintf(stderr, "\n");

  va_end(args);

  fprintf(stderr, "%s\n", cdiStringError(cdiErrno));

  return cdiErrno;
}

static int
check_tensorstore(const char **filename)
{
  const char ts[] = "ts:";
  int tsLen = (int) sizeof(ts) - 1;
  const char *uri = *filename;
  int len = (int) strlen(uri);
  if (len > tsLen && strncmp(uri, ts, tsLen) == 0)
  {
    *filename = uri + tsLen;
    return 1;
  }

  *filename = uri;
  return 0;
}

int
main(int argc, char **argv)
{
  tsConfig cfg;
  if (!parse_args(argc, argv, cfg))
  {
    print_usage(argv[0]);
    return 1;
  }
#ifdef HAVE_TENSORSTORE
  if (cfg.info) { std::cerr << "TensorStore " << tsVersion() << '\n'; }
#else
  if (cfg.info) { std::cerr << "TensorStore support: disabled (built with -DWITH_TENSORSTORE=OFF)\n"; }
#endif
  if (cfg.info)
  {
    unsigned int tc = std::thread::hardware_concurrency();
    if (tc == 0) tc = 1;
    std::cerr << "TensorStore threads: " << tc << '\n';
#ifdef C_COMPILER
    std::cerr << "C=" << C_COMPILER << '\n';
#endif
#ifdef C_COMPILER_VERSION
    std::cerr << "C version=" << C_COMPILER_VERSION << '\n';
#endif
#ifdef CXX_COMPILER
    std::cerr << "CXX=" << CXX_COMPILER << '\n';
#endif
#ifdef CXX_COMPILER_VERSION
    std::cerr << "CXX version=" << CXX_COMPILER_VERSION << '\n';
#endif
  }
  if (cfg.info && cfg.path.empty()) return 0;  // --info alone

  int numQueryGroups = 0;
  int numQueryNames = 0;
  int numQueryCells = 0;
  int numQueryLayers = 0;
  int numQuerySteps = 0;
  int querySteps[2];
  if (cfg.step.end)
  {
    numQuerySteps = 2;
    querySteps[0] = cfg.step.start;
    querySteps[1] = *cfg.step.end;
  }
  bool useQuery = (numQueryGroups > 0) || (numQueryNames > 0) || (numQueryCells > 0) || (numQueryLayers > 0) || (numQuerySteps > 0);
  CdiQueryPtr query;
  if (useQuery)
  {
    query = cdi_query_create();
    // if (numQueryGroups) cdiQuerySetGroups(query, numQueryGroups, queryGroups);
    // if (numQueryNames) cdiQuerySetNames(query, numQueryNames, queryNames);
    // if (numQueryCells) cdiQuerySetCells(query, numQueryCells, queryCells);
    // if (numQueryLayers) cdiQuerySetLayers(query, numQueryLayers, queryLayers);
    if (numQuerySteps) cdiQuerySetSteps(query.get(), numQuerySteps, querySteps);
    // if (Debug) cdiQueryPrint(query);
  }

  int Move = 0;
  int Field = 0;
  int Variable = 0;
  int Debug = 0;
  int Vardis = 0;
  int Version = 0;
  int Info = 1;
  int Longinfo = 0;
  int Shortinfo = 0;

  auto fname1 = cfg.path.c_str();
  const char *fname2 = NULL;

  int isZarrFormat = check_tensorstore(&fname1);

  int streamID1 = -1;
  if (isZarrFormat) { streamID1 = streamOpenTensorstore(fname1); }
  else
  {
    streamID1 = useQuery ? streamOpenReadQuery(fname1, query.get()) : streamOpenRead(fname1);
  }
  if (streamID1 < 0) return handle_error(streamID1, "Open failed on %s", fname1);

  if (useQuery && query) cdiQueryPrintEntriesNotFound(query.get());

  // if (numWorkerIn > 0) streamDefNumWorker(streamID1, numWorkerIn);

  // if (preScan) fprintf(stdout, "numSteps=%d\n", streamInqNumSteps(streamID1));

  int vlistID1 = streamInqVlist(streamID1);

  // if (Longinfo) vlistPrint(vlistID1);

  int nvars = vlistNvars(vlistID1);
  int taxisID1 = vlistInqTaxis(vlistID1);
  int ntsteps = vlistNtsteps(vlistID1);

  // if (Debug) fprintf(stderr, "nvars   = %d\nntsteps = %d\n", nvars, ntsteps);

  int vlistID2 = -1;
  int taxisID2 = -1;
  if (fname2)
  {
    vlistID2 = vlistDuplicate(vlistID1);
    taxisID2 = taxisDuplicate(taxisID1);
    vlistDefTaxis(vlistID2, taxisID2);
  }

  char varname[CDI_MAX_NAME];
  size_t datasize = 0;
  int maxlev = 0;

  for (int varID = 0; varID < nvars; varID++)
  {
    int gridID = vlistInqVarGrid(vlistID1, varID);
    size_t gridsize = (size_t) gridInqSize(gridID);
    if (gridsize > datasize) datasize = gridsize;
    int zaxisID = vlistInqVarZaxis(vlistID1, varID);
    size_t nlev = (size_t) zaxisInqSize(zaxisID);
    if (nlev > maxlev) maxlev = nlev;
    if (fname2)
    {
      // if (DefaultDataType != CDI_UNDEFID) vlistDefVarDatatype(vlistID2, varID, DefaultDataType);
    }
  }

  /*
  if (fname2)
  {
    Info = 0;
    filetype = streamInqFiletype(streamID1);

    if (DefaultFileType != CDI_UNDEFID) filetype = DefaultFileType;

    streamID2 = streamOpenWrite(fname2, filetype);
    if (streamID2 < 0) return handle_error(streamID2, "Open failed on %s", fname2);

    int maxSteps = vlistNtsteps(vlistID1);
    if ((filetype == CDI_FILETYPE_NCZARR || filetype == CDI_FILETYPE_ZARR2 || filetype == CDI_FILETYPE_ZARR3) && maxSteps >= 0)
      streamDefMaxSteps(streamID2, maxSteps);
    if (numWorkerOut > 0) streamDefNumWorker(streamID2, numWorkerOut);

    if (DefaultByteorder != CDI_UNDEFID) streamDefByteorder(streamID2, DefaultByteorder);

    if (filterSpec[0]) streamDefFilter(streamID2, filterSpec);

    if (comptype != CDI_COMPRESS_NONE)
    {
      streamDefCompType(streamID2, comptype);
      streamDefCompLevel(streamID2, complevel);
    }

    streamDefVlist(streamID2, vlistID2);

    if (otableID == CDI_UNDEFID) otableID = itableID;
  }
  */

  if (vlistNumber(vlistID1) != CDI_REAL) datasize *= 2;
  // if (Variable) datasize *= maxlev;
  double *data = (double *) malloc(datasize * sizeof(double));

  // if (Debug) printf("nts = %d streamID1 = %d, streamID2 = %d\n", nts, streamID1, streamID2);

  /*
  if (Shortinfo)
  {
    Info = 0;
    print_short_info(streamID1, vlistID1, Vardis);
  }
  */
  int nrecs = 0;
  int tsID = 0;
  // if (Info || fname2 || NoInfo)
  while ((nrecs = streamInqTimestep(streamID1, tsID)) > 0)
  {
    /*
    if (fname2)
    {
      taxisCopyTimestep(taxisID2, taxisID1);
      streamDefTimestep(streamID2, tsID);
    }
    */
    CdiDateTime vdatetime = taxisInqVdatetime(taxisID1);

    if (Field)
    {
      for (int recID = 0; recID < nrecs; recID++)
      {
        int varID, levelID;
        SizeType nmiss;
        streamInqField(streamID1, &varID, &levelID);
        streamReadField(streamID1, data, &nmiss);

        int number = vlistInqVarNumber(vlistID1, varID);
        int gridID = vlistInqVarGrid(vlistID1, varID);
        int zaxisID = vlistInqVarZaxis(vlistID1, varID);
        int param = vlistInqVarParam(vlistID1, varID);

        // cdiParamToString(param, paramstr, sizeof(paramstr));

        // if (Vardis)
        vlistInqVarName(vlistID1, varID, varname);
        // else
        //  strcpy(varname, paramstr);

        // printf("varID=%d, param=%d, gridID=%d, zaxisID=%d levelID=%d\n", varID, param, gridID, zaxisID, levelID);

        SizeType gridsize = gridInqSize(gridID);
        double level = zaxisInqLevels(zaxisID, NULL) ? zaxisInqLevel(zaxisID, levelID) : levelID + 1;
        double missval = vlistInqVarMissval(vlistID1, varID);

        if (Info) printInfo(vdatetime, varname, level, (size_t) gridsize, number, (size_t) nmiss, missval, data, Vardis);

        /*
        if (fname2)
        {
          streamDefField(streamID2, varID, levelID);
          if (Move)
            streamCopyField(streamID2, streamID1);
          else
            streamWriteField(streamID2, data, nmiss);
        }
        */
      }
    }
    else if (Variable)
    {
      for (int varID = 0; varID < nvars; varID++)
      {
        if (vlistInqVarTimetype(vlistID1, varID) == TIME_CONSTANT && tsID > 0) continue;

        int number = vlistInqVarNumber(vlistID1, varID);
        int gridID = vlistInqVarGrid(vlistID1, varID);
        int zaxisID = vlistInqVarZaxis(vlistID1, varID);
        int param = vlistInqVarParam(vlistID1, varID);

        // cdiParamToString(param, paramstr, sizeof(paramstr));

        // if (Vardis)
        vlistInqVarName(vlistID1, varID, varname);
        // else
        //  strcpy(varname, paramstr);

        if (Debug) fprintf(stdout, "varID = %d param = %d gridID = %d zaxisID = %d\n", varID, param, gridID, zaxisID);

        size_t gridsize = (size_t) gridInqSize(gridID);
        double missval = vlistInqVarMissval(vlistID1, varID);

        SizeType nmiss;
        streamReadVar(streamID1, varID, data, &nmiss);

        int nlevs = zaxisInqSize(zaxisID);
        for (int levelID = 0; levelID < nlevs; levelID++)
        {
          double level = zaxisInqLevels(zaxisID, NULL) ? zaxisInqLevel(zaxisID, levelID) : levelID + 1;
          size_t offset = (size_t) levelID * gridsize;
          if (Info) printInfo(vdatetime, varname, level, gridsize, number, (size_t) nmiss, missval, data + offset, Vardis);
        }

        // if (fname2) streamWriteVar(streamID2, varID, data, nmiss);
      }
    }
    else
    {
      for (int varID = 0; varID < nvars; varID++)
      {
        if (vlistInqVarTimetype(vlistID1, varID) == TIME_CONSTANT && tsID > 0) continue;

        int number = vlistInqVarNumber(vlistID1, varID);
        int gridID = vlistInqVarGrid(vlistID1, varID);
        int zaxisID = vlistInqVarZaxis(vlistID1, varID);
        int param = vlistInqVarParam(vlistID1, varID);

        // cdiParamToString(param, paramstr, sizeof(paramstr));

        // if (Vardis)
        vlistInqVarName(vlistID1, varID, varname);
        // else
        //  strcpy(varname, paramstr);

        // if (Debug) fprintf(stdout, "varID = %d param = %d gridID = %d zaxisID = %d\n", varID, param, gridID, zaxisID);

        size_t gridsize = (size_t) gridInqSize(gridID);
        double missval = vlistInqVarMissval(vlistID1, varID);

        int nlevs = zaxisInqSize(zaxisID);
        for (int levelID = 0; levelID < nlevs; levelID++)
        {
          double level = zaxisInqLevels(zaxisID, NULL) ? zaxisInqLevel(zaxisID, levelID) : levelID + 1;
          SizeType nmiss;
          streamReadVarSlice(streamID1, varID, levelID, data, &nmiss);

          if (Info) printInfo(vdatetime, varname, level, gridsize, number, (size_t) nmiss, missval, data, Vardis);

          // if (fname2) streamWriteVarSlice(streamID2, varID, levelID, data, nmiss);
        }
      }
    }

    tsID++;
  }

  free(data);

  // fprintf(stderr, "%ld\n", (long) streamNvals(streamID1));

  if (fname2)
  {
    // streamClose(streamID2);
    vlistDestroy(vlistID2);
    taxisDestroy(taxisID2);
  }
  streamClose(streamID1);

  return 0;
}
