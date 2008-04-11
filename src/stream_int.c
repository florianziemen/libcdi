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

#include "cdi.h"
#include "stream_int.h"


int cdiDefaultInstID  = CDI_UNDEFID;
int cdiDefaultModelID = CDI_UNDEFID;
int cdiDefaultTableID = CDI_UNDEFID;
int cdiNcMissingValue = CDI_UNDEFID;
int cdiSplitLtype105  = CDI_UNDEFID;

int cdiIgnoreAttCoordinates = FALSE;

char *cdiPartabPath   = NULL;
int   cdiPartabIntern = 1;

double cdiDefaultMissval = -9.E33;


char *Filetypes[] = {
  "UNKNOWN",
  "GRIB",
  "GRIB2",
  "netCDF",
  "netCDF2",
  "netCDF4",
  "SERVICE",
  "EXTRA",
  "HDF5",
  "IEG",
};

#undef  UNDEFID
#define UNDEFID  CDI_UNDEFID


int CDI_Debug   = 0;    /* If set to 1, debugging           */


int cdiDefaultLeveltype = -1;
static int cdiDataUnreduced = 0;


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
  static char func[] = "cdiInitialize";
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
      if ( envString ) cdiNcMissingValue = atoi(envString);

      envString = getenv("SPLIT_LTYPE_105");
      if ( envString ) cdiSplitLtype105 = atoi(envString);

      envString = getenv("IGNORE_ATT_COORDINATES");
      if ( envString ) cdiIgnoreAttCoordinates = atoi(envString);

      envString = getenv("PARTAB_INTERN");
      if ( envString ) cdiPartabIntern = atoi(envString);

      envString = getenv("PARTAB_PATH");
      if ( envString ) cdiPartabPath = strdup(envString);
    }
}


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


static int  STREAM_Debug = 0;   /* If set to 1, debugging */

static int _stream_max = MAX_STREAMS;

static void stream_initialize(void);

static int _stream_init = FALSE;

#if  defined  (HAVE_LIBPTHREAD)
#  include <pthread.h>

static pthread_once_t _stream_init_thread = PTHREAD_ONCE_INIT;
static pthread_mutex_t _stream_mutex;

#  define STREAM_LOCK           pthread_mutex_lock(&_stream_mutex);
#  define STREAM_UNLOCK         pthread_mutex_unlock(&_stream_mutex);
#  define STREAM_INIT                               \
   if ( _stream_init == FALSE ) pthread_once(&_stream_init_thread, stream_initialize);

#else

#  define STREAM_LOCK
#  define STREAM_UNLOCK
#  define STREAM_INIT                               \
   if ( _stream_init == FALSE ) stream_initialize();

#endif


typedef struct _streamPtrToIdx {
  int idx;
  STREAM *ptr;
  struct _streamPtrToIdx *next;
} streamPtrToIdx;


static streamPtrToIdx *_streamList  = NULL;
static streamPtrToIdx *_streamAvail = NULL;


static void stream_list_new(void)
{
  static char func[] = "stream_list_new";

  assert(_streamList == NULL);

  _streamList = (streamPtrToIdx *) malloc(_stream_max*sizeof(streamPtrToIdx));
}


static void stream_list_delete(void)
{
  static char func[] = "stream_list_delete";

  if ( _streamList ) free(_streamList);
}


static void stream_init_pointer(void)
{
  int  i;
  
  for ( i = 0; i < _stream_max; i++ )
    {
      _streamList[i].next = _streamList + i + 1;
      _streamList[i].idx  = i;
      _streamList[i].ptr  = 0;
    }

  _streamList[_stream_max-1].next = 0;

  _streamAvail = _streamList;
}


STREAM *stream_to_pointer(int idx)
{
  static char func[] = "stream_to_pointer";
  STREAM *streamptr = NULL;

  STREAM_INIT

  if ( idx >= 0 && idx < _stream_max )
    {
      STREAM_LOCK

      streamptr = _streamList[idx].ptr;

      STREAM_UNLOCK
    }
  else
    Error(func, "stream index %d undefined!", idx);

  return (streamptr);
}


/* Create an index from a pointer */
static int stream_from_pointer(STREAM *ptr)
{
  static char func[] = "stream_from_pointer";
  int      idx = -1;
  streamPtrToIdx *newptr;

  if ( ptr )
    {
      STREAM_LOCK

      if ( _streamAvail )
	{
	  newptr       = _streamAvail;
	  _streamAvail   = _streamAvail->next;
	  newptr->next = 0;
	  idx	       = newptr->idx;
	  newptr->ptr  = ptr;
      
	  if ( STREAM_Debug )
	    Message(func, "Pointer %p has idx %d from stream list", ptr, idx);
	}
      else
	Warning(func, "Too many open streams (limit is %d)!", _stream_max);

      STREAM_UNLOCK
    }
  else
    Error(func, "Internal problem (pointer %p undefined)", ptr);

  return (idx);
}


static void stream_init_entry(STREAM *streamptr)
{
  int i;

  streamptr->self         = stream_from_pointer(streamptr);

  streamptr->accesstype        = UNDEFID;
  streamptr->accessmode        = 0;
  streamptr->filetype          = UNDEFID;
  streamptr->byteorder         = UNDEFID;
  streamptr->fileID            = 0;
  streamptr->dimgroupID        = UNDEFID;
  streamptr->filemode          = 0;
  streamptr->numvals           = 0;
  streamptr->filename          = NULL;
  streamptr->ctlname           = NULL;
  streamptr->ctlfp             = NULL;
  streamptr->record            = NULL;
  streamptr->varsAllocated     = 0;
  streamptr->nrecs             = 0;
  streamptr->nvars             = 0;
  streamptr->vars              = NULL;
  streamptr->varinit           = 0;
  streamptr->ncmode            = 0;
  streamptr->curTsID           = UNDEFID;
  streamptr->rtsteps           = 0;
  streamptr->ntsteps           = UNDEFID;
  streamptr->numTimestep       = 0;
  streamptr->tsteps            = NULL;
  streamptr->tstepsTableSize   = 0;
  streamptr->tstepsNextID      = 0;
  streamptr->historyID         = UNDEFID;
  streamptr->vlistID           = UNDEFID;
  streamptr->globalatts        = 0;
  streamptr->localatts         = 0;
  streamptr->vct.ilev          = 0;
  streamptr->vct.mlev          = 0;
  streamptr->vct.ilevID        = UNDEFID;
  streamptr->vct.mlevID        = UNDEFID;
  streamptr->unreduced         = cdiDataUnreduced;
  streamptr->ztype             = COMPRESS_NONE;
  streamptr->zlevel            = 0;

  basetimeInit(&streamptr->basetime);

  for ( i = 0; i < MAX_GRIDS_PS; i++ ) streamptr->xdimID[i]   = UNDEFID;
  for ( i = 0; i < MAX_GRIDS_PS; i++ ) streamptr->ydimID[i]   = UNDEFID;
  for ( i = 0; i < MAX_ZAXIS_PS; i++ ) streamptr->zaxisID[i]  = UNDEFID;
  for ( i = 0; i < MAX_GRIDS_PS; i++ ) streamptr->ncxvarID[i] = UNDEFID;
  for ( i = 0; i < MAX_GRIDS_PS; i++ ) streamptr->ncyvarID[i] = UNDEFID;
  for ( i = 0; i < MAX_GRIDS_PS; i++ ) streamptr->ncavarID[i] = UNDEFID;

  streamptr->curfile           = 0;
  streamptr->nfiles            = 0;
  streamptr->fnames            = NULL;
}


STREAM *stream_new_entry(void)
{
  static char func[] = "stream_new_entry";
  STREAM *streamptr;

  cdiInitialize(); /* ***************** make MT version !!! */

  STREAM_INIT

  streamptr = (STREAM *) malloc(sizeof(STREAM));

  if ( streamptr ) stream_init_entry(streamptr);

  return (streamptr);
}


void stream_delete_entry(STREAM *streamptr)
{
  static char func[] = "stream_delete_entry";
  int idx;

  idx = streamptr->self;

  STREAM_LOCK

  free(streamptr);

  _streamList[idx].next = _streamAvail;
  _streamList[idx].ptr  = 0;
  _streamAvail          = &_streamList[idx];

  STREAM_UNLOCK

  if ( STREAM_Debug )
    Message(func, "Removed idx %d from stream list", idx);
}


static void stream_initialize(void)
{
  char *env;

#if  defined  (HAVE_LIBPTHREAD)
  /* initialize global API mutex lock */
  pthread_mutex_init(&_stream_mutex, NULL);
#endif

  env = getenv("STREAM_DEBUG");
  if ( env ) STREAM_Debug = atoi(env);

  stream_list_new();
  atexit(stream_list_delete);

  STREAM_LOCK

  stream_init_pointer();

  STREAM_UNLOCK

  _stream_init = TRUE;
}


void stream_check_ptr(const char *func, STREAM *streamptr)
{
  if ( streamptr == NULL )
    Error(func, "stream undefined!");
}


int streamSize(void)
{
  int streamsize = 0;
  int i;
  
  STREAM_INIT

  STREAM_LOCK

  for ( i = 0; i < _stream_max; i++ )
    if ( _streamList[i].ptr ) streamsize++;

  STREAM_UNLOCK

  return (streamsize);
}


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


void cdiCheckContents(int streamID)
{
  static char func[] = "cdiCheckContents";
  int index, nzaxis, zaxisID;
  int vlistID;
  STREAM *streamptr;

  streamptr = stream_to_pointer(streamID);

  stream_check_ptr(func, streamptr);

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
  STREAM *streamptr;

  streamptr = stream_to_pointer(streamID);

  return (streamptr->fileID);
}


void streamDefineTaxis(int streamID)
{
  static char func[] = "streamDefineTaxis";
  STREAM *streamptr;

  streamptr = stream_to_pointer(streamID);

  if ( streamptr->tsteps == NULL )
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
  STREAM *streamptr;

  streamptr = stream_to_pointer(streamID);

  streamptr->dimgroupID = dimgroupID;
}


int streamInqDimgroupID(int streamID)
{
  STREAM *streamptr;

  streamptr = stream_to_pointer(streamID);

  return (streamptr->dimgroupID);
}


void cdiDefAccesstype(int streamID, int type)
{
  static char func[] = "cdiDefAccesstype";
  STREAM *streamptr;

  streamptr = stream_to_pointer(streamID);

  if ( streamptr->accesstype == UNDEFID )
    {
      streamptr->accesstype = type;
    }
  else
    {
      if ( streamptr->accesstype != type )
	{
	  if ( streamptr->accesstype == TYPE_REC )
	    Error(func, "Change access type from REC to VAR not allowed!");
	  else
	    Error(func, "Change access type from VAR to REC not allowed!");
	}
    }
}


int cdiInqAccesstype(int streamID)
{
  STREAM *streamptr;

  streamptr = stream_to_pointer(streamID);

  return (streamptr->accesstype);
}
