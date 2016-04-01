#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cdi.h>
#include <cdi_uuid.h>


#ifndef IS_NOT_EQUAL
#  define IS_NOT_EQUAL(x,y) (x < y || y < x)
#endif

#include "printinfo.h"

#define DATE_FORMAT "%5.4d-%2.2d-%2.2d"
#define TIME_FORMAT "%2.2d:%2.2d:%2.2d"

void datetime2str(int date, int time, char *datetimestr, int maxlen)
{
  int year, month, day;
  cdiDecodeDate(date, &year, &month, &day);
  int hour, minute, second;
  cdiDecodeTime(time, &hour, &minute, &second);

  int len = sprintf(datetimestr, DATE_FORMAT "T" TIME_FORMAT, year, month, day, hour, minute, second);
  if ( len > ( maxlen-1) )
    fprintf(stderr, "Internal problem (%s): sizeof input string is too small!\n", __func__);
}


void date2str(int date, char *datestr, int maxlen)
{
  int year, month, day;
  cdiDecodeDate(date, &year, &month, &day);

  int len = sprintf(datestr, DATE_FORMAT, year, month, day);
  if ( len > ( maxlen-1) )
    fprintf(stderr, "Internal problem (%s): sizeof input string is too small!\n", __func__);
}


void time2str(int time, char *timestr, int maxlen)
{
  int hour, minute, second;
  cdiDecodeTime(time, &hour, &minute, &second);

  int len = sprintf(timestr, TIME_FORMAT, hour, minute, second);
  if ( len > ( maxlen-1) )
    fprintf(stderr, "Internal problem (%s): sizeof input string is too small!\n", __func__);
}


void printFiletype(int streamID, int vlistID)
{
  int filetype = streamInqFiletype(streamID);

  switch ( filetype )
    {
    case CDI_FILETYPE_GRB:
      printf("GRIB");
      break;
    case CDI_FILETYPE_GRB2:
      printf("GRIB2");
      break;
    case CDI_FILETYPE_NC:
      printf("NetCDF");
      break;
    case CDI_FILETYPE_NC2:
      printf("NetCDF2");
      break;
    case CDI_FILETYPE_NC4:
      printf("NetCDF4");
      break;
    case CDI_FILETYPE_NC4C:
      printf("NetCDF4 classic");
      break;
    case CDI_FILETYPE_SRV:
      printf("SERVICE");
      break;
    case CDI_FILETYPE_EXT:
      printf("EXTRA");
      break;
    case CDI_FILETYPE_IEG:
      printf("IEG");
      break;
    default:
      printf("  File format: unsupported filetype %d" , filetype);
      break;
    }

  if ( filetype == CDI_FILETYPE_SRV || filetype == CDI_FILETYPE_EXT || filetype == CDI_FILETYPE_IEG )
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

  if ( filetype == CDI_FILETYPE_GRB || filetype == CDI_FILETYPE_NC4 || filetype == CDI_FILETYPE_NC4C )
    {
      int nvars = vlistNvars(vlistID);
      for ( int varID = 0; varID < nvars; varID++ )
	{
	  int comptype = vlistInqVarCompType(vlistID, varID);
	  if ( comptype )
	    {
	      if      ( comptype == CDI_COMPRESS_SZIP ) printf(" SZIP");
	      else if ( comptype == CDI_COMPRESS_ZIP  ) printf(" ZIP");

	      break;
	    }
	}
    }

  if ( filetype == CDI_FILETYPE_GRB2 )
    {
      int nvars = vlistNvars(vlistID);
      for ( int varID = 0; varID < nvars; varID++ )
	{
	  int comptype = vlistInqVarCompType(vlistID, varID);
	  if ( comptype )
	    {
	      if ( comptype == CDI_COMPRESS_JPEG ) printf(" JPEG");
	      break;
	    }
	}
    }

  printf("\n");
}
