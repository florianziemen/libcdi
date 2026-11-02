// tsq.cpp - Kommandozeile, Tabellen und Statistik.
//
// Diese Datei benutzt weder TensorStore noch JSON: saemtliche Zarr-I/O laeuft
// ueber tsio.h/tsio.cpp (ohne TensorStore: tsio_stub.cpp).  Der Datensatz wird
// einmal sequenziell durchlaufen; tsReadField liefert jedes Feld genau einmal.
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "tsio.h"

#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <sys/resource.h>

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
      std::cerr << "Unknown argument: " << arg << "\n";
      return false;
    }
  }

  if (cfg.path.empty() && !cfg.info)
  {
    std::cerr << "Missing required argument(s).\n";
    return false;
  }
  return true;
}

std::string
fmt_dims(const std::vector<std::int64_t> &dims)
{
  std::ostringstream ss;
  ss << '[';
  for (std::size_t d = 0; d < dims.size(); ++d)
  {
    if (d) ss << ", ";
    ss << dims[d];
  }
  ss << ']';
  return ss.str();
}

// "1:416, 1:7, 1:768" - ausgewaehlte 1-basierte Indizes pro Achse.
std::string
fmt_indices(const tsArray &a)
{
  std::ostringstream ss;
  ss << '[';
  for (std::size_t d = 0; d < a.bstart.size(); ++d)
  {
    if (d) ss << ", ";
    ss << a.bstart[d] + 1 << ':' << a.bstart[d] + a.bcount[d];
  }
  ss << ']';
  return ss.str();
}

// "=== Selected arrays ===": Name, Format, Kvstore, Form, Chunks, Indizes und
// im Schreibmodus zusaetzlich die Ausgabe-Chunkform.
void
print_selected_arrays(const tsMetadata &md, bool with_out_chunks)
{
  std::cout << "=== Selected arrays ===\n";
  std::cout << "#  name  format  kvstore  dtype  compressor  shape  chunks  indices";
  if (with_out_chunks) std::cout << "  out_chunks";
  std::cout << "\n";
  std::size_t idx = 1;
  for (const auto &a : md.arrayList)
  {
    std::cout << idx++ << "  " << a.name << "  zarr" << a.format << "  " << a.kvstore << "  " << a.dataType << "  " << a.compressor
              << "  " << fmt_dims(a.shape) << "  " << fmt_dims(a.chunks) << "  " << fmt_indices(a);
    if (with_out_chunks) std::cout << "  " << fmt_dims(a.out_chunks);
    std::cout << '\n';
  }
}

// "=== Coordinate arrays ===": jede von den Datenarrays benutzte Achse, die
// selbst ein Array ist, und der `grid_mapping`-Wert (z. B. "crs").  Die Laenge
// einer grid_mapping-Koordinate ist die Gitter-Achse ("cell") der Datenarrays.
void
print_coordinate_arrays(const tsMetadata &md)
{
  std::cout << "=== Coordinate arrays ===\n";
  std::cout << "#  name  shape\n";
  std::set<std::string> coord_arrays;
  for (const auto &c : md.coordList) coord_arrays.insert(c.name);

  std::map<std::string, std::int64_t> grid_len;
  for (const auto &a : md.arrayList)
  {
    const auto gm = a.attributes.find("grid_mapping");
    if (gm == a.attributes.end()) continue;
    if (grid_len.count(gm->second)) continue;
    for (std::size_t di = 0; di < a.dim_names.size() && di < a.shape.size(); ++di)
    {
      if (coord_arrays.count(a.dim_names[di])) continue;  // Achse mit gleichnamiger Koordinate
      grid_len[gm->second] = a.shape[di];                 // Gitter-Achse dieser grid_mapping
      break;
    }
  }

  std::size_t idx = 1;
  for (const auto &c : md.coordList)
  {
    std::ostringstream ss;
    ss << '[';
    const auto glen = grid_len.find(c.name);
    if (glen != grid_len.end())
    {
      ss << glen->second;  // Gitter-Laenge statt [1]
    }
    else
    {
      for (std::size_t d = 0; d < c.shape.size(); ++d)
      {
        if (d) ss << ", ";
        ss << c.shape[d];
      }
    }
    ss << ']';
    std::cout << idx++ << "  " << c.name << "  " << ss.str();

    std::string details;
    const auto add_attr = [&](const char *key, const char *label)
    {
      const auto it = c.attributes.find(key);
      if (it == c.attributes.end()) return;
      if (!details.empty()) details += "  ";
      details += label;
      details += "=";
      details += it->second;
    };
    add_attr("grid_mapping_name", "grid_mapping");
    add_attr("refinement_level", "refinement_level");
    add_attr("indexing_scheme", "indexing_scheme");
    if (!details.empty()) std::cout << "  " << details;
    std::cout << '\n';
  }
}

// "=== Dataset overview ===" bzw. die Gruppenliste der konsolidierten Wurzel.
bool
print_overview(const tsConfig &cfg, const tsOverview &ov)
{
  if (!ov.groups.empty())
  {
    std::cout << "=== Groups in " << cfg.path << " ===\n";
    std::cout << "#  Group\n";
    std::size_t idx = 1;
    for (const auto &g : ov.groups) std::cout << std::setw(2) << idx++ << "  " << g << "\n";
    return true;
  }
  if (ov.arrays.empty()) return false;

  // Ohne --name werden nur die Datenarrays gelistet: ein Array, dessen Name
  // als Dimensionsname oder als `grid_mapping`-Ziel eines Arrays vorkommt, ist
  // eine Koordinate und wird aussortiert.
  std::set<std::string> coord_names;
  for (const auto &row : ov.arrays)
    for (const auto &d : row.dim_names) coord_names.insert(d);
  for (const auto &row : ov.arrays)
    if (!row.grid_mapping.empty()) coord_names.insert(row.grid_mapping);
  std::vector<const tsInfo *> data_arrays;
  for (const auto &row : ov.arrays)
    if (!coord_names.count(row.name)) data_arrays.push_back(&row);
  if (data_arrays.empty()) return false;

  std::cout << "=== Dataset overview ===\n";
  std::cout << "#  Name  [Units]  DataType  Shape  Chunks  Compressor\n";
  std::size_t idx = 1;
  for (const auto *row : data_arrays)
  {
    std::cout << std::setw(2) << idx++ << "  " << row->name << "  [" << row->units << "]  " << row->data_type << "  " << row->shape
              << "  " << row->chunks << "  " << row->compressor << "\n";
  }
  return ov.ok;
}

// Eine Zeile der mittleren Tabelle; `total` ist die Zahl der Werte eines
// Layers, `valid` die Zahl der Werte ungleich NaN.
void
print_row(std::int64_t step, std::int64_t layer, std::int64_t total, std::size_t valid, double mn, double mean, double mx,
          const std::string &name)
{
  const std::int64_t missing = total - static_cast<std::int64_t>(valid);
  if (valid == 0)
  {
    std::cout << step << "  " << layer << "  " << total << "  " << missing << "  NaN  NaN  NaN  " << name << '\n';
    return;
  }
  char bmin[64], bmean[64], bmax[64];
  std::snprintf(bmin, sizeof bmin, "%12.5g", mn);
  std::snprintf(bmean, sizeof bmean, "%12.5g", mean);
  std::snprintf(bmax, sizeof bmax, "%12.5g", mx);
  std::cout << step << "  " << layer << "  " << total << "  " << missing << "  " << bmin << "  " << bmean << "  " << bmax << "  "
            << name << '\n';
}

// Prints wall-clock time and peak RSS on exit (stderr, so stdout stays
// byte-identical between the C++ and Python tools).
struct RunStats
{
  std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
  ~RunStats()
  {
    const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    std::cerr << std::fixed << std::setprecision(3);
    std::cerr << "# time_seconds: " << secs;
    struct rusage ru;
    if (getrusage(RUSAGE_SELF, &ru) == 0) { std::cerr << "  peak_rss_bytes: " << static_cast<long long>(ru.ru_maxrss) << '\n'; }
  }
};

}  // namespace

int
main(int argc, char **argv)
{
  RunStats stats;
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
#ifdef CXX_COMPILER
    std::cerr << "CXX=" << CXX_COMPILER << '\n';
#endif
#ifdef CXX_COMPILER_VERSION
    std::cerr << "CXX version=" << CXX_COMPILER_VERSION << '\n';
#endif
  }
  if (cfg.info && cfg.path.empty()) return 0;  // --info alone

#ifndef HAVE_TENSORSTORE
  std::cerr << "This binary was built without TensorStore (WITH_TENSORSTORE=OFF) and cannot read Zarr data. "
            << "Reconfigure with -DWITH_TENSORSTORE=ON.\n";
  return 2;
#else
  // Ohne --name werden alle Arrays ausgewaehlt; die Namen der Koordinaten-
  // arrays werden aussortiert, weil sie automatisch mit den Datenarrays
  // ausgewaehlt werden.  Eine Wurzel mit nur Gruppen wird wie bisher gelistet.
  if (cfg.arrays.empty())
  {
    tsOverview ov;
    ov.ok = tsDatasetOverview(cfg, ov);
    if (!ov.groups.empty())
    {
      print_overview(cfg, ov);
      return 0;
    }
    std::set<std::string> coord_names;
    for (const auto &row : ov.arrays)
      for (const auto &d : row.dim_names) coord_names.insert(d);
    for (const auto &row : ov.arrays)
      if (!row.grid_mapping.empty()) coord_names.insert(row.grid_mapping);
    cfg.arrays.clear();
    for (const auto &row : ov.arrays)
      if (!coord_names.count(row.name)) cfg.arrays.push_back(row.name);
    if (cfg.arrays.empty()) return print_overview(cfg, ov) ? 0 : 1;
  }

  // --info gibt nur die beiden Kopf-Tabellen (Selected/Coordinate arrays) aus
  // und liest keine Messwerte; ein etwaiger Schreibmodus wird dafuer ignoriert.
  if (cfg.info)
  {
    cfg.out_path.reset();
    cfg.overwrite = false;
  }

  // Schreibmodus-Vorbereitung: --overwrite ohne --out-path ist ein Fehler; ein
  // vorhandener --out-path wird nur mit --overwrite ersetzt (vorher geloescht),
  // und darf nicht der Quelldatensatz selbst sein.
  if (cfg.overwrite && !cfg.out_path)
  {
    std::cerr << "--overwrite requires --out-path\n";
    return 1;
  }
  if (cfg.out_path)
  {
    const std::filesystem::path src = std::filesystem::weakly_canonical(cfg.path);
    const std::filesystem::path dst = std::filesystem::weakly_canonical(*cfg.out_path);
    if (!src.empty() && src == dst)
    {
      std::cerr << "--out-path must not be the source dataset itself\n";
      return 1;
    }
    if (std::filesystem::exists(dst))
    {
      if (!cfg.overwrite)
      {
        std::cerr << "output directory " << *cfg.out_path << " already exists (use --overwrite to replace it)\n";
        return 1;
      }
      std::error_code ec;
      std::filesystem::remove_all(*cfg.out_path, ec);
      if (ec)
      {
        std::cerr << "failed to remove " << *cfg.out_path << ": " << ec.message() << "\n";
        return 1;
      }
    }
  }

  tsHandle *input = tsOpenFileRead(cfg.path, cfg);
  if (!input) return 1;
  tsMetadata &md = tsGetReadMetadata(input);

  // Datenarrays vs. Koordinatenarrays: die Dimensionsnamen eines Datenarrays
  // verweisen auf Koordinaten; ein Array mit genau diesem Namen ist eine
  // Koordinate und kann nicht mit `--name` gewaehlt werden.
  std::set<std::string> used_dims;
  for (const auto &a : md.arrayList)
    for (const auto &d : a.dim_names) used_dims.insert(d);
  for (const auto &a : md.arrayList)
  {
    if (used_dims.count(a.name))
    {
      std::cerr << "'" << a.name << "' is a coordinate array and cannot be selected with --name.\n";
      tsCloseFile(input);
      return 1;
    }
  }

  // Die Auswahl wird Array fuer Array aufgeloest, damit die Tabelle Zeile fuer
  // Zeile entsteht und ein Startindex ausserhalb der Ausdehnung sofort abricht.
  std::cout << "=== Selected arrays ===\n";
  std::cout << "#  name  format  kvstore  dtype  compressor  shape  chunks  indices";
  if (cfg.out_path) std::cout << "  out_chunks";
  std::cout << "\n";
  {
    std::size_t idx = 1;
    for (std::size_t i = 0; i < md.arrayList.size(); ++i)
    {
      tsArray &a = md.arrayList[i];
      if (!tsResolveArray(input, i))
      {
        tsCloseFile(input);
        return 1;
      }
      std::cout << idx++ << "  " << a.name << "  zarr" << a.format << "  " << a.kvstore << "  " << a.dataType << "  "
                << a.compressor << "  " << fmt_dims(a.shape) << "  " << fmt_dims(a.chunks) << "  " << fmt_indices(a);
      if (cfg.out_path) std::cout << "  " << fmt_dims(a.out_chunks);
      std::cout << '\n';
    }
  }
  print_coordinate_arrays(md);

  if (cfg.info)
  {
    tsCloseFile(input);
    return 0;
  }

  if (!cfg.out_path) { std::cout << "# timestep  layer  count  missing  min  mean  max  array\n"; }

  // Die Datenarrays dtype-neutral oeffnen (Lesepuffer arbeiten auf float32);
  // die Ausgabe-Stores erben daraus Attribute und Kompression.
  if (!tsOpenFields(input))
  {
    tsCloseFile(input);
    return 1;
  }

  tsHandle *output = nullptr;
  if (cfg.out_path)
  {
    output = tsOpenFileWrite(*cfg.out_path, cfg);
    if (!output)
    {
      tsCloseFile(input);
      return 1;
    }
    // Zuerst die Koordinaten und Datenarray-Metadaten lesen, dann das
    // Ausgabe-Config anwenden und schliesslich die Ausgabestores schreiben
    // (jeweils mit Attributen und uebernommener Kompression).
    std::vector<tsCoordinate> coords;
    tsOutputMetadata meta;
    if (!tsReadCoordinates(input, coords) || !tsGetMetadata(input, meta))
    {
      tsCloseFile(input);
      tsCloseFile(output);
      return 1;
    }
    if (!tsApplyCoordinateConfig(output->cfg, coords))
    {
      tsCloseFile(input);
      tsCloseFile(output);
      return 1;
    }
    tsApplyMetadataConfig(output->cfg, meta);
    if (!tsWriteCoordinates(output, coords) || !tsSetMetadata(output, meta))
    {
      tsCloseFile(input);
      tsCloseFile(output);
      return 1;
    }
  }

  // Ein sequentieller Durchlauf: aeussere Schleife ueber die Zeitschritte,
  // innere Schleife ueber die (konstant viele) Felder je Schritt.  Zeitdimension
  // und Feldzahl sind fuer alle Arrays gleich; jedes Feld wird genau einmal
  // gelesen, im Schreibmodus direkt in den Ausgabestore geschrieben.
  const std::size_t numFields = tsGetNumFields(input);
  tsField field;
  std::vector<float> data;
  std::int64_t step;
  while ((step = tsGetNextStep(input)) >= 0)
  {
    if (output) tsSetNextStep(output);
    for (std::size_t f = 0; f < numFields; ++f)
    {
      if (!tsGetField(input, field)) break;
      const tsArray &a = md.arrayList[field.array];
      data.resize(static_cast<std::size_t>(a.num_cells));
      if (!tsReadField(input, data.data(), data.size()))
      {
        tsCloseFile(input);
        tsCloseFile(output);
        return 1;
      }
      if (output)
      {
        if (!tsSetField(output, field) || !tsWriteField(output, data.data(), data.size()))
        {
          tsCloseFile(input);
          tsCloseFile(output);
          return 1;
        }
        continue;
      }
      double sum = 0.0;
      std::size_t valid = 0;
      double mn = std::numeric_limits<double>::infinity();
      double mx = -std::numeric_limits<double>::infinity();
      for (const float value : data)
      {
        const double v = static_cast<double>(value);
        if (std::isnan(v)) continue;
        sum += v;
        if (v < mn) mn = v;
        if (v > mx) mx = v;
        ++valid;
      }
      print_row(step + 1, a.bstart.size() >= 3 ? field.layer + 1 : 0, a.num_cells, valid, mn, sum / static_cast<double>(valid), mx,
                a.name);
    }
  }

  tsCloseFile(input);
  tsCloseFile(output);
  return 0;
#endif
}
