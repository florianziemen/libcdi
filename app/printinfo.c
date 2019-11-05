#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>

#include <cdi.h>
#include "cdi_uuid.h"

#include "cdi_int.h"

#include "printinfo.h"

void
datetime2str(int64_t date, int time, char *datetimestr, int maxlen)
{
  int year, month, day;
  cdiDecodeDate(date, &year, &month, &day);
  int hour, minute, second;
  cdiDecodeTime(time, &hour, &minute, &second);

  const int len = sprintf(datetimestr, DATE_FORMAT "T" TIME_FORMAT, year, month, day, hour, minute, second);
  if (len > (maxlen - 1)) fprintf(stderr, "Internal problem (%s): sizeof input string is too small!\n", __func__);
}

void
date2str(int64_t date, char *datestr, int maxlen)
{
  int year, month, day;
  cdiDecodeDate(date, &year, &month, &day);

  const int len = sprintf(datestr, DATE_FORMAT, year, month, day);
  if (len > (maxlen - 1)) fprintf(stderr, "Internal problem (%s): sizeof input string is too small!\n", __func__);
}

void
time2str(int time, char *timestr, int maxlen)
{
  int hour, minute, second;
  cdiDecodeTime(time, &hour, &minute, &second);

  const int len = sprintf(timestr, TIME_FORMAT, hour, minute, second);
  if (len > (maxlen - 1)) fprintf(stderr, "Internal problem (%s): sizeof input string is too small!\n", __func__);
}

const char *
comp_name(int comptype)
{
  if (comptype == CDI_COMPRESS_SZIP) return "szip";
  if (comptype == CDI_COMPRESS_ZIP) return "zip";
  if (comptype == CDI_COMPRESS_JPEG) return "jpeg";
  if (comptype == CDI_COMPRESS_AEC) return "aec";
  return " ";
}

void
printFiletype(int streamID, int vlistID)
{
  const int filetype = streamInqFiletype(streamID);

  // clang-format off
  switch ( filetype )
    {
    case CDI_FILETYPE_GRB:  printf("GRIB");  break;
    case CDI_FILETYPE_GRB2: printf("GRIB2");  break;
    case CDI_FILETYPE_NC:   printf("NetCDF");  break;
    case CDI_FILETYPE_NC2:  printf("NetCDF2");  break;
    case CDI_FILETYPE_NC4:  printf("NetCDF4");  break;
    case CDI_FILETYPE_NC4C: printf("NetCDF4 classic");  break;
    case CDI_FILETYPE_NC5:  printf("NetCDF5");  break;
    case CDI_FILETYPE_SRV:  printf("SERVICE");  break;
    case CDI_FILETYPE_EXT:  printf("EXTRA");  break;
    case CDI_FILETYPE_IEG:  printf("IEG");  break;
    default: printf("  File format: unsupported filetype %d" , filetype);  break;
    }

  if ( filetype == CDI_FILETYPE_SRV || filetype == CDI_FILETYPE_EXT || filetype == CDI_FILETYPE_IEG )
    {
      switch ( streamInqByteorder(streamID) )
	{
	case CDI_BIGENDIAN:    printf("  BIGENDIAN");  break;
	case CDI_LITTLEENDIAN: printf("  LITTLEENDIAN");  break;
	default: printf("  byteorder: %d undefined", streamInqByteorder(streamID));  break;
	}
    }
  // clang-format on

  const int nvars = vlistNvars(vlistID);
  const int comps[] = { CDI_COMPRESS_ZIP, CDI_COMPRESS_JPEG, CDI_COMPRESS_SZIP, CDI_COMPRESS_AEC };
  unsigned kk = 0;
  for (unsigned k = 0; k < sizeof(comps) / sizeof(int); ++k)
    for (int varID = 0; varID < nvars; varID++)
      {
        const int comptype = vlistInqVarCompType(vlistID, varID);
        if (comptype == comps[k])
          {
            printf("%c%s", (kk++ == 0) ? ' ' : '/', comp_name(comptype));
            break;
          }
      }

  printf("\n");
}

static void
print_xvals(int gridID, int dig)
{
  const size_t xsize = gridInqXsize(gridID);
  if (xsize > 0 && gridInqXvals(gridID, NULL))
    {
      char xname[CDI_MAX_NAME], xunits[CDI_MAX_NAME];
      gridInqXname(gridID, xname);
      gridInqXunits(gridID, xunits);

      const double xfirst = gridInqXval(gridID, 0);
      const double xlast = gridInqXval(gridID, xsize - 1);
      const double xinc = gridInqXinc(gridID);
      fprintf(stdout, "%33s : %.*g", xname, dig, xfirst);
      if (xsize > 1)
        {
          fprintf(stdout, " to %.*g", dig, xlast);
          if (IS_NOT_EQUAL(xinc, 0)) fprintf(stdout, " by %.*g", dig, xinc);
        }
      fprintf(stdout, " %s", xunits);
      if (gridIsCircular(gridID)) fprintf(stdout, "  circular");
      fprintf(stdout, "\n");
    }
}

static void
print_yvals(int gridID, int dig)
{
  const size_t ysize = gridInqYsize(gridID);
  if (ysize > 0 && gridInqYvals(gridID, NULL))
    {
      char yname[CDI_MAX_NAME], yunits[CDI_MAX_NAME];
      gridInqYname(gridID, yname);
      gridInqYunits(gridID, yunits);

      const double yfirst = gridInqYval(gridID, 0);
      const double ylast = gridInqYval(gridID, ysize - 1);
      const double yinc = gridInqYinc(gridID);
      fprintf(stdout, "%33s : %.*g", yname, dig, yfirst);
      if (ysize > 1)
        {
          const int gridtype = gridInqType(gridID);
          fprintf(stdout, " to %.*g", dig, ylast);
          if (IS_NOT_EQUAL(yinc, 0) && gridtype != GRID_GAUSSIAN && gridtype != GRID_GAUSSIAN_REDUCED)
            fprintf(stdout, " by %.*g", dig, yinc);
        }
      fprintf(stdout, " %s", yunits);
      fprintf(stdout, "\n");
    }
}

static void
print_xyvals2D(int gridID, int dig)
{
  if (gridInqXvals(gridID, NULL) && gridInqYvals(gridID, NULL))
    {
      char xname[CDI_MAX_NAME], yname[CDI_MAX_NAME], xunits[CDI_MAX_NAME], yunits[CDI_MAX_NAME];
      gridInqXname(gridID, xname);
      gridInqYname(gridID, yname);
      gridInqXunits(gridID, xunits);
      gridInqYunits(gridID, yunits);

      const size_t gridsize = gridInqSize(gridID);
      double *xvals2D = (double *) malloc(gridsize * sizeof(double));
      double *yvals2D = (double *) malloc(gridsize * sizeof(double));

      gridInqXvals(gridID, xvals2D);
      gridInqYvals(gridID, yvals2D);

      double xfirst = xvals2D[0];
      double xlast = xvals2D[0];
      double yfirst = yvals2D[0];
      double ylast = yvals2D[0];
      for (size_t i = 1; i < gridsize; i++)
        {
          if (xvals2D[i] < xfirst) xfirst = xvals2D[i];
          if (xvals2D[i] > xlast) xlast = xvals2D[i];
          if (yvals2D[i] < yfirst) yfirst = yvals2D[i];
          if (yvals2D[i] > ylast) ylast = yvals2D[i];
        }

      double xinc = 0;
      double yinc = 0;
      int gridtype = gridInqType(gridID);
      if (gridtype == GRID_CURVILINEAR)
        {
          const size_t xsize = gridInqXsize(gridID);
          const size_t ysize = gridInqYsize(gridID);
          if (xsize > 1)
            {
              double *xvals = (double *) malloc((size_t) xsize * sizeof(double));
              for (size_t i = 0; i < xsize; ++i) xvals[i] = xvals2D[i];
              xinc = fabs(xvals[xsize - 1] - xvals[0]) / (xsize - 1);
              for (size_t i = 1; i < xsize; i++)
                if (fabs(fabs(xvals[i - 1] - xvals[i]) - xinc) > 0.01 * xinc)
                  {
                    xinc = 0;
                    break;
                  }
              free(xvals);
              if (IS_NOT_EQUAL(xinc, 0))
                {
                  for (size_t i = 1; i < ysize; i++)
                    if (IS_NOT_EQUAL(xvals2D[i * xsize], xvals2D[0])
                        || IS_NOT_EQUAL(xvals2D[(i + 1) * xsize - 1], xvals2D[xsize - 1]))
                      {
                        xinc = 0;
                        break;
                      }
                }
            }
          if (ysize > 1)
            {
              double *yvals = (double *) malloc((size_t) ysize * sizeof(double));
              for (size_t i = 0; i < ysize; ++i) yvals[i] = yvals2D[i * xsize];
              yinc = fabs(yvals[ysize - 1] - yvals[0]) / (ysize - 1);
              for (size_t i = 1; i < ysize; i++)
                if (fabs(fabs(yvals[i - 1] - yvals[i]) - yinc) > 0.01 * yinc)
                  {
                    yinc = 0;
                    break;
                  }
              free(yvals);
              if (IS_NOT_EQUAL(yinc, 0))
                {
                  for (size_t i = 1; i < xsize; i++)
                    if (IS_NOT_EQUAL(yvals2D[i], yvals2D[0])
                        || IS_NOT_EQUAL(yvals2D[(ysize - 1) * xsize + i], yvals2D[(ysize - 1) * xsize]))
                      {
                        yinc = 0;
                        break;
                      }
                }
            }
        }

      fprintf(stdout, "%33s : %.*g", xname, dig, xfirst);
      if (gridsize > 1) fprintf(stdout, " to %.*g", dig, xlast);
      if (IS_NOT_EQUAL(xinc, 0)) fprintf(stdout, " by %.*g", dig, xinc);
      fprintf(stdout, " %s", xunits);
      if (gridIsCircular(gridID)) fprintf(stdout, "  circular");
      fprintf(stdout, "\n");
      fprintf(stdout, "%33s : %.*g", yname, dig, yfirst);
      if (gridsize > 1) fprintf(stdout, " to %.*g", dig, ylast);
      if (IS_NOT_EQUAL(yinc, 0)) fprintf(stdout, " by %.*g", dig, yinc);
      fprintf(stdout, " %s", yunits);
      fprintf(stdout, "\n");

      free(xvals2D);
      free(yvals2D);
    }
}

static void
printGridNp(int gridtype, int gridID, size_t gridsize, size_t xsize, size_t ysize)
{

  size_t xysize = xsize * ysize;
  fprintf(stdout, "points=%zu", gridsize);
  if (gridtype == GRID_GAUSSIAN_REDUCED)
    fprintf(stdout, "  nlat=%zu", ysize);
  else if (xysize)
    fprintf(stdout, " (%zux%zu)", xsize, ysize);

  if (gridtype == GRID_GAUSSIAN || gridtype == GRID_GAUSSIAN_REDUCED) fprintf(stdout, "  np=%d", gridInqNP(gridID));

  fprintf(stdout, "\n");
}

static void
printGridInfoKernel(int gridID, int index, bool lproj)
{
  int gridtype = gridInqType(gridID);

  if (lproj && gridtype != GRID_PROJECTION)
    fprintf(stderr, "Internal problem (%s): sub grid not equal GRID_PROJECTION!\n", __func__);

  const int trunc = gridInqTrunc(gridID);
  const size_t gridsize = gridInqSize(gridID);
  const size_t xsize = gridInqXsize(gridID);
  const size_t ysize = gridInqYsize(gridID);

  // int prec     = gridInqDatatype(gridID);
  // int dig = (prec == CDI_DATATYPE_FLT64) ? 15 : 7;
  int dig = 7;

  if (!lproj)
    {
      fprintf(stdout, "  %4d : ", index + 1);
      fprintf(stdout, "%-24s", gridNamePtr(gridtype));
      fprintf(stdout, " : ");
    }

  if (gridtype == GRID_LONLAT || gridtype == GRID_PROJECTION || gridtype == GRID_GENERIC || gridtype == GRID_CHARXY
      || gridtype == GRID_GAUSSIAN || gridtype == GRID_GAUSSIAN_REDUCED)
    {
      if (!lproj)
        {
          printGridNp(gridtype, gridID, gridsize, xsize, ysize);
        }

      char name[CDI_MAX_NAME];
      name[0] = 0;
      cdiGridInqKeyStr(gridID, CDI_KEY_GRIDMAP_NAME, CDI_MAX_NAME, name);
      if (gridtype == GRID_PROJECTION || name[0])
        {
          if (name[0] == 0) strcpy(name, "undefined");
          fprintf(stdout, "         %24s", "mapping");
          fprintf(stdout, " : ");
          fprintf(stdout, "%s\n", name);
        }

      print_xvals(gridID, dig);
      print_yvals(gridID, dig);

      if (gridInqXbounds(gridID, NULL) || gridInqYbounds(gridID, NULL))
        {
          fprintf(stdout, "%33s :", "available");
          if (gridInqXbounds(gridID, NULL) && gridInqYbounds(gridID, NULL)) fprintf(stdout, " cellbounds");
          if (gridHasArea(gridID)) fprintf(stdout, " area");
          if (gridInqMask(gridID, NULL)) fprintf(stdout, " mask");
          fprintf(stdout, "\n");
        }
    }
  else if (gridtype == GRID_SPECTRAL)
    {
      fprintf(stdout, "points=%zu  nsp=%zu  truncation=%d", gridsize, gridsize / 2, trunc);
      if (gridInqComplexPacking(gridID)) fprintf(stdout, "  complexPacking");
      fprintf(stdout, "\n");
    }
  else if (gridtype == GRID_FOURIER)
    {
      fprintf(stdout, "points=%zu  nfc=%zu  truncation=%d\n", gridsize, gridsize / 2, trunc);
    }
  else if (gridtype == GRID_GME)
    {
      int nd, ni, ni2, ni3;
      gridInqParamGME(gridID, &nd, &ni, &ni2, &ni3);
      fprintf(stdout, "points=%zu  nd=%d  ni=%d\n", gridsize, nd, ni);
    }
  else if (gridtype == GRID_CURVILINEAR || gridtype == GRID_UNSTRUCTURED)
    {
      if (gridtype == GRID_CURVILINEAR)
        fprintf(stdout, "points=%zu (%zux%zu)", gridsize, xsize, ysize);
      else
        fprintf(stdout, "points=%zu", gridsize);

      if (gridtype == GRID_UNSTRUCTURED && gridInqNvertex(gridID) > 0) fprintf(stdout, "  nvertex=%d", gridInqNvertex(gridID));

      fprintf(stdout, "\n");

      if (gridtype == GRID_UNSTRUCTURED)
        {
          const int number = gridInqNumber(gridID);
          const int position = gridInqPosition(gridID);
          if (number > 0) fprintf(stdout, "%33s : number=%d  position=%d\n", "grid", number, position);

          if (gridInqReference(gridID, NULL))
            {
              char reference_link[8192];
              gridInqReference(gridID, reference_link);
              fprintf(stdout, "%33s : %s\n", "uri", reference_link);
            }
        }

      print_xyvals2D(gridID, dig);
    }
  else /* if ( gridtype == GRID_GENERIC ) */
    {
      if (ysize == 0)
        fprintf(stdout, "points=%zu\n", gridsize);
      else
        fprintf(stdout, "points=%zu (%zux%zu)\n", gridsize, xsize, ysize);
    }

  if (gridtype == GRID_CURVILINEAR || gridtype == GRID_UNSTRUCTURED)
    {
      if (gridHasArea(gridID) || gridInqXbounds(gridID, NULL) || gridInqYbounds(gridID, NULL))
        {
          fprintf(stdout, "%33s :", "available");
          if (gridInqXbounds(gridID, NULL) && gridInqYbounds(gridID, NULL)) fprintf(stdout, " cellbounds");
          if (gridHasArea(gridID)) fprintf(stdout, " area");
          if (gridInqMask(gridID, NULL)) fprintf(stdout, " mask");
          fprintf(stdout, "\n");
        }
    }

  unsigned char uuidOfHGrid[CDI_UUID_SIZE];
  gridInqUUID(gridID, uuidOfHGrid);
  if (!cdiUUIDIsNull(uuidOfHGrid))
    {
      char uuidOfHGridStr[37];
      cdiUUID2Str(uuidOfHGrid, uuidOfHGridStr);
      if (uuidOfHGridStr[0] != 0 && strlen(uuidOfHGridStr) == 36)
        {
          fprintf(stdout, "%33s : %s\n", "uuid", uuidOfHGridStr);
        }
    }
}

void
printGridInfo(int vlistID)
{
  const int ngrids = vlistNgrids(vlistID);
  for (int index = 0; index < ngrids; index++)
    {
      const int gridID = vlistGrid(vlistID, index);
      printGridInfoKernel(gridID, index, false);
      const int projID = gridInqProj(gridID);
      if (projID != CDI_UNDEFID) printGridInfoKernel(projID, index, true);
    }
}

static void
printZaxisBoundsInfo(const int zaxisID, const int dig, const int levelsize, const double zinc, const char *zunits)
{
  fprintf(stdout, "%33s : ", "bounds");

  double level1 = zaxisInqLbound(zaxisID, 0);
  double level2 = zaxisInqUbound(zaxisID, 0);
  fprintf(stdout, "%.*g-%.*g", dig, level1, dig, level2);
  if (levelsize > 1)
    {
      level1 = zaxisInqLbound(zaxisID, levelsize - 1);
      level2 = zaxisInqUbound(zaxisID, levelsize - 1);
      fprintf(stdout, " to %.*g-%.*g", dig, level1, dig, level2);
      if (IS_NOT_EQUAL(zinc, 0)) fprintf(stdout, " by %.*g", dig, zinc);
    }
  fprintf(stdout, " %s", zunits);
  fprintf(stdout, "\n");
}

static void
printZaxisLevelInfo(const int levelsize, const int zaxisID, const int zaxistype, double zinc, const int dig, const char *zname, const char *zunits)
{
  double *levels = (double *) malloc((size_t) levelsize * sizeof(double));
  zaxisInqLevels(zaxisID, levels);

  if (!(zaxistype == ZAXIS_SURFACE && levelsize == 1 && fabs(levels[0]) <= 0))
    {
      const double zfirst = levels[0];
      const double zlast = levels[levelsize - 1];
      if (levelsize > 2)
        {
          zinc = (levels[levelsize - 1] - levels[0]) / (levelsize - 1);
          for (int levelID = 2; levelID < levelsize; ++levelID)
            if (fabs(fabs(levels[levelID] - levels[levelID - 1]) - zinc) > 0.001 * zinc)
              {
                zinc = 0;
                break;
              }
        }

      fprintf(stdout, "%33s : %.*g", zname, dig, zfirst);
      if (levelsize > 1)
        {
          fprintf(stdout, " to %.*g", dig, zlast);
          if (IS_NOT_EQUAL(zinc, 0)) fprintf(stdout, " by %.*g", dig, zinc);
        }
      fprintf(stdout, " %s", zunits);
      fprintf(stdout, "\n");
    }

  free(levels);
}

static void
printZaxisHybridInfo(const int zaxisID)
{
  char psname[CDI_MAX_NAME];
  psname[0] = 0;
  cdiZaxisInqKeyStr(zaxisID, CDI_KEY_PSNAME, CDI_MAX_NAME, psname);
  const int vctsize = zaxisInqVctSize(zaxisID);
  if (vctsize || psname[0])
    {
      fprintf(stdout, "%33s :", "available");
      if (vctsize) fprintf(stdout, " vct");
      if (psname[0]) fprintf(stdout, "  ps: %s", psname);
      fprintf(stdout, "\n");
    }
}

static void
printZaxisGenericInfo(const int ltype, const int zaxistype, const char *zaxisname)
{
  if (zaxistype == ZAXIS_GENERIC && ltype != 0)
    {
      fprintf(stdout, "%-12s (ltype=%3d)", zaxisname, ltype);
    }
  else
    {
      fprintf(stdout, "%-24s", zaxisname);
    }
}

static void
printZaxisReferenceInfo(const int zaxisID)
{
  const int number = zaxisInqNumber(zaxisID);
  if (number > 0)
    {
      fprintf(stdout, "%33s : ", "zaxis");
      fprintf(stdout, "number=%d\n", number);
    }

  unsigned char uuidOfVGrid[CDI_UUID_SIZE];
  zaxisInqUUID(zaxisID, uuidOfVGrid);
  if (!cdiUUIDIsNull(uuidOfVGrid))
    {
      char uuidOfVGridStr[37];
      cdiUUID2Str(uuidOfVGrid, uuidOfVGridStr);
      if (uuidOfVGridStr[0] != 0 && strlen(uuidOfVGridStr) == 36)
        {
          fprintf(stdout, "%33s : ", "uuid");
          fprintf(stdout, "%s\n", uuidOfVGridStr);
        }
    }
}

void
printZaxisInfo(int vlistID)
{
  char zaxisname[CDI_MAX_NAME], zname[CDI_MAX_NAME], zunits[CDI_MAX_NAME];

  const int nzaxis = vlistNzaxis(vlistID);
  for (int index = 0; index < nzaxis; index++)
    {
      int dig = 7;
      //______________________________________________________--
      double zinc = 0;
      const int zaxisID = vlistZaxis(vlistID, index);
      const int zaxistype = zaxisInqType(zaxisID);
      const int ltype = zaxisInqLtype(zaxisID);
      const int levelsize = zaxisInqSize(zaxisID);
      // int prec      = zaxisInqDatatype(zaxisID);
      // int dig = (prec == CDI_DATATYPE_FLT64) ? 15 : 7;

      zaxisName(zaxistype, zaxisname);
      zaxisInqName(zaxisID, zname);
      zaxisInqUnits(zaxisID, zunits);
      zunits[12] = 0;

      fprintf(stdout, "  %4d : ", vlistZaxisIndex(vlistID, zaxisID) + 1);

      printZaxisGenericInfo(ltype, zaxistype, zaxisname);

      fprintf(stdout, " :");

      fprintf(stdout, " levels=%d", levelsize);
      const bool zscalar = (levelsize == 1) ? zaxisInqScalar(zaxisID) : false;
      if (zscalar) fprintf(stdout, "  scalar");
      fprintf(stdout, "\n");

      if (zaxisInqLevels(zaxisID, NULL))
        {
          printZaxisLevelInfo(levelsize, zaxisID, zaxistype, zinc, dig, zname, zunits);
        }

      if (zaxisInqLbounds(zaxisID, NULL) && zaxisInqUbounds(zaxisID, NULL))
        {
          printZaxisBoundsInfo(zaxisID, dig, levelsize, zinc, zunits);
        }

      if (zaxistype == ZAXIS_HYBRID) printZaxisHybridInfo(zaxisID);

      if (zaxistype == ZAXIS_REFERENCE) printZaxisReferenceInfo(zaxisID);
    }
}

void
printSubtypeInfo(int vlistID)
{
  const int nsubtypes = vlistNsubtypes(vlistID);
  for (int index = 0; index < nsubtypes; index++)
    {
      const int subtypeID = vlistSubtype(vlistID, index);
      const int subtypesize = subtypeInqSize(subtypeID);
      // subtypePrint(subtypeID);
      fprintf(stdout, "  %4d : %-24s :", vlistSubtypeIndex(vlistID, subtypeID) + 1, "tiles");
      fprintf(stdout, " ntiles=%d", subtypesize);
      fprintf(stdout, "\n");
    }
}

static int
printDateTime(int ntimeout, const int64_t vdate, const int vtime)
{
  char vdatestr[32], vtimestr[32];

  if (ntimeout == 4)
    {
      ntimeout = 0;
      fprintf(stdout, "\n");
    }

  date2str(vdate, vdatestr, sizeof(vdatestr));
  time2str(vtime, vtimestr, sizeof(vtimestr));

  fprintf(stdout, " %s %s", vdatestr, vtimestr);

  return ++ntimeout;
}

#define NUM_TIMESTEP 60
#define MAX_DOTS 80

static int
printDot(int ndotout, int *nfact, int *ncout)
{
  // printf("ncout %d %d %d\n",*ncout, (*ncout)%(*nfact), *nfact);
  if ((*ncout) % (*nfact) == 0)
    {
      if (ndotout == MAX_DOTS)
        {
          *ncout = 0;
          ndotout = 0;
          fprintf(stdout, "\n   ");
          (*nfact) *= 10;
        }

      fprintf(stdout, ".");
      fflush(stdout);
      ndotout++;
    }

  (*ncout)++;

  return ndotout;
}

void
printTimesteps(int streamID, int taxisID, int verbose)
{
  int nrecs;
  struct datetime
  {
    int64_t vdate;
    int vtime;
    struct datetime *next;
  };
  struct datetime vdatetime[NUM_TIMESTEP];
  struct datetime *next_vdatetime = vdatetime;

  for (int i = 0; i < NUM_TIMESTEP - 1; ++i) vdatetime[i].next = &vdatetime[i + 1];
  vdatetime[NUM_TIMESTEP - 1].next = &vdatetime[0];

  int ntimeout = 0;
  int ndotout = 0;
  int nvdatetime = 0;
  int ncout = 0;
  int nfact = 1;
  int tsID = 0;

  while ((nrecs = streamInqTimestep(streamID, tsID)))
    {
      const int64_t vdate = taxisInqVdate(taxisID);
      const int vtime = taxisInqVtime(taxisID);

      if (verbose || tsID < NUM_TIMESTEP)
        {
          ntimeout = printDateTime(ntimeout, vdate, vtime);
        }
      else
        {
          if (tsID == 2 * NUM_TIMESTEP) fprintf(stdout, "\n   ");
          if (tsID >= 2 * NUM_TIMESTEP) ndotout = printDot(ndotout, &nfact, &ncout);

          if (nvdatetime < NUM_TIMESTEP)
            {
              vdatetime[nvdatetime].vdate = vdate;
              vdatetime[nvdatetime].vtime = vtime;
              nvdatetime++;
            }
          else
            {
              next_vdatetime->vdate = vdate;
              next_vdatetime->vtime = vtime;
              next_vdatetime = next_vdatetime->next;
            }
        }

      tsID++;
    }

  if (nvdatetime)
    {
      fprintf(stdout, "\n");

      ntimeout = 0;
      int toff = 0;
      if (tsID > 2 * NUM_TIMESTEP)
        {
          toff = tsID % 4;
          if (toff > 0) toff = 4 - toff;
          for (int i = 0; i < toff; ++i) next_vdatetime = next_vdatetime->next;
        }
      for (int i = toff; i < nvdatetime; ++i)
        {
          const int64_t vdate = next_vdatetime->vdate;
          const int vtime = next_vdatetime->vtime;
          ntimeout = printDateTime(ntimeout, vdate, vtime);
          next_vdatetime = next_vdatetime->next;
        }
    }
}
