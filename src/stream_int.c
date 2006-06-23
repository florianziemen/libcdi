#if defined (HAVE_CONFIG_H)
#  include "config.h"
#endif

#include <stdio.h>
#include <stdarg.h>
#include <string.h> 
#include <errno.h>
#include <math.h>
#include <ctype.h>

#include "dmemory.h"

#include "file.h"
#include "cdi.h"
#include "stream_int.h"
#include "cdf_int.h"
#include "stream_srv.h"


int cdiDefaultInstID  = CDI_UNDEFID;
int cdiDefaultModelID = CDI_UNDEFID;
int cdiDefaultTableID = CDI_UNDEFID;
int cdiNcMissingValue = CDI_UNDEFID;

double cdiDefaultMissval = -9.E33;


char *Filetypes[] = {
  "UNKNOWN",
  "GRIB",
  "GRIB2",
  "netCDF",
  "netCDF2",
  "SERVICE",
  "EXTRA",
  "HDF5",
  "IEG",
};

#undef  UNDEFID
#define UNDEFID  CDI_UNDEFID

CDITABLE *streams = 0;
int streamsSize = 0;

int CDI_Debug   = 0;    /* If set to 1, debugging           */


char *strfiletype(int filetype)
{
  char *name;
  int size = (int) (sizeof(Filetypes)/sizeof(char *));

  if ( filetype > 0 && filetype < size )
    name = Filetypes[filetype];
  else
    name = Filetypes[0];  

  return (name);
}

void streamCheckID(const char *func, int streamID)
{
  if ( streamID == CDI_UNDEFID )
    Error(func, "streamID undefined!");

  if ( streamID < 0 || streamID >= streamsSize )
    Error(func, "streamID %d out of range!", streamID);

  if ( streams[streamID].filename == NULL )
    Error(func, "streamID %d not open!", streamID);
}

int cdiDefaultLeveltype = -1;
static int cdiDataUnreduced = 0;
static int cdiDataCompress = 0;


int cdiDefGlobal(const char *string, int val)
{
  int ret = 0;

  if ( strcmp(string, "CD_REGULARGRID") == 0 )
    {
      cdiDataUnreduced = val;
      ret = 1;
    }

  return (ret);
}

void cdiDefCompress(int level)
{
  cdiDataCompress = level;
}

long cdiGetenvInt(char *envName)
{
  static char func[] = "cdiGetenv";
  char *envString;
  long envValue = -1;
  long fact = 1;

  envString = getenv(envName);

  if ( envString )
    {
      int loop, len;

      len = (int) strlen(envString);
      for ( loop = 0; loop < len; loop++ )
	{
	  if ( ! isdigit((int) envString[loop]) )
	    {
	      switch ( tolower((int) envString[loop]) )
		{
		case 'k':  fact = 1024;        break;
		case 'm':  fact = 1048576;     break;
		case 'g':  fact = 1073741824;  break;
		default:
		  fact = 0;
		  Message(func, "Invalid number string in %s: %s", envName, envString);
		  Warning(func, "%s must comprise only digits [0-9].",envName);
		}
	      break;
	    }
	}

      if ( fact ) envValue = fact*atol(envString);

      if ( CDI_Debug ) Message(func, "set %s to %ld", envName, envValue);
    }

  return (envValue);
}


void cdiInitialize(void)
{
  static int Init_CDI = FALSE;
  char *envString;
  long value;

  if ( ! Init_CDI )
    {
      Init_CDI = TRUE;

      value = cdiGetenvInt("CD_REGULARGRID");
      if ( value >= 0 ) cdiDataUnreduced = (int) value;

      value = cdiGetenvInt("CD_LEVELTYPE");
      if ( value >= 0 ) cdiDefaultLeveltype = (int) value;

      envString = getenv("CD_MISSVAL");
      if ( envString ) cdiDefaultMissval = atof(envString);

      envString = getenv("NC_MISSING_VALUE");
      if ( envString ) cdiNcMissingValue = atof(envString);
    }
}


void cdiDefMissval(double missval)
{
  cdiInitialize();

  cdiDefaultMissval = missval;

}


double cdiInqMissval(void)
{
  cdiInitialize();

  return (cdiDefaultMissval);;
}


void streamsInitEntry(int streamID)
{
  static char func[] = "streamsInitEntry";
  int i;

  if ( streamID < 0 || streamID >= streamsSize )
    Error(func, "streamID %d undefined!", streamID);

  cdiInitialize();

  streams[streamID].accesstype        = UNDEFID;
  streams[streamID].accessmode        = 0;
  streams[streamID].filetype          = UNDEFID;
  streams[streamID].byteorder         = UNDEFID;
  streams[streamID].fileID            = 0;
  streams[streamID].dimgroupID        = UNDEFID;
  streams[streamID].filemode          = 0;
  streams[streamID].filename          = NULL;
  streams[streamID].ctlname           = NULL;
  streams[streamID].ctlfp             = NULL;
  streams[streamID].record            = NULL;
  streams[streamID].varsAllocated     = 0;
  streams[streamID].nrecs             = 0;
  streams[streamID].nvars             = 0;
  streams[streamID].vars              = NULL;
  streams[streamID].varinit           = 0;
  streams[streamID].ncmode            = 0;
  streams[streamID].curTsID           = UNDEFID;
  streams[streamID].rtsteps           = 0;
  streams[streamID].ntsteps           = UNDEFID;
  streams[streamID].numTimestep       = 0;
  streams[streamID].tsteps            = NULL;
  streams[streamID].tstepsTableSize   = 0;
  streams[streamID].tstepsNextID      = 0;
  streams[streamID].historyID         = UNDEFID;
  streams[streamID].vlistID           = UNDEFID;
  streams[streamID].globalatts        = 0;
  streams[streamID].localatts         = 0;
  streams[streamID].vct.ilev          = 0;
  streams[streamID].vct.mlev          = 0;
  streams[streamID].vct.ilevID        = UNDEFID;
  streams[streamID].vct.mlevID        = UNDEFID;
  streams[streamID].unreduced         = cdiDataUnreduced;
  streams[streamID].compress          = cdiDataCompress;

  basetimeInit(&streams[streamID].basetime);

  for ( i = 0; i < MAX_GRIDS; i++ ) streams[streamID].xdimID[i]   = UNDEFID;
  for ( i = 0; i < MAX_GRIDS; i++ ) streams[streamID].ydimID[i]   = UNDEFID;
  for ( i = 0; i < MAX_ZAXIS; i++ ) streams[streamID].zaxisID[i]  = UNDEFID;
  for ( i = 0; i < MAX_GRIDS; i++ ) streams[streamID].ncxvarID[i] = UNDEFID;
  for ( i = 0; i < MAX_GRIDS; i++ ) streams[streamID].ncyvarID[i] = UNDEFID;
  for ( i = 0; i < MAX_GRIDS; i++ ) streams[streamID].ncavarID[i] = UNDEFID;
}


int streamsNewEntry(int filetype)
{
  static char func[] = "streamsNewEntry";
  int streamsMax = 1024;
  int streamID = 0;

  /*
    Look for a free slot in streams.
    (Create the table the first time through).
  */
  if ( !streamsSize )
    {
      int i;

      streamsSize = 2;
      streams = (CDITABLE *) malloc(streamsSize*sizeof(CDITABLE));
      if( streams == NULL )
	{
          Message(func, "streamsSize = %d", streamsSize);
	  SysError(func, "Allocation of CDITABLE failed");
	}

      for( i = 0; i < streamsSize; i++ )
	streamsInitEntry(i);
    }
  else
    {
      while( streamID < streamsSize )
	{
	  if ( streams[streamID].filetype == UNDEFID ) break;
	  streamID++;
	}
    }
  /*
    If the table overflows, double its size.
  */
  if( streamID == streamsSize )
    {
      int i;

      if ( streamsSize == streamsMax )
	Error(func, "CDI maximum table size of %d reached", streamsMax);

      streamsSize = 2*streamsSize;
      streams = (CDITABLE *) realloc(streams, streamsSize*sizeof(CDITABLE));
      if( streams == NULL )
	{
          Message(func, "streamsSize = %d", streamsSize);
	  SysError(func, "Reallocation of CDITABLE failed");
	}
      streamID = streamsSize/2;

      for( i = streamID; i < streamsSize; i++ )
	streamsInitEntry(i);
    }

  streamsInitEntry(streamID);

  streams[streamID].filetype = filetype;

  return (streamID);
}


void cdiCheckContents(int streamID)
{
  static char func[] = "cdiCheckContents";
  int index, nzaxis, zaxisID;
  int vlistID;

  streamCheckID(func, streamID);

  vlistID = streamInqVlist(streamID);
  nzaxis = vlistNzaxis(vlistID);

  for ( index = 0; index < nzaxis; index++ )
    {
      zaxisID = vlistZaxis(vlistID, index);
      if ( zaxisInqType(zaxisID) == ZAXIS_GENERIC )
	cdiCheckZaxis(zaxisID);
    }
    
}


int streamInqFileID(int streamID)
{
  return (streams[streamID].fileID);
}


void streamDefineTaxis(int streamID)
{
  static char func[] = "streamDefineTaxis";

  if ( streams[streamID].tsteps == NULL )
    {
      int varID, nvars;
      int vlistID;
  
      vlistID = streamInqVlist(streamID);

      nvars = vlistNvars(vlistID);
      for ( varID = 0; varID < nvars; varID++ )
	if ( vlistInqVarTime(vlistID, varID) == TIME_VARIABLE ) break;

      if ( varID == nvars )
	{
	  int taxisID;

	  taxisID = vlistInqTaxis(vlistID);
	  if ( taxisID == CDI_UNDEFID )
	    {
	      taxisID = taxisCreate(TAXIS_ABSOLUTE);
	      vlistDefTaxis(vlistID, taxisID);
	    }
	    
	  (void) streamDefTimestep(streamID, 0);
	}
      else
	Error(func, "time axis undefined");
    }
}


void streamDefDimgroupID(int streamID, int dimgroupID)
{
  streams[streamID].dimgroupID = dimgroupID;
}


int streamInqDimgroupID(int streamID)
{
  return (streams[streamID].dimgroupID);
}


void cdiDefAccesstype(int streamID, int type)
{
  static char func[] = "cdiDefAccesstype";

  if ( streams[streamID].accesstype == UNDEFID )
    {
      streams[streamID].accesstype = type;
    }
  else
    {
      if ( streams[streamID].accesstype != type )
	{
	  if ( streams[streamID].accesstype == TYPE_REC )
	    Error(func, "Change access type from REC to VAR not allowed!");
	  else
	    Error(func, "Change access type from VAR to REC not allowed!");
	}
    }
}


int cdiInqAccesstype(int streamID)
{
  return (streams[streamID].accesstype);
}
