#if defined (HAVE_CONFIG_H)
#  include "../src/config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <math.h>

#include "cdi.h"


#ifndef DBL_IS_NAN
#if  defined  (HAVE_ISNAN)
#  define DBL_IS_NAN(x)     (isnan(x))
#elif  defined  (FP_NAN)
#  define DBL_IS_NAN(x)     (fpclassify(x) == FP_NAN)
#else
#  define DBL_IS_NAN(x)     ((x) != (x))
#endif
#endif

#ifndef DBL_IS_EQUAL
/*#define DBL_IS_EQUAL(x,y) (!(x < y || y < x)) */
#  define DBL_IS_EQUAL(x,y) (DBL_IS_NAN(x)||DBL_IS_NAN(y)?(DBL_IS_NAN(x)&&DBL_IS_NAN(y)?1:0):!(x < y || y < x))
#endif

#ifndef IS_EQUAL
#  define IS_NOT_EQUAL(x,y) (x < y || y < x)
#  define IS_EQUAL(x,y)     (!IS_NOT_EQUAL(x,y))
#endif


void decode_date(int date, int *year, int *month, int *day);
void decode_time(int time, int *hour, int *minute);


int getopt(int argc, char *const argv[], const char *optstring);

extern char *optarg;
extern int optind, opterr, optopt;


char *Progname;

int DefaultFileType  = CDI_UNDEFID;
int DefaultDataType  = CDI_UNDEFID;
int DefaultByteorder = CDI_UNDEFID;

int Ztype  = COMPRESS_NONE;
int Zlevel = 0;


static void version(void)
{
  fprintf(stderr, "CDI version 1.7.0\n");
  cdiPrintVersion();
  fprintf(stderr, "\n");
/*
  1.0.0   6 Feb 2001 : initial version
  1.1.0  30 Jul 2003 : missing values implemented
  1.2.0   8 Aug 2003 : changes for CDI library version 0.7.0
  1.3.0  10 Feb 2004 : changes for CDI library version 0.7.9
  1.4.0   5 May 2004 : changes for CDI library version 0.8.1 (error handling)
  1.4.1  18 Sep 2004 : netCDF 2 support
  1.4.2  22 Mar 2005 : change level from int to double
  1.4.3  11 Apr 2005 : change date and time format to ISO
  1.5.0  22 Nov 2005 : IEG support
  1.5.1  21 Feb 2006 : add option -s for short info
  1.6.0   1 Aug 2006 : add option -z szip for SZIP compression of GRIB records
  1.6.1  27 Feb 2007 : short info with ltype for GENERIC zaxis
  1.6.2   3 Jan 2008 : changes for CDI library version 1.1.0 (compress)
  1.6.3  26 Mar 2008 : call streamDefTimestep also if ntsteps = 0 (buf fix)
  1.7.0  11 Apr 2008 : add option -z zip for deflate compression of netCDF4 variables
*/
}

static void usage(void)
{
  char *name;
  int id;

  fprintf(stderr, "usage : %s  [Option]  [ifile]  [ofile]\n", Progname);

  fprintf(stderr, "\n");
  fprintf(stderr, "  Options:\n");
  fprintf(stderr, "    -d             Print debugging information\n");
  fprintf(stderr, "    -f <format>    Format of the output file. (grb, nc, nc2, nc4, src, ext or ieg)\n");
  fprintf(stderr, "    -s             give short information if ofile is missing\n");
  fprintf(stderr, "    -t <table>     Parameter table name/file\n");
  fprintf(stderr, "                   Predefined tables: ");
  for ( id = 0; id < tableInqNumber(); id++ )
    if ( (name = tableInqNamePtr(id)) )
      fprintf(stderr, " %s", name);
  fprintf(stderr, "\n");

  fprintf(stderr, "    -V             Print version number\n");
  fprintf(stderr, "    -z szip        Compress GRIB records with szip\n");
  fprintf(stderr, "        zip        Deflate compression of netCDF4 variables\n");
  fprintf(stderr, "\n");
  fprintf(stderr, "  Report bugs to <Uwe.Schulzweida@zmaw.de>\n");
}


static void printInfo(int gridtype, int date, int time, int code, double level,
		      int datasize, int nmiss, double missval, const double *data)
{
  static int rec = 0;
  int i, ivals = 0, imiss = 0;
  int year, month, day, hour, minute;
  double arrmean, arrmin, arrmax;
 
  if ( ! rec )
  {
    fprintf(stdout, 
    "   Rec :       Date  Time Code  Level    Size    Miss :     Minimum        Mean     Maximum\n");
/*   ----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+ */
  }

  decode_date(date, &year, &month, &day);
  decode_time(time, &hour, &minute);

  fprintf(stdout, "%6d : %4.4d-%2.2d-%2.2d %2.2d:%2.2d %3d %7g ",
	  ++rec, year, month, day, hour, minute, code, level);

  fprintf(stdout, "%7d ", datasize);

  fprintf(stdout, "%7d :", nmiss);
  /*
  if ( gridtype == GRID_SPECTRAL )
    {
      fprintf(stdout, "            %#12.5g\n", data[0]);
    }
  else
  */
    {
      if ( nmiss > 0 )
	{
	  arrmean = 0;
	  arrmin  =  1e50;
	  arrmax  = -1e50;
	  for ( i = 0; i < datasize; i++ )
	    {
	      if ( !DBL_IS_EQUAL(data[i], missval) )
		{
		  if ( data[i] < arrmin ) arrmin = data[i];
		  if ( data[i] > arrmax ) arrmax = data[i];
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
	  arrmin  = data[0];
	  arrmax  = data[0];
	  for ( i = 1; i < datasize; i++ )
	    {
	      if ( data[i] < arrmin ) arrmin = data[i];
	      if ( data[i] > arrmax ) arrmax = data[i];
	      arrmean += data[i];
	    }
	}

      if ( datasize > 0 ) arrmean /= datasize;

      fprintf(stdout, "%#12.5g%#12.5g%#12.5g\n", arrmin, arrmean, arrmax);

      if ( imiss != nmiss && nmiss > 0 )
	fprintf(stdout, "Found %d of %d missing values!\n", imiss, nmiss);
    }
}


void printFiletype(int streamID, int vlistID)
{
  int filetype;

  filetype = streamInqFiletype(streamID);

  switch ( filetype )
    {
    case FILETYPE_GRB:
      printf("GRIB");
      break;
    case FILETYPE_NC:
      printf("netCDF");
      break;
    case FILETYPE_NC2:
      printf("netCDF2");
      break;
    case FILETYPE_NC4:
      printf("netCDF4");
      break;
    case FILETYPE_SRV:
      printf("SERVICE");
      break;
    case FILETYPE_EXT:
      printf("EXTRA");
      break;
    case FILETYPE_IEG:
      printf("IEG");
      break;
    default:
      printf("  File format: unsupported filetype %d" , filetype);
    }

  if ( filetype == FILETYPE_SRV || filetype == FILETYPE_EXT || filetype == FILETYPE_IEG )
    {
      switch ( streamInqByteorder(streamID) )
	{
	case CDI_BIGENDIAN:
	  printf("  BIGENDIAN"); break;
	case CDI_LITTLEENDIAN:
	  printf("  LITTLEENDIAN"); break;
	default:
	  printf("  byteorder: %d undefined", streamInqByteorder(streamID)); break;
	}
    }

  if ( filetype == FILETYPE_GRB || filetype == FILETYPE_NC4 )
    {
      int nvars, varID;
      int ztype;

      nvars = vlistNvars(vlistID);

      for ( varID = 0; varID < nvars; varID++ )
	{
	  ztype = vlistInqVarZtype(vlistID, varID);
	  if ( ztype )
	    {
	      if ( ztype == COMPRESS_SZIP )
		printf(" SZIP");
	      else if ( ztype == COMPRESS_ZIP )
		printf(" ZIP");

	      break;
	    }
	}
    }

  printf("\n");
}


static void printGridInfo(int vlistID)
{
  static char func[] = "printGridInfo";
  int ngrids, index;
  int gridID, gridtype, trunc, gridsize, xsize, ysize;
  int nbyte0;
  char xname[128], yname[128], xunits[128], yunits[128];

  ngrids = vlistNgrids(vlistID);
  for ( index = 0; index < ngrids; index++ )
    {
      gridID   = vlistGrid(vlistID, index);
      gridtype = gridInqType(gridID);
      trunc    = gridInqTrunc(gridID);
      gridsize = gridInqSize(gridID);
      xsize    = gridInqXsize(gridID);
      ysize    = gridInqYsize(gridID);
      gridInqXname(gridID, xname);
      gridInqYname(gridID, yname);
      gridInqXunits(gridID, xunits);
      gridInqYunits(gridID, yunits);

      /*	  nbyte0   = fprintf(stdout, "  %4d : %-23s : ",*/
      nbyte0   = fprintf(stdout, "  %4d : %-12s > ",
			 gridID+1, gridNamePtr(gridtype));

      if ( gridtype == GRID_LONLAT   ||
	   gridtype == GRID_LAEA ||
	   gridtype == GRID_SINUSOIDAL ||
	   gridtype == GRID_GAUSSIAN ||
	   gridtype == GRID_GAUSSIAN_REDUCED )
	{
	  double xfirst = 0.0, xlast = 0.0;
	  double yfirst = 0.0, ylast = 0.0;
	  double xinc = 0.0, yinc = 0.0;
	  
	  yfirst = gridInqYval(gridID, 0);
	  ylast  = gridInqYval(gridID, ysize-1);
	  yinc   = gridInqYinc(gridID);
	  if ( gridtype == GRID_GAUSSIAN_REDUCED )
	    {
	      fprintf(stdout, "size : dim = %d  nlat = %d\n", gridsize, ysize);
	      fprintf(stdout, "%*s", nbyte0, "");
	      fprintf(stdout, "longitude : reduced\n");
	    }
	  else
	    {
	      xfirst = gridInqXval(gridID, 0);
	      xlast  = gridInqXval(gridID, xsize-1);
	      xinc   = gridInqXinc(gridID);
	      fprintf(stdout, "size      : dim = %d  nlon = %d  nlat = %d\n", gridsize, xsize, ysize);
	      fprintf(stdout, "%*s", nbyte0, "");
	      fprintf(stdout, "%-9s : first = %.9g  last = %.9g", xname, xfirst, xlast);
	      if ( IS_NOT_EQUAL(xinc, 0) )
		fprintf(stdout, "  inc = %.9g", xinc);
	      fprintf(stdout, "  %s", xunits);
	      if ( gridIsCircular(gridID) )
		fprintf(stdout, "  circular");
	      fprintf(stdout, "\n");
	    }
	  fprintf(stdout, "%*s", nbyte0, "");
	  fprintf(stdout, "%-9s : first = %.9g  last = %.9g", yname, yfirst, ylast);
	  if ( IS_NOT_EQUAL(yinc, 0) && 
	       (gridtype == GRID_LONLAT || gridtype == GRID_SINUSOIDAL || gridtype == GRID_LAEA) )
	    fprintf(stdout, "  inc = %.9g", yinc);
	  fprintf(stdout, "  %s", yunits);
	  fprintf(stdout, "\n");
	  
	  if ( gridIsRotated(gridID) )
	    {
	      double lonpole, latpole;
	      lonpole = gridInqXpole(gridID);
	      latpole = gridInqYpole(gridID);
	      fprintf(stdout, "%*s", nbyte0, "");
	      fprintf(stdout, "northpole : lon = %.9g  lat = %.9g\n", lonpole, latpole);
	    }
		
	  if ( gridInqXbounds(gridID, NULL) || gridInqYbounds(gridID, NULL) )
	    {
	      fprintf(stdout, "%*s", nbyte0, "");
	      fprintf(stdout, "available :");
	      if ( gridInqXbounds(gridID, NULL) ) fprintf(stdout, " xbounds");
	      if ( gridInqYbounds(gridID, NULL) ) fprintf(stdout, " ybounds");
	      fprintf(stdout, "\n");
	    }
	  
	  if ( gridtype == GRID_LAEA )
	    {
	      double a, lon_0, lat_0;
	      gridInqLaea(gridID, &a, &lon_0, &lat_0);
	      fprintf(stdout, "%*s", nbyte0, "");
	      fprintf(stdout, "projpar   : a = %g  lon_0 = %g  lat_0 = %g\n", a, lon_0, lat_0);
	    }
	}
      else if ( gridtype == GRID_SPECTRAL )
	{
	  fprintf(stdout, "size      : dim = %d  truncation = %d  spc = %d\n",
		  gridsize, trunc, gridsize/2);
	}
      else if ( gridtype == GRID_GME )
	{
	  int ni, nd;
	  ni = gridInqGMEni(gridID);
	  nd = gridInqGMEnd(gridID);
	  fprintf(stdout, "size      : dim = %d  nd = %d  ni = %d\n", gridsize, nd, ni);
	}
      else if ( gridtype == GRID_CURVILINEAR || gridtype == GRID_CELL )
	{
	  if ( gridtype == GRID_CURVILINEAR )
	    fprintf(stdout, "size      : dim = %d  nx = %d  ny = %d\n", gridsize, xsize, ysize);
	  else
	    fprintf(stdout, "size      : dim = %d  nvertex = %d\n", gridsize, gridInqNvertex(gridID));

	  if ( gridInqXvals(gridID, NULL) && gridInqYvals(gridID, NULL) )
	    {
	      int i;
	      double *xvals, *yvals;
	      double xfirst, xlast, yfirst, ylast;
	      xvals = (double *) malloc(gridsize*sizeof(double));
	      yvals = (double *) malloc(gridsize*sizeof(double));

	      gridInqXvals(gridID, xvals);
	      gridInqYvals(gridID, yvals);

	      xfirst = xvals[0];
	      xlast  = xvals[0];
	      yfirst = yvals[0];
	      ylast  = yvals[0];
	      for ( i = 1; i < gridsize; i++ )
		{
		  if ( xvals[i] < xfirst ) xfirst = xvals[i];
		  if ( xvals[i] > xlast )  xlast  = xvals[i];
		  if ( yvals[i] < yfirst ) yfirst = yvals[i];
		  if ( yvals[i] > ylast )  ylast  = yvals[i];
		}

	      fprintf(stdout, "%*s", nbyte0, "");
	      fprintf(stdout, "%-9s : min = %.9g  max = %.9g  %s", xname, xfirst, xlast, xunits);
	      if ( gridIsCircular(gridID) )
		fprintf(stdout, "  circular");
	      fprintf(stdout, "\n");
	      fprintf(stdout, "%*s", nbyte0, "");
	      fprintf(stdout, "%-9s : min = %.9g  max = %.9g  %s\n", yname, yfirst, ylast, yunits);
	      
	      free(xvals);
	      free(yvals);
	    }
	}
      else if ( gridtype == GRID_LCC )
	{
	  double originLon, originLat, lonParY, lat1, lat2, xincm, yincm;
	  int projflag, scanflag;

	  gridInqLCC(gridID, &originLon, &originLat, &lonParY, &lat1, &lat2, &xincm, &yincm,
		     &projflag, &scanflag);

	  fprintf(stdout, "size      : dim = %d  nx = %d  ny = %d  ", gridsize, xsize, ysize);
	  if ( (projflag&128) == 0 )
	    fprintf(stdout, "North Pole\n");
	  else
	    fprintf(stdout, "South Pole\n");
	  fprintf(stdout, "%*s", nbyte0, "");	  
	  fprintf(stdout, "            originLon = %g  originLat = %g  lonParY = %g\n",
		  originLon, originLat, lonParY);
	  fprintf(stdout, "%*s", nbyte0, "");	  
	  fprintf(stdout, "            lat1 = %g  lat2 = %g  xinc = %gm  yinc = %gm\n", 
		  lat1, lat2, xincm, yincm);
	}
      else /* if ( gridtype == GRID_GENERIC ) */
	{
	  if ( ysize == 0 )
	    fprintf(stdout, "size      : dim = %d\n", gridsize);
	  else
	    {
	      fprintf(stdout, "size      : dim = %d  nx = %d  ny = %d\n", gridsize, xsize, ysize);
	      if ( gridIsCircular(gridID) )
		{
		  fprintf(stdout, "%*s", nbyte0, "");	  
		  fprintf(stdout, "longitude :  circular\n");
		}
	    }
	}

      if ( gridtype == GRID_CURVILINEAR || gridtype == GRID_CELL ||
	   gridtype == GRID_GENERIC || gridtype == GRID_LCC )
	{
	  if ( gridInqXvals(gridID, NULL) || gridInqYvals(gridID, NULL) || gridHasArea(gridID) ||
	       gridInqXbounds(gridID, NULL) || gridInqYbounds(gridID, NULL) )
	    {
	      fprintf(stdout, "%*s", nbyte0, "");
	      fprintf(stdout, "available :");
	      if ( gridInqXvals(gridID, NULL) )   fprintf(stdout, " xvals");
	      if ( gridInqYvals(gridID, NULL) )   fprintf(stdout, " yvals");
	      if ( gridInqXbounds(gridID, NULL) ) fprintf(stdout, " xbounds");
	      if ( gridInqYbounds(gridID, NULL) ) fprintf(stdout, " ybounds");
	      if ( gridHasArea(gridID) )          fprintf(stdout, " area");
	      fprintf(stdout, "\n");
	    }
	}
    }
}


static void printShortinfo(int streamID, int vlistID, int vardis)
{
  int varID;
  int gridsize = 0;
  int gridID, zaxisID, code;
  int zaxistype, ltype;
  int vdate, vtime;
  int nrecs, nvars, nzaxis, ntsteps;
  int levelID, levelsize;
  int tsID, ntimeout;
  int timeID, taxisID;
  int nbyte, nbyte0;
  int index;
  char varname[128];
  char longname[128];
  char units[128];
  double level;
  char *modelptr, *instptr;
  int prec;
  int year, month, day, hour, minute;
  char pstr[4];

      printf("   File format: ");
      printFiletype(streamID, vlistID);

      if ( vardis )
	fprintf(stdout,
		"   Var : Institut Source  Table   Time   Typ  Grid Size Num  Levels Num Varname\n");
      else
	fprintf(stdout,
		"   Var : Institut Source  Table Code   Time   Typ  Grid Size Num  Levels Num\n");

      nvars = vlistNvars(vlistID);

      for ( varID = 0; varID < nvars; varID++ )
	{
	  code    = vlistInqVarCode(vlistID, varID);
	  gridID  = vlistInqVarGrid(vlistID, varID);
	  zaxisID = vlistInqVarZaxis(vlistID, varID);

	  if ( vardis ) vlistInqVarName(vlistID, varID, varname);

	  gridsize = gridInqSize(gridID);

	  fprintf(stdout, "%6d : ", varID + 1);

	  instptr = institutInqNamePtr(vlistInqVarInstitut(vlistID, varID));
	  if ( instptr )
	    fprintf(stdout, "%-9s", instptr);
	  else
	    fprintf(stdout, "unknown  ");

	  modelptr = modelInqNamePtr(vlistInqVarModel(vlistID, varID));
	  if ( modelptr )
	    fprintf(stdout, "%-9s", modelptr);
	  else
	    fprintf(stdout, "unknown  ");

	  fprintf(stdout, "%4d", tableInqNum(vlistInqVarTable(vlistID, varID)));

	  if ( ! vardis )
	    fprintf(stdout, "%5d", code);

	  timeID = vlistInqVarTime(vlistID, varID);
	  if ( timeID == TIME_CONSTANT )
	    fprintf(stdout, " constant");
	  else
	    fprintf(stdout, " variable");

	  prec = vlistInqVarDatatype(vlistID, varID);
	  if      ( prec == DATATYPE_PACK   ) strcpy(pstr, "P0");
	  else if ( prec > 0 && prec <= 32  ) sprintf(pstr, "P%d", prec);
	  else if ( prec == DATATYPE_FLT32  ) strcpy(pstr, "F32");
	  else if ( prec == DATATYPE_FLT64  ) strcpy(pstr, "F64");
	  else if ( prec == DATATYPE_INT8   ) strcpy(pstr, "I8");
	  else if ( prec == DATATYPE_INT16  ) strcpy(pstr, "I16");
	  else if ( prec == DATATYPE_INT32  ) strcpy(pstr, "I32");
	  else if ( prec == DATATYPE_UINT8  ) strcpy(pstr, "U8");
	  else if ( prec == DATATYPE_UINT16 ) strcpy(pstr, "U16");
	  else if ( prec == DATATYPE_UINT32 ) strcpy(pstr, "U32");
	  else                                strcpy(pstr, "-1");

	  fprintf(stdout, " %-3s", pstr);

	  if ( vlistInqVarZtype(vlistID, varID) == COMPRESS_NONE )
	    fprintf(stdout, " ");
	  else
	    fprintf(stdout, "z");

	  fprintf(stdout, "%9d", gridsize);

	  fprintf(stdout, " %3d ", gridID + 1);

	  levelsize = zaxisInqSize(zaxisID);
	  fprintf(stdout, " %6d", levelsize);
	  fprintf(stdout, " %3d", zaxisID + 1);
	    
	  if ( vardis )
	    fprintf(stdout, "  %-10s", varname);

	  fprintf(stdout, "\n");
	}

      fprintf(stdout, "   Horizontal grids :\n");
      printGridInfo(vlistID);

      nzaxis = vlistNzaxis(vlistID);
      fprintf(stdout, "   Vertical grids :\n");
      for ( index = 0; index < nzaxis; index++)
	{
	  zaxisID   = vlistZaxis(vlistID, index);
	  zaxistype = zaxisInqType(zaxisID);
	  ltype     = zaxisInqLtype(zaxisID);
	  levelsize = zaxisInqSize(zaxisID);
	  /* zaxisInqLongname(zaxisID, longname); */
	  zaxisName(zaxistype, longname);
	  longname[16] = 0;
	  zaxisInqUnits(zaxisID, units);
	  units[12] = 0;
	  if ( zaxistype == ZAXIS_GENERIC && ltype != 0 )
	    nbyte0    = fprintf(stdout, "  %4d : %-10s  (ltype=%3d) : ", zaxisID+1, longname, ltype);
	  else
	    nbyte0    = fprintf(stdout, "  %4d : %-16s  %5s : ", zaxisID+1, longname, units);
	  nbyte = nbyte0;
	  for ( levelID = 0; levelID < levelsize; levelID++ )
	    {
	      if ( nbyte > 80 )
		{
		  fprintf(stdout, "\n");
		  fprintf(stdout, "%*s", nbyte0, "");
		  nbyte = nbyte0;
		}
	      level = zaxisInqLevel(zaxisID, levelID);
	      nbyte += fprintf(stdout, "%.9g ", level);
	    }
	  fprintf(stdout, "\n");
	  if ( zaxisInqLbounds(zaxisID, NULL) && zaxisInqUbounds(zaxisID, NULL) )
	    {
	      double level1, level2;
	      nbyte = nbyte0;
	      nbyte0 = fprintf(stdout, "%32s : ", "bounds");
	      for ( levelID = 0; levelID < levelsize; levelID++ )
		{
		  if ( nbyte > 80 )
		    {
		      fprintf(stdout, "\n");
		      fprintf(stdout, "%*s", nbyte0, "");
		      nbyte = nbyte0;
		    }
		  level1 = zaxisInqLbound(zaxisID, levelID);
		  level2 = zaxisInqUbound(zaxisID, levelID);
		  nbyte += fprintf(stdout, "%.9g-%.9g ", level1, level2);
		}
	      fprintf(stdout, "\n");
	    }
	}

      taxisID = vlistInqTaxis(vlistID);
      ntsteps = vlistNtsteps(vlistID);

      if ( ntsteps != 0 )
	{
	  if ( ntsteps == CDI_UNDEFID )
	    fprintf(stdout, "   Time axis :  unlimited steps\n");
	  else
	    fprintf(stdout, "   Time axis :  %d step%s\n", ntsteps, ntsteps == 1 ? "" : "s");

	  if ( taxisID != CDI_UNDEFID )
	    {
	      int calendar, unit;

	      if ( taxisInqType(taxisID) == TAXIS_RELATIVE )
		{
		  vdate = taxisInqRdate(taxisID);
		  vtime = taxisInqRtime(taxisID);

		  decode_date(vdate, &year, &month, &day);
		  decode_time(vtime, &hour, &minute);

		  fprintf(stdout, "     RefTime = %4.4d-%2.2d-%2.2d %2.2d:%2.2d",
			  year, month, day, hour, minute);
		      
		  unit = taxisInqTunit(taxisID);
		  if ( unit != CDI_UNDEFID )
		    {
		      if ( unit == TUNIT_YEAR )
			fprintf(stdout, "  Units = years");
		      else if ( unit == TUNIT_MONTH )
			fprintf(stdout, "  Units = months");
		      else if ( unit == TUNIT_DAY )
			fprintf(stdout, "  Units = days");
		      else if ( unit == TUNIT_HOUR )
			fprintf(stdout, "  Units = hours");
		      else if ( unit == TUNIT_MINUTE )
			fprintf(stdout, "  Units = minutes");
		      else if ( unit == TUNIT_SECOND )
			fprintf(stdout, "  Units = seconds");
		      else
			fprintf(stdout, "  Units = unknown");
		    }
	      
		  calendar = taxisInqCalendar(taxisID);
		  if ( calendar != CDI_UNDEFID )
		    {
		      if ( calendar == CALENDAR_STANDARD )
			fprintf(stdout, "  Calendar = STANDARD");
		      else if ( calendar == CALENDAR_PROLEPTIC )
			fprintf(stdout, "  Calendar = PROLEPTIC");
		      else if ( calendar == CALENDAR_360DAYS )
			fprintf(stdout, "  Calendar = 360DAYS");
		      else if ( calendar == CALENDAR_365DAYS )
			fprintf(stdout, "  Calendar = 365DAYS");
		      else if ( calendar == CALENDAR_366DAYS )
			fprintf(stdout, "  Calendar = 366DAYS");
		      else
			fprintf(stdout, "  Calendar = unknown");
		    }

		  fprintf(stdout, "\n");
		}
	    }

	  fprintf(stdout, "   YYYY-MM-DD hh:mm   YYYY-MM-DD hh:mm   YYYY-MM-DD hh:mm   YYYY-MM-DD hh:mm\n");

	  ntimeout = 0;
	  tsID = 0;
	  while ( (nrecs = streamInqTimestep(streamID, tsID)) )
	    {
	      if ( ntimeout == 4 )
		{
		  ntimeout = 0;
		  fprintf(stdout, "\n");
		}

	      vdate = taxisInqVdate(taxisID);
	      vtime = taxisInqVtime(taxisID);

	      decode_date(vdate, &year, &month, &day);
	      decode_time(vtime, &hour, &minute);

	      fprintf(stdout, " %6.4d-%2.2d-%2.2d %2.2d:%2.2d", year, month, day, hour, minute);
	      ntimeout++;
	      tsID++;
	    }
	  fprintf(stdout, "\n");
	}
}


#undef  IsBigendian
#define IsBigendian()  ( u_byteorder.c[sizeof(long) - 1] )


static void setDefaultDataType(char *datatypestr)
{
  static union {unsigned long l; unsigned char c[sizeof(long)];} u_byteorder = {1};
  int nbits = -1;
  enum {D_UINT, D_INT, D_FLT};
  int dtype = -1;

  if      ( *datatypestr == 'i' || *datatypestr == 'I' )
    {
      dtype = D_INT;
      datatypestr++;
    }
  else if ( *datatypestr == 'u' || *datatypestr == 'U' )
    {
      dtype = D_UINT;
      datatypestr++;
    }
  else if ( *datatypestr == 'f' || *datatypestr == 'F' )
    {
      dtype = D_FLT;
      datatypestr++;
    }

  if ( isdigit((int) *datatypestr) )
    {
      nbits = atoi(datatypestr);
      if ( nbits < 10 )
	datatypestr += 1;
      else
	datatypestr += 2;

      if ( dtype == -1 )
	{
	  if      ( nbits > 0 && nbits < 32 ) DefaultDataType = nbits;
	  else if ( nbits == 32 )
	    {
	      if ( DefaultFileType == FILETYPE_GRB )
		DefaultDataType = DATATYPE_PACK32;
	      else
		DefaultDataType = DATATYPE_FLT32;
	    }
	  else if ( nbits == 64 ) DefaultDataType = DATATYPE_FLT64;
	  else
	    {
	      fprintf(stderr, "Unsupported number of bits %d!\n", nbits);
	      fprintf(stderr, "Use 32/64 for filetype nc, srv, ext, ieg and 1-32 for grb.\n");
	      exit(EXIT_FAILURE);
	    }
	}
      else
	{
	  if ( dtype == D_INT )
	    {
	      if      ( nbits ==  8 ) DefaultDataType = DATATYPE_INT8;
	      else if ( nbits == 16 ) DefaultDataType = DATATYPE_INT16;
	      else if ( nbits == 32 ) DefaultDataType = DATATYPE_INT32;
	      else
		{
		  fprintf(stderr, "Unsupported number of bits = %d for datatype INT!\n", nbits);
		  exit(EXIT_FAILURE);
		}
	    }
	  /*
	  else if ( dtype == D_UINT )
	    {
	      if      ( nbits ==  8 ) DefaultDataType = DATATYPE_UINT8;
	      else
		{
		  fprintf(stderr, "Unsupported number of bits = %d for datatype UINT!\n", nbits);
		  exit(EXIT_FAILURE);
		}
	    }
	  */
	  else if ( dtype == D_FLT )
	    {
	      if      ( nbits == 32 ) DefaultDataType = DATATYPE_FLT32;
	      else if ( nbits == 64 ) DefaultDataType = DATATYPE_FLT64;
	      else
		{
		  fprintf(stderr, "Unsupported number of bits = %d for datatype FLT!\n", nbits);
		  exit(EXIT_FAILURE);
		}
	    }
	}
    }

  if ( *datatypestr == 'l' || *datatypestr == 'L' )
    {
      if ( IsBigendian() ) DefaultByteorder = CDI_LITTLEENDIAN;
      datatypestr++;
    }

  if ( *datatypestr == 'b' || *datatypestr == 'B' )
    {
      if ( ! IsBigendian() ) DefaultByteorder = CDI_BIGENDIAN;
      datatypestr++;
    }
}


static void setDefaultFileType(char *filetypestr)
{
  if ( filetypestr )
    {
      char *ftstr = filetypestr;

      if      ( strncmp(filetypestr, "grb", 3)  == 0 ) { ftstr += 3; DefaultFileType = FILETYPE_GRB; }
      else if ( strncmp(filetypestr, "nc2", 3)  == 0 ) { ftstr += 3; DefaultFileType = FILETYPE_NC2; }
      else if ( strncmp(filetypestr, "nc4", 3)  == 0 ) { ftstr += 3; DefaultFileType = FILETYPE_NC4; }
      else if ( strncmp(filetypestr, "nc",  2)  == 0 ) { ftstr += 2; DefaultFileType = FILETYPE_NC;  }
      else if ( strncmp(filetypestr, "srv", 3)  == 0 ) { ftstr += 3; DefaultFileType = FILETYPE_SRV; }
      else if ( strncmp(filetypestr, "ext", 3)  == 0 ) { ftstr += 3; DefaultFileType = FILETYPE_EXT; }
      else if ( strncmp(filetypestr, "ieg", 3)  == 0 ) { ftstr += 3; DefaultFileType = FILETYPE_IEG; }
      else
	{
	  fprintf(stderr, "Unsupported filetype %s!\n", filetypestr);
	  fprintf(stderr, "Available filetypes: grb, nc, nc2, nc4, srv, ext and ieg\n");
	  exit(EXIT_FAILURE);
	}

      if ( DefaultFileType != CDI_UNDEFID && *ftstr != 0 )
	{
	  if ( *ftstr == '_' )
	    {
	      ftstr++;

	      setDefaultDataType(ftstr);
	    }
	  else
	    {
	      fprintf(stderr, "Unexpected character >%c< in file type >%s<!\n", *ftstr, filetypestr);
	      fprintf(stderr, "Use format[_nbits] with:\n");
	      fprintf(stderr, "    format = grb, nc, nc2, nc4, srv, ext or ieg\n");
	      fprintf(stderr, "    nbits  = 32/64 for nc, nc2, nc4, srv, ext, ieg; 1 - 32 for grb\n");
	      exit(EXIT_FAILURE);
	    }
	}
    }
}


int handle_error(int cdiErrno, const char *fmt, ...)
{
  va_list args;
	
  va_start(args, fmt);

  printf("\n");
  vfprintf(stderr, fmt, args);
   fprintf(stderr, "\n");

  va_end(args);

  fprintf(stderr, "%s\n", cdiStringError(cdiErrno));

  return (cdiErrno);
}


void defineCompress(const char *arg)
{
  size_t len = strlen(arg);

  if      ( strncmp(arg, "szip", len) == 0 )
    {
      Ztype = COMPRESS_SZIP;
    }
  else if ( strncmp(arg, "gzip", len) == 0 )
    {
      Ztype = COMPRESS_GZIP;
      Zlevel = 6;
    }
  else if ( strncmp(arg, "zip", len) == 0 )
    {
      Ztype = COMPRESS_ZIP;
      Zlevel = 1;
    }
  else
    fprintf(stderr, "Compression %s unsupported!\n", arg);  
}



int main(int argc, char *argv[])
{
  int c;
  char *fname1 = NULL;
  char *fname2 = NULL;
  char *rTable = NULL;
  char *wTable = NULL;
  int Move = 0;
  int Record = 0;
  int Debug = 0;
  int Quiet  = 0;
  int Vardis = 0;
  int Version = 0;
  int Longinfo = 0;
  int Shortinfo = 0;
  int varID;
  int itableID = CDI_UNDEFID, otableID = CDI_UNDEFID;
  int Info = 1;

  Progname = strrchr(argv[0], '/');
  if (Progname == 0) Progname = argv[0];
  else               Progname++;

  while ( (c = getopt(argc, argv, "f:t:w:z:cdhlmqrsvVZ")) != EOF )
    {
      switch (c)
	{
	case 'd':
	  Debug = 1;
	  break;
	case 'f':
	  setDefaultFileType(optarg);
	  break;
	case 'r':
	  Record = 1;
	  break;
	case 'l':
	  Longinfo = 1;
	  break;
	case 'm':
	  Move = 1;
	  break;
	case 'v':
	  Vardis = 1;
	  break;
	case 'V':
	  Version = 1;
	  break;
	case 'q':
	  Quiet = 1;
	  break;
	case 's':
	  Shortinfo = 1;
	  break;
	case 't':
	  rTable = optarg;
	  break;
	case 'w':
	  wTable = optarg;
	  break;
	case 'z':
	  defineCompress(optarg);
	  break;
	case 'h':
	  usage();
	  exit (0);
	}
    }

  if ( optind < argc ) fname1 = argv[optind++];
  if ( optind < argc ) fname2 = argv[optind++];
  if ( optind < argc ) 
    {
       fprintf(stderr, "optind: %d argc: %d\n", optind, argc);
    }

  if ( Debug || Version ) version();

  if ( Debug ) cdiDebug(Debug);

  if ( rTable )
    {
      itableID = tableInq(-1, 0, rTable);
      otableID = itableID;
    }

  if ( fname1 == NULL && ! (Debug || Version) )
    {
      usage();
      exit (0);
    }

  if ( fname1 )
    {
      double *data = NULL;
      double missval;
      double level;
      int nmiss;
      int datasize = 0;
      int streamID1 = CDI_UNDEFID;
      int streamID2 = CDI_UNDEFID;
      int filetype;
      int gridID, zaxisID, code;
      int vdate, vtime;
      int nrecs, nvars;
      int levelID, levelsize;
      int nts = 0;
      int gridsize = 0;
      int recID;
      int tsID;
      int ntsteps = 0;
      int taxisID;
      int gridtype;
      int vlistID1, vlistID2 = CDI_UNDEFID;

      streamID1 = streamOpenRead(fname1);
      if ( streamID1 < 0 )
	return (handle_error(streamID1, "Open failed on %s", fname1));

      vlistID1 = streamInqVlist(streamID1);

      if ( Longinfo )
	{
	  int ngrids, nzaxis;
	  vlistPrint(vlistID1);
	  ngrids = vlistNgrids(vlistID1);
	  nzaxis = vlistNzaxis(vlistID1);
	  for ( gridID = 0; gridID < ngrids; gridID++ ) gridPrint(gridID, 1);
	  for ( zaxisID = 0; zaxisID < nzaxis; zaxisID++ ) zaxisPrint(zaxisID);
	}

      nvars   = vlistNvars(vlistID1);
      taxisID = vlistInqTaxis(vlistID1);
      ntsteps = vlistNtsteps(vlistID1);

      if ( Debug ) fprintf(stderr, "nvars = %d\n", nvars);
      if ( Debug ) fprintf(stderr, "ntsteps = %d\n", ntsteps);

      if ( fname2 ) vlistID2 = vlistDuplicate(vlistID1);
	  
      for ( varID = 0; varID < nvars; varID++)
	{
	  gridID   = vlistInqVarGrid(vlistID1, varID);
	  gridsize = gridInqSize(gridID);
	  if ( gridsize > datasize ) datasize = gridsize;
	  if ( fname2 )
	    {
	      if ( DefaultDataType != CDI_UNDEFID )
		vlistDefVarDatatype(vlistID2, varID, DefaultDataType);
	    }
	}

      if ( fname2 )
	{
	  Info = 0;
	  filetype = streamInqFiletype(streamID1);

	  if ( DefaultFileType != CDI_UNDEFID )
	    filetype = DefaultFileType;

	  streamID2 = streamOpenWrite(fname2, filetype);
	  if ( streamID2 < 0 )
	    return (handle_error(streamID2, "Open failed on %s", fname2));

	  if ( DefaultByteorder != CDI_UNDEFID )
	    streamDefByteorder(streamID2, DefaultByteorder);

	  if ( Ztype != COMPRESS_NONE )
	    {
	      streamDefZtype(streamID2, Ztype);
	      streamDefZlevel(streamID2, Zlevel);
	    }

	  streamDefVlist(streamID2, vlistID2);

	  if ( otableID == CDI_UNDEFID ) otableID = itableID;
	}

      data = (double *) malloc(datasize*sizeof(double));

      /*
	nts = cdiInqTimeSize(streamID1);
      */
      if ( Debug )
	printf("nts = %d\n", nts);

      if ( Debug )
	printf("streamID1 = %d, streamID2 = %d\n", streamID1, streamID2);

      if ( Shortinfo )
	{
	  Info = 0;
	  printShortinfo(streamID1, vlistID1, Vardis);
	}

      tsID = 0;
      if ( Info || fname2 )
      while ( (nrecs = streamInqTimestep(streamID1, tsID)) > 0 )
	{
	  if ( fname2 /* && ntsteps != 0*/ )
	    streamDefTimestep(streamID2, tsID);

	  vdate = taxisInqVdate(taxisID);
	  vtime = taxisInqVtime(taxisID);

	  if ( Debug )
	    fprintf(stdout, "tsID = %d nrecs = %d date = %d time = %d\n", tsID, nrecs, vdate, vtime);

	  if ( Record )
	    {
	      for ( recID = 0; recID < nrecs; recID++ )
		{
		  streamInqRecord(streamID1, &varID, &levelID);
		  streamReadRecord(streamID1, data, &nmiss);

		  gridID  = vlistInqVarGrid(vlistID1, varID);
		  zaxisID = vlistInqVarZaxis(vlistID1, varID);
		  code    = vlistInqVarCode(vlistID1, varID);
		  /*
		  printf("varID=%d, code=%d, gridID=%d, zaxisID=%d levelID=%d\n",
			 varID, code, gridID, zaxisID, levelID);
		  */
		  gridtype = gridInqType(gridID);
		  gridsize = gridInqSize(gridID);
		  level    = zaxisInqLevel(zaxisID, levelID);
		  missval  = vlistInqVarMissval(vlistID1, varID);
		  
		  if ( Info )
		    printInfo(gridtype, vdate, vtime, code, level, gridsize, nmiss, missval, data);

		  if ( fname2 )
		    {
		      streamDefRecord(streamID2, varID, levelID);
		      if ( Move )
			streamCopyRecord(streamID2, streamID1);
		      else
			streamWriteRecord(streamID2, data, nmiss);
		    }
	      	}
	    }
	  else
	    {
	      for ( varID = 0; varID < nvars; varID++ )
		{
		  if ( vlistInqVarTime(vlistID1, varID) == TIME_CONSTANT && tsID > 0 ) continue;

		  gridID  = vlistInqVarGrid(vlistID1, varID);
		  zaxisID = vlistInqVarZaxis(vlistID1, varID);
		  code    = vlistInqVarCode(vlistID1, varID);
		  if ( Debug )
		    fprintf(stdout, "varID = %d code = %d gridID = %d zaxisID = %d\n",
			    varID, code, gridID, zaxisID);

		  gridtype = gridInqType(gridID);
		  gridsize = gridInqSize(gridID);
		  missval  = vlistInqVarMissval(vlistID1, varID);

		  levelsize = zaxisInqSize(zaxisID);
		  for ( levelID = 0; levelID < levelsize; levelID++ )
		    {
		      level = zaxisInqLevel(zaxisID, levelID);
		      streamReadVarSlice(streamID1, varID, levelID, data, &nmiss);

		      if ( Info )
			printInfo(gridtype, vdate, vtime, code, level, gridsize, nmiss, missval, data);

		      if ( fname2 )
			streamWriteVarSlice(streamID2, varID, levelID, data, nmiss);
		    }
		}
	    }
	  tsID++;
        }

      free(data);

      /* fprintf(stderr, "%ld\n", (long) streamNvals(streamID1)); */

      if ( fname2 )
	{
	  streamClose(streamID2);
	  vlistDestroy(vlistID2);
	}
      streamClose(streamID1);
    }

  if ( wTable )
    tableWrite(wTable, itableID);

  return (0);
}
