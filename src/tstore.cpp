#include "tstore.h"
#include "cdi.h"
#include "cdi_int.h"
#include "cdi_query.h"
#include "cdf_util.h"
#include "tsio.h"

#include <cmath>  // nanf()
#include <array>
#include <filesystem>
#include <iostream>
#include <set>
#include <sstream>
#include <string>

static std::array<tsHandle *, 32> tsHandleIDs{ nullptr };

static int
get_next_fileID(tsHandle *handle)
{
  if (!handle) return -1;

  for (size_t i = 0; i < tsHandleIDs.size(); ++i)
  {
    if (tsHandleIDs[i] == nullptr)
    {
      tsHandleIDs[i] = handle;
      return static_cast<int>(i);
    }
  }

  return -1;
}

static tsHandle *
get_tsHandle(int fileID)
{
  if (fileID < 0 || fileID >= (int) tsHandleIDs.size()) fprintf(stderr, "%s: fileID=%d undefined\n", __func__, fileID);
  tsHandle *handle = tsHandleIDs[fileID];
  if (handle == nullptr) fprintf(stderr, "%s: Undefined handle for fileID=%d\n", __func__, fileID);
  return handle;
}

const char *
ts_lib_version(void)
{ return tsVersion(); }

static std::string
effective_path(std::string const &path)
{
  if (path.starts_with("https://") || path.ends_with(".zip")) return path;

  std::error_code ec;
  const auto canon = std::filesystem::canonical(path, ec);
  return ec ? path : canon.string();
}

int
ts_open(const char *fileName, const char *mode, int *fileType)
{
  int fileID = -1;
  int fmode = tolower(*mode);
  switch (fmode)
  {
    case 'r':
    {
      tsConfig cfg;
      cfg.path = effective_path(fileName);
      if (CDI_Data_Copy_Concurrency > 0) { cfg.data_copy_concurrency = CDI_Data_Copy_Concurrency; }
      tsHandle *handle = tsOpenFileRead(cfg.path, cfg);
      if (!handle) return -1;
      if (handle->cfg.format == 2) { *fileType = CDI_FILETYPE_TSZARR2; }
      else if (handle->cfg.format == 3) { *fileType = CDI_FILETYPE_TSZARR3; }
      fileID = get_next_fileID(handle);
      break;
    }
    case 'w':
      tsConfig cfg;
      fileID = get_next_fileID(tsOpenFileWrite(fileName, cfg));
      break;
  }

  // printf("%s: %s fileID=%d\n", __func__, fileName, fileID);
  if (fileID < 0) fprintf(stderr, "%s: fileID failed for %s!\n", __func__, fileName);
  return fileID;
}

void
ts_close(int fileID)
{ tsCloseFile(get_tsHandle(fileID)); }

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
static std::string
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

static void
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
      details += '=';
      details += it->second;
    };
    add_attr("grid_mapping_name", "grid_mapping");
    add_attr("refinement_level", "refinement_level");
    add_attr("indexing_scheme", "indexing_scheme");
    if (!details.empty()) std::cout << "  " << details;
    std::cout << '\n';
  }
}

static void
ts_read_timesteps(size_t numTimesteps, stream_t *streamP, taxis_t *taxis0, const char *timeName)
{
  streamP->curTsID = 0;
  streamP->rtsteps = 1;

  if (numTimesteps == 0)
  {
    cdi_create_timesteps(numTimesteps, streamP);
    cdf_create_records(streamP, 0);
  }
  else
  {
    int fileID = streamP->groupID;
    int nctimevarid = streamP->basetime.ncvarid;
    int nctimeboundsid = streamP->basetime.ncvarboundsid;
    bool hasTimesteps = (nctimevarid != CDI_UNDEFID /*&& streamP->basetime.hasUnits*/);
    hasTimesteps = true;

    int *ncStepIndices = (int *) malloc(numTimesteps * sizeof(int));
    for (size_t tsID = 0; tsID < numTimesteps; ++tsID) { ncStepIndices[tsID] = (int) tsID; }

    CdiDateTime *vDateTimeList = NULL;

    // if (hasTimesteps)
    {
      vDateTimeList = (CdiDateTime *) malloc(numTimesteps * sizeof(CdiDateTime));

      if (1 || streamP->basetime.hasUnits)
      {
        auto *timevarBuffer = (double *) malloc(numTimesteps * sizeof(double));
        // cdf_get_var_double(fileID, nctimevarid, timevarBuffer);
        auto *handle = get_tsHandle(streamP->fileID);
        ts_read_array(handle, timeName, timevarBuffer);
        for (size_t i = 0; i < numTimesteps; ++i)
        {
          // vDateTimeList[i] = cdi_decode_timeval(get_timevalue(fileID, nctimevarid, i, timevarBuffer), taxis0);
          vDateTimeList[i] = cdi_decode_timeval(timevarBuffer[i], taxis0);
        }
        free(timevarBuffer);
      }
      else
      {
        hasTimesteps = false;
      }
    }

    // process step query information if available
    process_time_query(streamP->query, numTimesteps, vDateTimeList, ncStepIndices);

    size_t numSteps = 0;
    for (size_t tsID = 0; tsID < numTimesteps; ++tsID) { numSteps += (ncStepIndices[tsID] >= 0); }

    if (numTimesteps > 0 && numSteps == 0) Error("No timesteps selected!");

    cdi_create_timesteps(numSteps, streamP);

    for (size_t tsID = 0, stepID = 0; tsID < numTimesteps; ++tsID)
    {
      if (ncStepIndices[tsID] >= 0)
      {
        streamP->tsteps[stepID].ncStepIndex = ncStepIndices[tsID];
        cdf_create_records(streamP, stepID);

        taxis_t *taxis = &streamP->tsteps[stepID].taxis;
        ptaxisCopy(taxis, taxis0);
        if (hasTimesteps) { taxis->vDateTime = vDateTimeList[tsID]; }

        stepID++;
      }
    }

    if (ncStepIndices) free(ncStepIndices);
    if (vDateTimeList) free(vDateTimeList);

    /*
    if (hasTimesteps)
    {
      if (nctimeboundsid != CDI_UNDEFID)
      {
        enum
        {
          numBnds = 2,
          tbNdims = 2
        };

        for (size_t tsID = 0; tsID < numSteps; ++tsID)
        {
          size_t ncStepIndex = (size_t) streamP->tsteps[tsID].ncStepIndex;
          taxis_t *taxis = &streamP->tsteps[tsID].taxis;
          size_t start[tbNdims] = { ncStepIndex, 0 };
          size_t count[tbNdims] = { 1, numBnds };
          double timeBnds[numBnds];
          cdf_get_vara_double(fileID, nctimeboundsid, start, count, timeBnds);
          for (size_t i = 0; i < numBnds; ++i)
            if (timeBnds[i] >= NC_FILL_DOUBLE || timeBnds[i] < -NC_FILL_DOUBLE) { timeBnds[i] = 0.0; }

          taxis->vDateTime_lb = cdi_decode_timeval(timeBnds[0], taxis);
          taxis->vDateTime_ub = cdi_decode_timeval(timeBnds[1], taxis);
        }
      }

      int leadtimeid = streamP->basetime.leadtimeid;
      if (leadtimeid != CDI_UNDEFID)
      {
        for (size_t tsID = 0; tsID < numSteps; ++tsID)
        {
          size_t ncStepIndex = (size_t) streamP->tsteps[tsID].ncStepIndex;
          taxis_t *taxis = &streamP->tsteps[tsID].taxis;
          cdi_set_forecast_period(get_timevalue(fileID, leadtimeid, ncStepIndex, NULL), taxis);
        }
      }
    }
    */
  }
}

#include <string>
#include <charconv>
#include <system_error>
#include <optional>

static int
dtype_str_to_id(std::string const &s)
{
  if (s == "float64") return CDI_DATATYPE_FLT64;
  return CDI_DATATYPE_FLT32;
}

static std::optional<double>
string_to_double(const std::string &str)
{
  double value{};
  auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), value);
  if (ec != std::errc{} || ptr != str.data() + str.size()) return std::nullopt;
  return value;
}

static std::string
find_time_dimname(tsMetadata const &md)
{
  for (auto const &c : md.coordList)
  {
    if (c.shape.size() == 1)
    {
      auto const &attributes = c.attributes;
      if (attributes.contains("units"))
      {
        auto units = attributes.find("units")->second;
        if (is_timeaxis_units(units.c_str())) return c.name;
      }
    }
  }

  return {};
}

int
ts_inq_contents(stream_t *streamP)
{
  auto *handle = get_tsHandle(streamP->fileID);
  auto &cfg = handle->cfg;

  auto *query = streamP->query;

  // process group query information if available
  if (query && cdiQueryNumGroups(query) > 0)
  {
    const char *groupName = cdiQueryGetGroup(query, 0);
    if (groupName) { cfg.group = groupName; }
  }

  if (cfg.arrays.empty())
  {
    tsOverview ov;
    ov.ok = tsDatasetOverview(cfg, ov);
    if (!ov.ok) return -1;
    if (!ov.groups.empty())
    {
      fprintf(stderr, "Found %zu groups in %s, select one of them:\n", ov.groups.size(), cfg.path.c_str());
      for (auto const &group : ov.groups) fprintf(stderr, " %s", group.c_str());
      fprintf(stderr, "\n");
      return CDI_ETMG;
    }

    // process var query information if available
    if (query && cdiQueryNumNames(query) > 0)
    {
      for (const auto &a : ov.arrays)
      {
        if (cdiQueryName(query, a.name.c_str()) == 0) cfg.arrays.push_back(a.name);
      }
    }
    else
    {
      std::set<std::string> coord_names;
      for (const auto &a : ov.arrays)
        for (const auto &d : a.dim_names) coord_names.insert(d);
      for (const auto &a : ov.arrays)
        if (!a.grid_mapping.empty()) coord_names.insert(a.grid_mapping);
      cfg.arrays.clear();
      for (const auto &a : ov.arrays)
        if (!coord_names.count(a.name)) cfg.arrays.push_back(a.name);
    }

    if (cfg.arrays.empty())
    {
      fprintf(stderr, "No data arrays found in %s!\n", cfg.path.c_str());
      return CDI_ENAF;
    }
  }

  if (query)
  {
    int numCells = cdiQueryNumCells(query);
    if (numCells > 0)
    {
      const int64_t *cellRange = cdiQueryGetCells(query);
      cfg.cell.start = cellRange[0];
      cfg.cell.end = cellRange[1];
    }
  }

  if (query)
  {
    int numLayers = cdiQueryNumLayers(query);
    if (numLayers > 0)
    {
      const int64_t *layerRange = cdiQueryGetLayers(query);
      cfg.layer.start = layerRange[0];
      cfg.layer.end = layerRange[1];
    }
  }

  if (!tsApplyConfig(handle))
  {
    printf("Apply config failed for %s!\n", cfg.path.c_str());
    return -1;
  }

  tsMetadata &md = tsGetReadMetadata(handle);

  std::set<std::string> used_dims;
  for (const auto &a : md.arrayList)
    for (const auto &d : a.dim_names) used_dims.insert(d);
  for (const auto &a : md.arrayList)
  {
    if (used_dims.count(a.name))
    {
      std::cerr << "'" << a.name << "' is a coordinate array and cannot be selected with --name.\n";
      tsCloseFile(handle);
      return 1;
    }
  }

  if (query)
  {
    int queryNumSteps = cdiQueryNumSteps(query);
    if (queryNumSteps > 0)
    {
      const int64_t *stepRange = cdiQueryGetSteps(query);
      cfg.step.start = stepRange[0];
      cfg.step.end = stepRange[1];
    }
  }

  auto timeDimName = find_time_dimname(md);
  for (auto &a : md.arrayList)
  {
    if (!a.shape.empty()) { a.time_constant = (a.dim_names[0] != timeDimName); }
  }

  if (CDI_Debug)
  {
    std::cout << "=== Selected arrays ===\n";
    std::cout << "#  name  format  kvstore  dtype  compressor  shape  chunks  indices";
    if (cfg.out_path) std::cout << "  out_chunks";
    std::cout << "\n";
  }
  {
    std::size_t idx = 1;
    for (std::size_t i = 0; i < md.arrayList.size(); ++i)
    {
      auto const &a = md.arrayList[i];
      if (!tsResolveArray(handle, i))
      {
        tsCloseFile(handle);
        return 1;
      }
      if (CDI_Debug)
      {
        std::cout << idx++ << "  " << a.name << "  zarr" << a.format << "  " << a.kvstore << "  " << a.dataType << "  "
                  << a.compressor << "  " << fmt_dims(a.shape) << "  " << fmt_dims(a.chunks) << "  " << fmt_indices(a);
        if (cfg.out_path) std::cout << "  " << fmt_dims(a.out_chunks);
        std::cout << '\n';
      }
    }
  }
  if (CDI_Debug) print_coordinate_arrays(md);

  // printf("Found %zu data arrays:\n", md.arrayList.size());
  // for (auto const &array : md.arrayList) printf(" %s", array.name.c_str());
  // printf("\n");

  {
    // auto const &array = md.arrayList[0];
    // printf("%s: num_steps=%lld, num_layers=%lld, num_cells=%lld\n", array.name.c_str(), array.num_steps, array.num_layers,
    //      array.num_cells);
    // for (const auto &[key, value] : array.attributes) printf("[%s] = %s\n", key.c_str(), value.c_str());
  }

  size_t numVars = md.arrayList.size();
  assert(numVars == 1);
  int vlistID = streamP->vlistID;

  auto numCells = static_cast<size_t>(md.arrayList[0].num_cells);
  // printf("numCells %zu\n", numCells);
  int gridID = CDI_UNDEFID;
  int surfaceID = zaxisCreate(ZAXIS_SURFACE, 1);

  size_t numSteps{};
  std::string timeUnits;
  int calendar = CALENDAR_STANDARD;
  std::map<std::string, int> zaxisMap;

  // printf("Found %zu coordinate arrays:\n", md.coordList.size());
  // for (auto const &array : md.coordList) printf("  %s: size=%lld\n", array.name.c_str(), array.shape[0]);
  for (auto const &array : md.coordList)
  {
    auto const &attributes = array.attributes;
    // for (auto const &[key, value] : attributes)
    {
      // printf("[%s] = %s\n", key.c_str(), value.c_str());
      // if (key == "grid_mapping_name" && value == "healpix") printf("healpix grid\n");
    }
    if (array.name == timeDimName)
    {
      numSteps = array.shape[0];
      if (attributes.contains("units")) { timeUnits = attributes.find("units")->second; }
      if (attributes.contains("calendar")) { calendar = attribute_to_calendar(attributes.find("calendar")->second.c_str()); }
    }
    else if (gridID == CDI_UNDEFID && attributes.contains("grid_mapping_name")
             && attributes.find("grid_mapping_name")->second == "healpix")
    {
      if (attributes.contains("refinement_level") && attributes.contains("indexing_scheme"))
      {
        gridID = gridCreate(GRID_HEALPIX, numCells);
        auto it1 = attributes.find("refinement_level");
        auto it2 = attributes.find("indexing_scheme");
        int attlen = 1;
        int refinement_level = std::stoi(it1->second);
        auto indexing_scheme = it2->second;
        cdiDefAttInt(gridID, CDI_GLOBAL, "refinement_level", CDI_DATATYPE_INT32, (int) attlen, &refinement_level);
        cdiDefAttTxt(gridID, CDI_GLOBAL, "indexing_scheme", (int) indexing_scheme.size(), indexing_scheme.c_str());
      }
    }
    else
    {
      auto numLayers = array.shape[0];
      std::vector<double> layers(numLayers);
      ts_read_array(handle, array.name.c_str(), layers.data());

      int64_t start = 0;
      if (query && cdiQueryNumLayers(query) > 0)
      {
        start = cfg.layer.start - 1;
        numLayers = *cfg.layer.end - cfg.layer.start + 1;
      }

      int zaxisID = zaxisCreate(ZAXIS_GENERIC, numLayers);
      zaxisDefLevels(zaxisID, layers.data() + start);
      cdiDefKeyString(zaxisID, CDI_GLOBAL, CDI_KEY_NAME, array.name.c_str());
      if (attributes.contains("units"))
      {
        auto units = attributes.find("units")->second;
        cdiDefKeyString(zaxisID, CDI_GLOBAL, CDI_KEY_UNITS, units.c_str());
      }
      zaxisMap[array.name] = zaxisID;
    }
  }

  if (gridID == CDI_UNDEFID) { gridID = gridCreate(GRID_GENERIC, numCells); }

  for (int k = 0; k < 2; ++k)
  {
    for (size_t i = 0; i < numVars; ++i)
    {
      auto const &a = md.arrayList[i];
      auto rank = a.shape.size();
      if (rank > 3) continue;
      int timeType = TIME_VARYING;
      // auto numCells = static_cast<size_t>(a.shape[rank - 1]);
      int zaxisID = surfaceID;
      if (rank == 3)
      {
        // zaxisID = zaxisCreate(ZAXIS_GENERIC, a.num_layers);
        // printf("array 3d: %s dimname %s zaxisID %d\n", a.name.c_str(), a.dim_names[1].c_str(), zaxisMap[a.dim_names[1]]);
        zaxisID = zaxisMap[a.dim_names[1]];
      }
      else if (a.dim_names[0] != timeDimName)
      {
        timeType = TIME_CONSTANT;
        if (rank == 2) { zaxisID = zaxisMap[a.dim_names[0]]; }
      }
      if (k == 0 && timeType == TIME_CONSTANT) continue;
      if (k == 1 && timeType == TIME_VARYING) continue;
      stream_new_var(streamP, gridID, zaxisID, CDI_UNDEFID);
      int varID = vlistDefVar(vlistID, gridID, zaxisID, timeType);
      vlistDefVarDatatype(vlistID, varID, dtype_str_to_id(a.dataType));
      vlistDefVarName(vlistID, varID, a.name.c_str());
      if (a.fill_value != "-")
      {
        auto missVal = string_to_double(a.fill_value);
        if (*missVal) vlistDefVarMissval(vlistID, varID, *missVal);
        // vlistDefVarMissval(vlistID, varID, std::nanf(""));
      }
      if (a.attributes.contains("units"))
      {
        auto units = a.attributes.find("units")->second;
        cdiDefKeyString(vlistID, varID, CDI_KEY_UNITS, units.c_str());
      }
    }
  }

  // Die Datenarrays dtype-neutral oeffnen (Lesepuffer arbeiten auf float32);
  // die Ausgabe-Stores erben daraus Attribute und Kompression.
  if (!handle->fields_open && !tsOpenFields(handle))
  {
    tsCloseFile(handle);
    return 1;
  }

  if (numSteps)
  {
    taxis_t taxis0;
    ptaxisInit(&taxis0);
    if (!timeUnits.empty() && set_base_time(timeUnits.c_str(), &taxis0) == 1)
    {
      // nctimevarid = CDI_UNDEFID;
      streamP->basetime.ncvarid = CDI_UNDEFID;
      streamP->basetime.hasUnits = false;
    }
    ptaxisDefName(&taxis0, timeDimName.c_str());
    ptaxisDefUnits(&taxis0, timeUnits.c_str());
    ptaxisDefDatatype(&taxis0, CDI_DATATYPE_FLT64);

    int taxisID = taxisCreate(TAXIS_RELATIVE);

    taxis0.calendar = calendar;
    taxisDefCalendar(taxisID, calendar);

    vlistDefTaxis(vlistID, taxisID);

    ts_read_timesteps((int) numSteps, streamP, &taxis0, timeDimName.c_str());
    taxisDestroyKernel(&taxis0);
  }

  handle->cur_t = handle->start_t;

  return 0;
}

int
ts_inq_timestep(stream_t *streamP, int tsID)
{
  auto *handle = get_tsHandle(streamP->fileID);
  std::int64_t step = tsGetNextStep(handle);

  streamP->curTsID = tsID;
  int numRecs = streamP->tsteps[tsID].nrecs;
  return numRecs;
}

template <typename T>
size_t
count_nummiss(T *data, size_t numCells, double mv)
{
  size_t numMiss = 0;
  auto missVal = static_cast<T>(mv);
  if (std::isnan(missVal))
  {
    for (size_t i = 0; i < numCells; ++i)
    {
      if (std::isnan(data[i])) numMiss++;
    }
  }
  else
  {
    constexpr auto is_equal = [](auto a, auto b) noexcept { return (!(a < b || b < a)); };
    for (size_t i = 0; i < numCells; ++i)
    {
      if (is_equal(data[i], missVal)) numMiss++;
    }
  }

  return numMiss;
}

void
ts_read_var_slice(stream_t *streamP, int varID, int levelID, int memtype, void *data, size_t *numMissVals)
{
  auto *handle = get_tsHandle(streamP->fileID);

  tsField field;
  if (!tsGetField(handle, field))
  {
    fprintf(stderr, "ts_read_var_slice::tsGetField failed\n");
    return;
  }
  tsMetadata const &md = tsGetReadMetadata(handle);
  const tsArray &a = md.arrayList[field.array];
  size_t numCells = a.num_cells;
  if (memtype == MEMTYPE_FLOAT)
    tsReadField(handle, static_cast<float *>(data), numCells);
  else
    tsReadField(handle, static_cast<double *>(data), numCells);

  streamP->numvals += numCells;

  size_t numMiss = 0;
  if (a.fill_value != "-")
  {
    auto missValOpt = string_to_double(a.fill_value);
    if (*missValOpt)
    {
      if (memtype == MEMTYPE_FLOAT)
        numMiss = count_nummiss(static_cast<float *>(data), numCells, *missValOpt);
      else
        numMiss = count_nummiss(static_cast<double *>(data), numCells, *missValOpt);
    }
  }
  *numMissVals = numMiss;
}

void
ts_read_field(stream_t *streamP, int memtype, void *data, size_t *numMissVals)
{
  int tsID = streamP->curTsID;
  int vrecID = streamP->tsteps[tsID].curRecID;
  int recID = streamP->tsteps[tsID].recIDs[vrecID];
  recinfo_t *recInfo = &streamP->tsteps[tsID].recinfo[recID];

  ts_read_var_slice(streamP, recInfo->varID, recInfo->levelID, memtype, data, numMissVals);
}
