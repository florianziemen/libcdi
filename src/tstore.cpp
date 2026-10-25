#include "tstore.h"
#include "cdi_int.h"
#include "tsio.h"

#include <cmath>  // nanf()
#include <array>
#include <iostream>
#include <iomanip>
#include <set>
#include <sstream>
#include <string>

static std::array<tsHandle *, 32> tsHandleIDs{ nullptr };

static int
get_next_fileID(tsHandle *handle)
{
  if (!handle) return -1;

  for (size_t i = 0; i < (int) tsHandleIDs.size(); ++i)
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
{
  return tsVersion();
}

int
ts_open(const char *fileName, const char *mode, int fileType)
{
  int fileID = -1;
  int fmode = tolower(*mode);
  switch (fmode)
  {
    case 'r':
    {
      printf("open %s\n", fileName + 6);
      tsConfig cfg;
      cfg.path = fileName + 6;
      tsOverview ov;
      ov.ok = tsDatasetOverview(cfg, ov);
      if (!ov.ok) return -1;
      if (!ov.groups.empty())
      {
        printf("Found %zu groups in %s, select one of them:\n", ov.groups.size(), fileName + 6);
        for (const auto &group : ov.groups) printf(" %s", group.c_str());
        printf("\n");
        return -1;
      }
      printf("Found %zu arrays in %s:\n", ov.arrays.size(), fileName + 6);
      for (const auto &row : ov.arrays) printf(" %s", row.name.c_str());
      printf("\n");
      std::set<std::string> coord_names;
      for (const auto &row : ov.arrays)
        for (const auto &d : row.dim_names) coord_names.insert(d);
      for (const auto &row : ov.arrays)
        if (!row.grid_mapping.empty()) coord_names.insert(row.grid_mapping);
      cfg.arrays.clear();
      for (const auto &row : ov.arrays)
        if (!coord_names.count(row.name)) cfg.arrays.push_back(row.name);
      if (cfg.arrays.empty())
      {
        printf("No data arrays found in %s!", fileName + 6);
        return -1;
      };

      tsHandle *handle = tsOpenFileRead(fileName + 6, cfg);
      if (!handle) return -1;

      fileID = get_next_fileID(handle);
      break;
    }
    case 'w':
      tsConfig cfg;
      fileID = get_next_fileID(tsOpenFileWrite(fileName, cfg));
      break;
  }

  printf("%s: %s fileID=%d\n", __func__, fileName, fileID);
  if (fileID < 0) fprintf(stderr, "%s: fileID failed for %s!", __func__, fileName);
  return fileID;
}

void
ts_close(int fileID)
{
  printf("%s: fileID=%d\n", __func__, fileID);
  tsCloseFile(get_tsHandle(fileID));
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

static void
ts_read_timesteps(size_t numTimesteps, stream_t *streamP, taxis_t *taxis0)
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

    int *ncStepIndices = (int *) malloc(numTimesteps * sizeof(int));
    for (size_t tsID = 0; tsID < numTimesteps; ++tsID) { ncStepIndices[tsID] = (int) tsID; }

    CdiDateTime *vDateTimeList = NULL;

    // if (hasTimesteps)
    {
      // vDateTimeList = (CdiDateTime *) malloc(numTimesteps * sizeof(CdiDateTime));
      vDateTimeList = (CdiDateTime *) malloc(1000 * sizeof(CdiDateTime));

      if (1 || streamP->basetime.hasUnits)
      {
        printf("numTimesteps %ld\n", numTimesteps);
        // double *timevarBuffer = (double *) malloc(numTimesteps * sizeof(double));
        double *timevarBuffer = (double *) malloc(1000 * sizeof(double));
        // cdf_get_var_double(fileID, nctimevarid, timevarBuffer);
        auto handle = get_tsHandle(streamP->fileID);
        ts_read_array(handle, "time", timevarBuffer);
        for (int i = 0; i < 16; ++i) printf("step %d %g\n", i + 1, timevarBuffer[i]);
        for (size_t i = 0; i < numTimesteps; ++i)
        {
          // vDateTimeList[i] = cdi_decode_timeval(get_timevalue(fileID, nctimevarid, i, timevarBuffer), taxis0);
          // vDateTimeList[i] = cdi_decode_timeval(timevarBuffer[i], taxis0);
        }
        free(timevarBuffer);
      }
      else
      {
        hasTimesteps = false;
      }
    }

    // process step query information if available
    // process_time_query(streamP->query, numTimesteps, vDateTimeList, ncStepIndices);

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

int
ts_inq_contents(stream_t *streamP)
{
  printf("%s: fileID=%d\n", __func__, streamP->fileID);

  auto handle = get_tsHandle(streamP->fileID);
  auto const &cfg = handle->cfg;

  tsMetadata const &md = tsGetReadMetadata(handle);

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

  std::cout << "=== Selected arrays ===\n";
  std::cout << "#  name  format  kvstore  dtype  compressor  shape  chunks  indices";
  if (cfg.out_path) std::cout << "  out_chunks";
  std::cout << "\n";
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
      std::cout << idx++ << "  " << a.name << "  zarr" << a.format << "  " << a.kvstore << "  " << a.dataType << "  "
                << a.compressor << "  " << fmt_dims(a.shape) << "  " << fmt_dims(a.chunks) << "  " << fmt_indices(a);
      if (cfg.out_path) std::cout << "  " << fmt_dims(a.out_chunks);
      std::cout << '\n';
    }
  }
  print_coordinate_arrays(md);

  handle->metadata.num_steps = 16;
  printf("numSteps %lld\n", md.num_steps);
  printf("Found %zu data arrays:\n", md.arrayList.size());
  for (auto const &array : md.arrayList) printf(" %s", array.name.c_str());
  printf("\n");
  auto const &array = md.arrayList[0];
  printf("%s: num_steps=%lld, num_layers=%lld, num_cells=%lld\n", array.name.c_str(), array.num_steps, array.num_layers,
         array.num_cells);
  auto const &attributes = array.attributes;
  for (const auto &[key, value] : attributes) printf("[%s] = %s\n", key.c_str(), value.c_str());

  int gridID = -1;
  int zaxisID = -1;
  int taxisID = taxisCreate(TAXIS_ABSOLUTE);
  printf("ts_inq_contents: taxisID=%d\n", taxisID);

  size_t numVars = md.arrayList.size();
  assert(numVars == 1);
  int vlistID = streamP->vlistID;

  int timeType = TIME_VARYING;
  for (size_t i = 0; i < numVars; ++i)
  {
    auto const &a = md.arrayList[i];
    auto rank = a.shape.size();
    if (rank != 2 && rank != 3) continue;
    auto numCells = static_cast<size_t>(a.shape[rank - 1]);
    printf("%zu %s: numCells=%zu %lld\n", i + 1, a.name.c_str(), numCells, a.num_cells);
    gridID = gridCreate(GRID_HEALPIX, numCells);
    if (rank == 3) { zaxisID = zaxisCreate(ZAXIS_GENERIC, a.num_layers); }
    else
    {
      zaxisID = zaxisCreate(ZAXIS_SURFACE, 1);
    }
    stream_new_var(streamP, gridID, zaxisID, CDI_UNDEFID);
    int varID = vlistDefVar(vlistID, gridID, zaxisID, timeType);
    vlistDefVarDatatype(vlistID, varID, DATATYPE_FLT32);
    vlistDefVarName(vlistID, varID, a.name.c_str());
    vlistDefVarMissval(vlistID, varID, std::nanf(""));
  }

  vlistDefTaxis(vlistID, taxisID);
  taxis_t taxis0;
  ptaxisInit(&taxis0);
  int ntsteps = 16;
  ts_read_timesteps(ntsteps, streamP, &taxis0);
  taxisDestroyKernel(&taxis0);

  printf("Found %zu coordinate arrays:\n", md.coordList.size());
  for (auto const &array : md.coordList) printf("  %s: size=%lld\n", array.name.c_str(), array.shape[0]);
  for (auto const &array : md.coordList)
  {
    auto const &attributes = array.attributes;
    for (const auto &[key, value] : attributes)
    {
      printf("[%s] = %s\n", key.c_str(), value.c_str());
      if (key == "grid_mapping_name" && value == "healpix") printf("healpix grid\n");
    }
    if (attributes.contains("grid_mapping_name") && attributes.find("grid_mapping_name")->second == "healpix")
    {
      printf("healpix grid\n");
    }
  }

  // Die Datenarrays dtype-neutral oeffnen (Lesepuffer arbeiten auf float32);
  // die Ausgabe-Stores erben daraus Attribute und Kompression.
  if (!handle->fields_open && !tsOpenFields(handle))
  {
    tsCloseFile(handle);
    return 1;
  }

  handle->metadata.num_steps = 16;
  // Ein sequentieller Durchlauf: aeussere Schleife ueber die Zeitschritte,
  // innere Schleife ueber die (konstant viele) Felder je Schritt.  Zeitdimension
  // und Feldzahl sind fuer alle Arrays gleich; jedes Feld wird genau einmal
  // gelesen, im Schreibmodus direkt in den Ausgabestore geschrieben.
  /*
  const std::size_t numFields = tsGetNumFields(handle);
  tsField field;
  std::vector<float> data;
  std::int64_t step;
  if (!cfg.out_path) { std::cout << "# timestep  layer  count  missing  min  mean  max  array\n"; }
  while ((step = tsGetNextStep(handle)) >= 0)
  {
    for (std::size_t f = 0; f < numFields; ++f)
    {
      if (!tsGetField(handle, field)) break;
      const tsArray &a = md.arrayList[field.array];
      data.resize(static_cast<std::size_t>(a.num_cells));
      if (!tsReadField(handle, data.data(), data.size()))
      {
        tsCloseFile(handle);
        return 1;
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
  handle->cur_t = handle->start_t - 1;
  handle->next_field = 0;
  handle->have_field = false;
  handle->done = false;
  */
  handle->cur_t = handle->start_t;

  return 0;
}

int
ts_inq_timestep(stream_t *streamP, int tsID)
{
  auto handle = get_tsHandle(streamP->fileID);
  std::int64_t step = tsGetNextStep(handle);
  return (int) step;
}

void
ts_read_var_slice(stream_t *streamP, int varID, int levelID, int memtype, void *data, size_t *numMissVals)
{
  auto handle = get_tsHandle(streamP->fileID);

  tsField field;
  if (!tsGetField(handle, field))
  {
    printf("ts_read_var_slice::tsGetField failed\n");
    return;
  }
  tsMetadata const &md = tsGetReadMetadata(handle);
  const tsArray &a = md.arrayList[field.array];
  size_t numCells = a.num_cells;
  float *fdata = (float *) data;
  tsReadField(handle, fdata, numCells);
  size_t numMiss = 0;
  for (size_t i = 0; i < numCells; ++i)
  {
    if (std::isnan(fdata[i])) numMiss++;
  }
  *numMissVals = numMiss;

  if (memtype == MEMTYPE_DOUBLE)
  {
    double *ddata = (double *) data;
    for (size_t i = 0; i < numCells; ++i) { ddata[numCells - i - 1] = fdata[numCells - i - 1]; }
  }

  /*
  if (memtype == MEMTYPE_DOUBLE)
    cdfReadVarSliceDP(streamP, varID, levelID, (double *) data, numMissVals);
  else
    cdfReadVarSliceSP(streamP, varID, levelID, (float *) data, numMissVals);
  */
}
