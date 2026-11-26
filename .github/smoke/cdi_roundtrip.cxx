// Write a small dataset with CDI, read it back and compare everything.
//
// Covers what ParaView's CDIReader relies on: netCDF classic/netCDF4, a regular
// lon/lat grid with pressure levels, an unstructured (ICON-like) grid with cell
// bounds, a time axis, and variable names. Usage: cdi_roundtrip nc|nc4

// Must match the library build; ParaView's CDIReader does the same.
#define CDI_SIZE_TYPE size_t
#include "cdi.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace
{

constexpr int nlon = 12, nlat = 6, nlev = 5, ncell = 4, nvertex = 3, nts = 3;
constexpr int vtime = 120000;
const double lons[nlon] = { 0, 30, 60, 90, 120, 150, 180, 210, 240, 270, 300, 330 };
const double lats[nlat] = { -75, -45, -15, 15, 45, 75 };
const double levels[nlev] = { 101300, 92500, 85000, 50000, 20000 };
// Four triangles between the equator and the north pole.
const double clon[ncell] = { 45, 135, 225, 315 };
const double clat[ncell] = { 30, 30, 30, 30 };
const double vlon[ncell * nvertex] = { 0, 90, 45, 90, 180, 135, 180, 270, 225, 270, 360, 315 };
const double vlat[ncell * nvertex] = { 0, 0, 90, 0, 0, 90, 0, 0, 90, 0, 0, 90 };

struct VarSpec
{
  const char *name;
  size_t size;  // values per time step (grid size * levels)
};
const VarSpec vars[] = { { "ta", static_cast<size_t>(nlon * nlat * nlev) }, { "ts_icon", static_cast<size_t>(ncell) } };
constexpr int nvars = 2;

int failures = 0;

void
check(bool ok, const std::string &what)
{
  if (!ok)
  {
    std::fprintf(stderr, "FAIL: %s\n", what.c_str());
    ++failures;
  }
}

double
expected(int varID, int tsID, size_t i)
{
  return 1000.0 * varID + 100.0 * tsID + 0.25 * static_cast<double>(i);
}

int
vdate(int tsID)
{
  return 19850101 + tsID;
}

bool
same(const double *a, const double *b, size_t n)
{
  for (size_t i = 0; i < n; ++i)
    if (std::fabs(a[i] - b[i]) > 1e-9 * (1.0 + std::fabs(b[i]))) return false;
  return true;
}

int
write(const char *path, int filetype)
{
  int lonlat = gridCreate(GRID_LONLAT, nlon * nlat);
  gridDefXsize(lonlat, nlon);
  gridDefYsize(lonlat, nlat);
  gridDefXvals(lonlat, lons);
  gridDefYvals(lonlat, lats);

  int unstructured = gridCreate(GRID_UNSTRUCTURED, ncell);
  gridDefNvertex(unstructured, nvertex);
  gridDefXvals(unstructured, clon);
  gridDefYvals(unstructured, clat);
  gridDefXbounds(unstructured, vlon);
  gridDefYbounds(unstructured, vlat);

  int pressure = zaxisCreate(ZAXIS_PRESSURE, nlev);
  zaxisDefLevels(pressure, levels);
  int surface = zaxisCreate(ZAXIS_SURFACE, 1);

  int vlistID = vlistCreate();
  int varIDs[nvars] = { vlistDefVar(vlistID, lonlat, pressure, TIME_VARYING),
                        vlistDefVar(vlistID, unstructured, surface, TIME_VARYING) };
  for (int v = 0; v < nvars; ++v)
  {
    vlistDefVarName(vlistID, varIDs[v], vars[v].name);
    vlistDefVarDatatype(vlistID, varIDs[v], CDI_DATATYPE_FLT64);
  }
  int taxisID = taxisCreate(TAXIS_ABSOLUTE);
  vlistDefTaxis(vlistID, taxisID);

  int streamID = streamOpenWrite(path, filetype);
  if (streamID < 0)
  {
    std::fprintf(stderr, "FAIL: streamOpenWrite(%s): %s\n", path, cdiStringError(streamID));
    return 1;
  }
  streamDefVlist(streamID, vlistID);
  for (int tsID = 0; tsID < nts; ++tsID)
  {
    taxisDefVdate(taxisID, vdate(tsID));
    taxisDefVtime(taxisID, vtime);
    streamDefTimestep(streamID, tsID);
    for (int v = 0; v < nvars; ++v)
    {
      std::vector<double> data(vars[v].size);
      for (size_t i = 0; i < data.size(); ++i) data[i] = expected(v, tsID, i);
      streamWriteVar(streamID, varIDs[v], data.data(), 0);
    }
  }
  streamClose(streamID);

  vlistDestroy(vlistID);
  taxisDestroy(taxisID);
  zaxisDestroy(pressure);
  zaxisDestroy(surface);
  gridDestroy(lonlat);
  gridDestroy(unstructured);
  return 0;
}

int
findVar(int vlistID, const char *name)
{
  char buf[CDI_MAX_NAME];
  for (int varID = 0; varID < vlistNvars(vlistID); ++varID)
  {
    vlistInqVarName(vlistID, varID, buf);
    if (std::strcmp(buf, name) == 0) return varID;
  }
  return CDI_UNDEFID;
}

void
checkGrids(int vlistID, int taID, int iconID)
{
  int lonlat = vlistInqVarGrid(vlistID, taID);
  check(gridInqType(lonlat) == GRID_LONLAT, "ta is on a lon/lat grid");
  check(gridInqSize(lonlat) == static_cast<SizeType>(nlon * nlat), "lon/lat grid size");
  std::vector<double> buf(nlon * nlat);
  check(gridInqXvals(lonlat, buf.data()) == static_cast<SizeType>(nlon) && same(buf.data(), lons, nlon),
        "longitudes");
  check(gridInqYvals(lonlat, buf.data()) == static_cast<SizeType>(nlat) && same(buf.data(), lats, nlat),
        "latitudes");

  int pressure = vlistInqVarZaxis(vlistID, taID);
  check(zaxisInqSize(pressure) == nlev, "number of pressure levels");
  for (int l = 0; l < nlev && l < zaxisInqSize(pressure); ++l)
    check(zaxisInqLevel(pressure, l) == levels[l], "pressure level " + std::to_string(l));

  int unstructured = vlistInqVarGrid(vlistID, iconID);
  check(gridInqType(unstructured) == GRID_UNSTRUCTURED, "ts_icon is on an unstructured grid");
  check(gridInqSize(unstructured) == static_cast<SizeType>(ncell), "unstructured grid size");
  check(gridInqNvertex(unstructured) == nvertex, "vertices per cell");
  std::vector<double> bounds(ncell * nvertex);
  check(gridInqXvals(unstructured, buf.data()) == static_cast<SizeType>(ncell) && same(buf.data(), clon, ncell),
        "cell center longitudes");
  check(gridInqYvals(unstructured, buf.data()) == static_cast<SizeType>(ncell) && same(buf.data(), clat, ncell),
        "cell center latitudes");
  check(gridInqXbounds(unstructured, bounds.data()) == static_cast<SizeType>(ncell * nvertex)
            && same(bounds.data(), vlon, ncell * nvertex),
        "cell vertex longitudes");
  check(gridInqYbounds(unstructured, bounds.data()) == static_cast<SizeType>(ncell * nvertex)
            && same(bounds.data(), vlat, ncell * nvertex),
        "cell vertex latitudes");
}

int
read(const char *path, int filetype)
{
  int streamID = streamOpenRead(path);
  if (streamID < 0)
  {
    std::fprintf(stderr, "FAIL: streamOpenRead(%s): %s\n", path, cdiStringError(streamID));
    return 1;
  }
  check(streamInqFiletype(streamID) == filetype, "file type");

  int vlistID = streamInqVlist(streamID);
  check(vlistNvars(vlistID) == nvars, "number of variables");
  int varIDs[nvars];
  for (int v = 0; v < nvars; ++v)
  {
    varIDs[v] = findVar(vlistID, vars[v].name);
    check(varIDs[v] != CDI_UNDEFID, std::string("variable ") + vars[v].name + " exists");
  }
  if (failures) return 1;
  checkGrids(vlistID, varIDs[0], varIDs[1]);

  int taxisID = vlistInqTaxis(vlistID);
  int tsID = 0;
  while (streamInqTimestep(streamID, tsID) > 0)
  {
    const std::string ts = "time step " + std::to_string(tsID);
    check(taxisInqVdate(taxisID) == vdate(tsID), ts + ": date");
    check(taxisInqVtime(taxisID) == vtime, ts + ": time");
    for (int v = 0; v < nvars; ++v)
    {
      std::vector<double> data(vars[v].size), want(vars[v].size);
      for (size_t i = 0; i < want.size(); ++i) want[i] = expected(v, tsID, i);
      SizeType numMissVals = 1;
      streamReadVar(streamID, varIDs[v], data.data(), &numMissVals);
      check(numMissVals == 0, ts + ": " + vars[v].name + " has no missing values");
      check(same(data.data(), want.data(), want.size()), ts + ": " + vars[v].name + " values");
    }
    ++tsID;
  }
  check(tsID == nts, "number of time steps (got " + std::to_string(tsID) + ")");
  streamClose(streamID);
  return 0;
}

}  // namespace

int
main(int argc, char **argv)
{
  const std::string format = argc > 1 ? argv[1] : "nc";
  int filetype;
  if (format == "nc")
    filetype = CDI_FILETYPE_NC;
  else if (format == "nc4")
    filetype = CDI_FILETYPE_NC4;
  else
  {
    std::fprintf(stderr, "usage: %s nc|nc4\n", argv[0]);
    return 2;
  }
  std::printf("CDI library version: %s\n", cdiLibraryVersion());

  const std::string path = "roundtrip." + format;
  if (write(path.c_str(), filetype) != 0) return 1;
  if (read(path.c_str(), filetype) != 0 || failures)
  {
    std::fprintf(stderr, "%d check(s) failed for %s\n", failures, path.c_str());
    return 1;
  }
  std::printf("OK: %s round trip\n", path.c_str());
  return 0;
}
