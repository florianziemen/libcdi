#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

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

  if (useQuery && query) cdiQueryPrintEntriesNotFound(query.get());

  return 0;
}
