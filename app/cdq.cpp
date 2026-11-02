#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "printinfo.h"
#include "printinfo.hpp"
#include "field.hpp"

// #include "tsio.h"
#include "cdi.h"
#include "cdi_cpp.h"
#include "cdi_query.h"
#include "julian_date.h"

#include <iostream>
#include <optional>
#include <string>
#include <sstream>
#include <thread>
#include <vector>

namespace
{

// 1-basierter, inklusiver Bereich einer Achse; ein nicht gesetztes `end`
// bedeutet "bis zur Ausdehnung des Arrays".
struct indexRange
{
  std::int64_t start = 1;
  std::optional<std::int64_t> end;
};

// Konfiguration eines Datensatzes inklusive Array- und Hypercube-Auswahl.
struct cdqConfig
{
  int format = 0;       // 0 = auto-detect, 2 or 3 = force
  std::string kvstore;  // "" = auto-detect, "file", "s3" or "http"
  std::string path;
  std::vector<std::string> arrays;
  std::optional<std::string> endpoint;
  bool consolidated = false;                 // Metadaten aus `.zmetadata` (zarr2)
  std::optional<std::string> group;          // zarr3 Gruppe unterhalb `path`
  bool debug = false;                        // Diagnose auf stderr (in tsio)
  bool info = false;                         // nur TensorStore-Version ausgeben
  std::optional<int> data_copy_concurrency;  // parallele Chunk-(De)Kompression
  bool double_buffer = false;                // Double-Buffer-Prefetch
  bool overwrite = false;                    // --out-path vorher loeschen
  std::optional<std::string> out_path;       // Ausgabe-Datensatz (Schreibmodus)
  std::string out_format = "zarr3";          // Ausgabeformat: "zarr2", "zarr3" oder "n5"
  std::string out_dtype = "float32";         // Datenarrays (zarr3): "float64", "float32", "float16" oder "bfloat16"
  std::optional<std::string>
      compressor;  // --compressor: Ausgabe-Kompressor (Schreibmodus); ungesetzt = Quell-Kompression uebernehmen
  std::optional<std::int64_t> step_chunk;  // --step-chunk: Laenge der step-Ausgabe-Chunks (Schreibmodus)
  std::optional<std::int64_t> cell_chunk;  // --cell-chunk: Laenge der cell-Ausgabe-Chunks (letzte Dim, Schreibmodus)
  indexRange step;                         // Zeitachse
  indexRange layer;                        // Layer-Achse (rank >= 3)
  indexRange cell;                         // letzte Achse (Gitterpunkte)
};

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
parse_index_range(const std::string &opt, const std::string &value, indexRange &range)
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
parse_args(int argc, char **argv, cdqConfig &cfg)
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
      indexRange &range = (arg == "--step") ? cfg.step : (arg == "--layer" ? cfg.layer : cfg.cell);
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

static MinMaxMean
compute_stat(Field const &field)
{
  return (field.numMiss) ? field_min_max_mean_mv(field) : field_min_max_mean(field);
}

static void
field_print(Field const &field, CdiVar const &var, int levelID, CdiDateTime vdatetime)
{
  static int recNum = 0;
  size_t numMissFound = 0;
  auto numMiss = field.numMiss;

  if (!recNum)
  {
    fprintf(stdout,
            " Field :       Date     Time   Level Gridsize    Miss :     Minimum        Mean     Maximum : Parameter name\n");
    //       ----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+
  }

  char vdatestr[32], vtimestr[32];
  date2str(vdatetime.date, vdatestr, sizeof(vdatestr));
  time2str(vdatetime.time, vtimestr, sizeof(vtimestr));

  fprintf(stdout, "%6d :%s %s %7g ", ++recNum, vdatestr, vtimestr, var.layers[levelID]);
  fprintf(stdout, "%8zu ", field.size);
  fprintf(stdout, "%7zu :", field.numMiss);

  auto mmm = compute_stat(field);

  if (numMiss) { numMissFound = field.size - mmm.n; }

  fprintf(stdout, "%#12.5g%#12.5g%#12.5g", mmm.min, mmm.mean, mmm.max);

  fprintf(stdout, " : %-14s\n", var.name.c_str());

  if (numMissFound != numMiss && numMiss > 0) fprintf(stdout, "Found %zu of %zu missing values!\n", numMissFound, numMiss);
}

static int
print_time_info(int numSteps, int taxisID, StreamHandle const &stream)
{
  int numTimesteps = numSteps;
  TimeIncrement timeIncrement0;
  CdiDateTime vDateTimeFirst{};
  CdiDateTime vDateTimeLast{};

  std::fprintf(stdout, "   Time coordinate");
  std::fprintf(stdout, " :\n");

  if (taxisID != CDI_UNDEFID)
  {
    auto calendar = taxisInqCalendar(taxisID);

    bool computeTimeIncrement = true;
    int tsID = 0;
    while (true)
    {
      auto numFields = streamInqTimestep(stream, tsID);
      if (numFields == 0) break;

      auto vDateTime = taxisInqVdatetime(taxisID);

      if (tsID == 0) { vDateTimeFirst = vDateTime; }
      else if (computeTimeIncrement)
      {
        auto julianDate0 = julianDate_encode(calendar, vDateTimeLast);
        auto julianDate = julianDate_encode(calendar, vDateTime);
        auto jdelta = julianDate_to_seconds(julianDate_sub(julianDate, julianDate0));
        auto timeIncrement = get_time_increment(jdelta, vDateTimeLast.date, vDateTime.date);

        if (tsID == 1) { timeIncrement0 = timeIncrement; }
        else if (timeIncrement0 != timeIncrement)
        {
          timeIncrement0.period = 0;
          computeTimeIncrement = false;
        }
      }

      vDateTimeLast = vDateTime;

      tsID++;
    }

    numTimesteps = tsID;
  }

  const auto *taxisName = taxisNamePtr(taxisID);
  const auto *tname = taxisName ? taxisName : "time";

  std::fprintf(stdout, "%33s : ", "steps");

  if (numSteps == CDI_UNDEFID)
    std::fprintf(stdout, "unlimited (%d currently)\n", numTimesteps);
  else
    std::fprintf(stdout, "%d\n", numSteps);

  if (taxisID != CDI_UNDEFID)
  {
    std::fprintf(stdout, "%33s : ", tname);

    std::fprintf(stdout, "%s", datetime_to_string(vDateTimeFirst).c_str());
    if (numTimesteps > 1)
    {
      std::fprintf(stdout, " to %s", datetime_to_string(vDateTimeLast).c_str());
      if (timeIncrement0.period)
        std::fprintf(stdout, " by %ld %s%s", static_cast<long>(timeIncrement0.period), time_units_cstr(timeIncrement0.units),
                     (timeIncrement0.period != 1) ? "s" : "");
    }
    std::fprintf(stdout, "\n");

    auto taxisType = taxisInqType(taxisID);
    if (taxisType != TAXIS_ABSOLUTE)
    {
      std::fprintf(stdout, "%33s : ", "units");

      auto tunits = taxisInqTunit(taxisID);
      std::fprintf(stdout, "%s", tunit_to_cstr(tunits));

      auto rDateTime = taxisInqRdatetime(taxisID);
      std::fprintf(stdout, " since %s\n", datetime_to_string(rDateTime).c_str());

      auto calendar = taxisInqCalendar(taxisID);
      if (calendar != CDI_UNDEFID) std::fprintf(stdout, "%33s : %s\n", "calendar", calendar_to_cstr(calendar));

      if (taxisType == TAXIS_FORECAST)
      {
        auto fDateTime = taxisInqFdatetime(taxisID);
        std::fprintf(stdout, "%33s : %s\n", "forecastRefTime", datetime_to_string(fDateTime).c_str());
      }
    }

    if (taxisHasBounds(taxisID)) std::fprintf(stdout, "%33s : %s\n", "available", "bounds");
  }

  return numTimesteps;
}

const char *
datatype_to_cstr(int datatype)
{
  static char cstr[20]{};
  if (datatype > 0 && datatype <= 32) std::snprintf(cstr, sizeof(cstr), "P%d", datatype);

  // clang-format off
  if      (datatype == CDI_DATATYPE_PACK  ) return "P0";
  else if (datatype > 0 && datatype <= 32 ) return cstr;
  else if (datatype == CDI_DATATYPE_CPX32 ) return "C32";
  else if (datatype == CDI_DATATYPE_CPX64 ) return "C64";
  else if (datatype == CDI_DATATYPE_FLT32 ) return "F32";
  else if (datatype == CDI_DATATYPE_FLT64 ) return "F64";
  else if (datatype == CDI_DATATYPE_INT8  ) return "I8";
  else if (datatype == CDI_DATATYPE_INT16 ) return "I16";
  else if (datatype == CDI_DATATYPE_INT32 ) return "I32";
  else if (datatype == CDI_DATATYPE_UINT8 ) return "U8";
  else if (datatype == CDI_DATATYPE_UINT16) return "U16";
  else if (datatype == CDI_DATATYPE_UINT32) return "U32";
  else                                      return "";
  // clang-format on
}

const char *
get_steptype_name(int tsteptype)
{
  // clang-format off
  if      (tsteptype == TSTEP_INSTANT)  return "instant";
  else if (tsteptype == TSTEP_INSTANT2) return "instant";
  else if (tsteptype == TSTEP_INSTANT3) return "instant";
  else if (tsteptype == TSTEP_MIN)      return "min";
  else if (tsteptype == TSTEP_MAX)      return "max";
  else if (tsteptype == TSTEP_AVG)      return "avg";
  else if (tsteptype == TSTEP_ACCUM)    return "accum";
  else if (tsteptype == TSTEP_RANGE)    return "range";
  else if (tsteptype == TSTEP_DIFF)     return "diff";
  else if (tsteptype == TSTEP_SUM)      return "sum";
  else if (tsteptype == TSTEP_SD)       return "standard_deviation";
  else if (tsteptype == TSTEP_VAR)      return "variance";
  // clang-format on
  return "unknown";
}

[[nodiscard]] static constexpr const char *
memtype_to_cstr(MemType memType) noexcept
{
  return (memType == MemType::Double) ? "F64" : "F32";
}

[[nodiscard]] static constexpr std::string_view
limit_string_view(std::string_view name, size_t maxLen) noexcept
{
  if (maxLen == 0) return {};

  name = name.substr(0, maxLen - 1);
  if (name.size() > 10)
  {
    auto pos = name.find_first_of(" ,", 3);
    auto dotPos = name.find('.', 11);
    if (dotPos < pos) pos = dotPos;
    if (pos != std::string_view::npos) name = name.substr(0, pos);
  }

  return name;
}

void
print_vars_info(int vlistID, VarList const &varList)
{
  for (auto const &var : varList)
  {
    std::fprintf(stdout, "%6d : ", var.ID + 1);

    // institute info
    const auto *instptr = institutInqNamePtr(vlistInqVarInstitut(vlistID, var.ID));
    auto instName = limit_string_view(instptr ? instptr : "unknown", 32);
    std::fprintf(stdout, "%-8.*s ", static_cast<int>(instName.size()), instName.data());

    // source info
    const auto *modelptr = modelInqNamePtr(vlistInqVarModel(vlistID, var.ID));
    auto modelName = limit_string_view(modelptr ? modelptr : "unknown", 32);
    std::fprintf(stdout, "%-8.*s ", static_cast<int>(modelName.size()), modelName.data());

    std::fprintf(stdout, "%c ", var.isConstant ? 'c' : 'v');

    // tsteptype
    std::fprintf(stdout, "%-8s ", get_steptype_name(var.stepType));

    // layer info
    std::fprintf(stdout, "%6d ", var.numLayers);
    std::fprintf(stdout, "%3d ", vlistZaxisIndex(vlistID, var.zaxisID) + 1);

    // grid info
    std::fprintf(stdout, "%9zu ", var.numCells);
    std::fprintf(stdout, "%3d ", vlistGridIndex(vlistID, var.gridID) + 1);

    // datatype
    std::fprintf(stdout, " %-3s", datatype_to_cstr(var.dataType));
    int compType = vlistInqVarCompType(vlistID, var.ID);
    bool isCompressed = (compType != CDI_COMPRESS_NONE);
    std::fprintf(stdout, "%c ", isCompressed ? (int) comptype_to_name(compType)[0] : ' ');

    std::fprintf(stdout, "  %-3s ", memtype_to_cstr(var.memType));

    // parameter info
    std::fprintf(stdout, ": ");
    std::fprintf(stdout, "%-14s", var.name.c_str());
    std::fprintf(stdout, "\n");
  }
}

void
print_shortinfo(StreamHandle const &stream, int vlistID, VarList const &varList)
{
  std::fprintf(stdout, "   Var : Institut Source   T Steptype Levels Num    Points Num Dtype Mtype : ");
  std::fprintf(stdout, "Parameter name\n");

  int numFieldsConst = 0;
  int numFieldsVar = 0;
  for (auto const &var : varList)
  {
    if (var.isConstant) { numFieldsConst += var.numLayers; }
    else
    {
      numFieldsVar += var.numLayers;
    }
  }

  print_vars_info(vlistID, varList);

  std::fprintf(stdout, "   Grid coordinates");
  std::fprintf(stdout, " :\n");
  print_grid_info(vlistID);

  std::fprintf(stdout, "   Vertical coordinates");
  std::fprintf(stdout, " :\n");
  print_zaxis_info(vlistID);

  int taxisID = vlistInqTaxis(vlistID);
  int numSteps = vlistNtsteps(vlistID);
  int numTimesteps = numSteps;
  if (numSteps != 0) print_time_info(numSteps, taxisID, stream);

  std::fprintf(stdout, "   Summary");
  std::fprintf(stdout, " :\n");
  std::fprintf(stdout, "%33s : %zu\n", "number of variables", varList.size());
  if (numTimesteps) std::fprintf(stdout, "%33s : %d\n", "number of timesteps", numTimesteps);
  int numFields = numFieldsVar * numTimesteps + numFieldsConst;
  std::fprintf(stdout, "%33s : %d\n", "number of fields", numFields);
}

static bool
is_int16_type(int dataType)
{
  return (dataType == CDI_DATATYPE_UINT16 || dataType == CDI_DATATYPE_INT16);
}

static bool
is_int8_type(int dataType)
{
  return (dataType == CDI_DATATYPE_UINT8 || dataType == CDI_DATATYPE_INT8);
}

static bool
is_float_type(int dataType)
{
  return (dataType == CDI_DATATYPE_FLT32 || dataType == CDI_DATATYPE_CPX32);
}

static VarList
varlist_create(int vlistID)
{
  char buffer[CDI_MAX_NAME];
  auto numVars = vlistNvars(vlistID);
  VarList varList(numVars);
  for (int varID = 0; varID < numVars; ++varID)
  {
    auto &var = varList[varID];
    var.ID = varID;
    buffer[0] = 0;
    vlistInqVarName(vlistID, varID, buffer);
    var.name = buffer;
    buffer[0] = 0;
    vlistInqVarUnits(vlistID, varID, buffer);
    if (buffer[0]) var.units = buffer;
    var.gridID = vlistInqVarGrid(vlistID, varID);
    var.zaxisID = vlistInqVarZaxis(vlistID, varID);
    var.timeType = vlistInqVarTimetype(vlistID, varID);
    var.stepType = vlistInqVarTsteptype(vlistID, varID);
    var.gridType = gridInqType(var.gridID);
    var.numCells = gridInqSize(var.gridID);
    var.zaxisType = zaxisInqType(var.zaxisID);
    var.numLayers = zaxisInqSize(var.zaxisID);
    var.dataType = vlistInqVarDatatype(vlistID, varID);
    var.missval = vlistInqVarMissval(vlistID, varID);
    var.code = vlistInqVarCode(vlistID, varID);
    var.param = vlistInqVarParam(vlistID, varID);
    var.nwpv = vlistInqVarNumber(vlistID, varID);
    var.isConstant = (var.timeType == TIME_CONSTANT);
    double addOffset = 0.0, scaleFactor = 1.0;
    auto haveAddOffset = (cdiInqKeyFloat(vlistID, varID, CDI_KEY_ADDOFFSET, &addOffset) == CDI_NOERR);
    auto haveScaleFactor = (cdiInqKeyFloat(vlistID, varID, CDI_KEY_SCALEFACTOR, &scaleFactor) == CDI_NOERR);
    auto isPacked = (haveAddOffset || haveScaleFactor);

    auto defaultMemType{ MemType::Native };
    if (defaultMemType == MemType::Native)
    {
      auto useFloatType = (var.dataType == CDI_UNDEFID) || is_float_type(var.dataType) || is_int8_type(var.dataType)
                          || (is_int16_type(var.dataType) && isPacked);
      var.memType = useFloatType ? MemType::Float : MemType::Double;
    }
    else
    {
      var.memType = defaultMemType;
    }

    var.layers.resize(var.numLayers);
    if (zaxisInqLevels(var.zaxisID, nullptr) == var.numLayers) { zaxisInqLevels(var.zaxisID, var.layers.data()); }
    else
    {
      if (var.numLayers == 1 && var.zaxisType == ZAXIS_SURFACE) { var.layers[0] = 0.0; }
      else
        for (int i = 0; i < var.numLayers; ++i) { var.layers[i] = i + 1.0; }
    }
  }
  return varList;
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

static bool
strip_ts_prefix(std::string_view &s)
{
  constexpr std::string_view prefix = "ts:";
  if (s.size() >= prefix.size() && s.substr(0, prefix.size()) == prefix)
  {
    s.remove_prefix(prefix.size());
    return true;
  }
  return false;
}

int
main(int argc, char **argv)
{
  cdqConfig cfg;
  if (!parse_args(argc, argv, cfg))
  {
    print_usage(argv[0]);
    return 1;
  }

  if (cfg.info)
  {
#ifdef HAVE_TENSORSTORE
    //  std::cerr << "TensorStore " << tsVersion() << '\n';
#else
    //  std::cerr << "TensorStore support: disabled (built with -DWITH_TENSORSTORE=OFF)\n";
#endif
    unsigned int tc = std::thread::hardware_concurrency();
    if (tc == 0) { tc = 1; }
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
    if (cfg.path.empty()) return 0;  // --info alone
  }

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
  CdiQueryHandle query;
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

  enum struct AccessMode
  {
    Sequential,
    Direct,
  };

  auto accessMode{ AccessMode::Direct };
  int Move = 0;
  int Debug = 0;
  int Version = 0;
  int Info = 1;
  int Longinfo = 0;

  auto fname1 = cfg.path.c_str();
  const char *fname2 = NULL;
  if (cfg.out_path) { fname2 = (*cfg.out_path).c_str(); }
  auto printShortinfo = !cfg.info && !fname2;
  auto printInfo = cfg.info && !fname2;

  std::string_view fnameView = fname1;
  bool isZarr = strip_ts_prefix(fnameView);

  StreamHandle stream1;
  if (isZarr) { stream1.id = streamOpenTensorstore(fname1); }
  else
  {
    stream1.id = useQuery ? streamOpenReadQuery(fname1, query.get()) : streamOpenRead(fname1);
  }
  if (stream1 < 0) return handle_error(stream1, "Open failed on %s", fname1);

  if (useQuery && query) cdiQueryPrintEntriesNotFound(query.get());

  // if (numWorkerIn > 0) streamDefNumWorker(stream1, numWorkerIn);

  // if (preScan) fprintf(stdout, "numSteps=%d\n", streamInqNumSteps(stream1));

  int vlistID1 = streamInqVlist(stream1);
  auto varList = varlist_create(vlistID1);

  // if (Longinfo) vlistPrint(vlistID1);

  int numVars = varList.size();
  int taxisID1 = vlistInqTaxis(vlistID1);

  if (printShortinfo && !printInfo)
  {
    print_shortinfo(stream1, vlistID1, varList);
    return 0;
  }

  StreamHandle stream2;
  VlistHandle vlist2;
  TaxisHandle taxis2;
  if (fname2)
  {
    vlist2.id = vlistDuplicate(vlistID1);
    taxis2.id = taxisDuplicate(taxisID1);
    vlistDefTaxis(vlist2, taxis2);
  }

  for (int varID = 0; varID < numVars; varID++)
  {
    if (fname2)
    {
      // if (DefaultDataType != CDI_UNDEFID) vlistDefVarDatatype(vlist2, varID, DefaultDataType);
      vlistDefVarDatatype(vlist2, varID, CDI_DATATYPE_FLT32);
    }
  }

  if (fname2)
  {
    Info = 0;
    auto filetype = streamInqFiletype(stream1);
    filetype = CDI_FILETYPE_NC4;
    // if (DefaultFileType != CDI_UNDEFID) filetype = DefaultFileType;

    stream2.id = streamOpenWrite(fname2, filetype);
    if (stream2 < 0) return handle_error(stream2, "Open failed on %s", fname2);

    int maxSteps = vlistNtsteps(vlistID1);
    if ((filetype == CDI_FILETYPE_NCZARR || filetype == CDI_FILETYPE_ZARR2 || filetype == CDI_FILETYPE_ZARR3) && maxSteps >= 0)
      streamDefMaxSteps(stream2, maxSteps);
    // if (numWorkerOut > 0) streamDefNumWorker(stream2, numWorkerOut);

    // if (DefaultByteorder != CDI_UNDEFID) streamDefByteorder(stream2, DefaultByteorder);

    // if (filterSpec[0]) streamDefFilter(stream2, filterSpec);

    // if (comptype != CDI_COMPRESS_NONE)
    //{
    //   streamDefCompType(stream2, comptype);
    //   streamDefCompLevel(stream2, complevel);
    // }

    streamDefVlist(stream2, vlist2);

    // if (otableID == CDI_UNDEFID) otableID = itableID;
  }

  Field field;

  int numFields = 0;
  int tsID = 0;
  // if (Info || fname2 || NoInfo)
  while ((numFields = streamInqTimestep(stream1, tsID)) > 0)
  {
    if (fname2)
    {
      taxisCopyTimestep(taxis2, taxisID1);
      streamDefTimestep(stream2, tsID);
    }

    CdiDateTime vdatetime = taxisInqVdatetime(taxisID1);

    if (accessMode == AccessMode::Sequential)
    {
      for (int fieldID = 0; fieldID < numFields; fieldID++)
      {
        int varID, levelID;
        streamInqField(stream1, &varID, &levelID);
        auto const &var = varList[varID];
        field.init(var);
        stream_read_field(stream1, field);

        if (printInfo) field_print(field, var, levelID, vdatetime);
        if (fname2)
        {
          streamDefField(stream2, varID, levelID);
          if (Move) { streamCopyField(stream2, stream1); }
          else
          {
            stream_write_field(stream2, field);
          }
        }
      }
    }
    else
    {
      for (int varID = 0; varID < numVars; varID++)
      {
        auto const &var = varList[varID];
        if (var.timeType == TIME_CONSTANT && tsID > 0) continue;

        field.init(var);
        for (int levelID = 0; levelID < var.numLayers; levelID++)
        {
          stream_read_var_slice(stream1, varID, levelID, field);
          if (Info) field_print(field, var, levelID, vdatetime);
          if (fname2) stream_write_var_slice(stream2, varID, levelID, field);
        }
      }
    }

    ++tsID;
  }

  // fprintf(stderr, "%ld\n", (long) streamNvals(stream1));

  return 0;
}
