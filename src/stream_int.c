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
#include "pio_util.h"
#include "namespace.h"
#include "resource_handle.h"
#include "pio_rpc.h"

#if  defined  (HAVE_LIBCGRIBEX)
#include "cgribex.h"
#endif

int cdiDefaultCalendar = CALENDAR_PROLEPTIC;

int cdiDefaultInstID   = CDI_UNDEFID;
int cdiDefaultModelID  = CDI_UNDEFID;
int cdiDefaultTableID  = CDI_UNDEFID;
int cdiNcMissingValue  = CDI_UNDEFID;
int cdiSplitLtype105   = CDI_UNDEFID;

int cdiIgnoreAttCoordinates = FALSE;
int cdiSkipRecords          = 0;
int cdiInventoryMode        = 1;

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
  "IEG",
  "HDF5",
};

#undef  UNDEFID
#define UNDEFID  CDI_UNDEFID


int CDI_Debug   = 0;    /* If set to 1, debugging           */

static int  STREAM_Debug = 0;   /* If set to 1, debugging */

int cdiDefaultLeveltype = -1;
static int cdiDataUnreduced = 0;
static int cdiSortName = 0;
static int cdiHaveMissval = 0;


static int    streamCompareP ( void * streamptr1, void * streamptr2 );
static void   streamDestroyP ( void * streamptr );
static void   streamPrintP   ( void * streamptr, FILE * fp );
#ifdef USE_MPI
static int    streamGetPackSize ( void * streamptr, MPI_Comm comm );
static void   streamPack        ( void * streamptr, void * buff, int size,
				  int * position, MPI_Comm comm );
static int    streamTxCode      ( void );
#endif

resOps streamOps = { streamCompareP, streamDestroyP, streamPrintP,
#ifdef USE_MPI
                     streamGetPackSize,
		     streamPack,
                     streamTxCode
#endif
};

long cdiGetenvInt(char *envName)
{
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
		  Message("Invalid number string in %s: %s", envName, envString);
		  Warning("%s must comprise only digits [0-9].",envName);
		}
	      break;
	    }
	}

      if ( fact ) envValue = fact*atol(envString);

      if ( CDI_Debug ) Message("set %s to %ld", envName, envValue);
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

#if  defined  (HAVE_LIBCGRIBEX)
      gribFixZSE(1);   // 1: Fix ZeroShiftError of simple packed spherical harmonics
      gribSetConst(1); // 1: Don't pack constant fields on regular grids
#endif

      value = cdiGetenvInt("CD_REGULARGRID");
      if ( value >= 0 ) cdiDataUnreduced = (int) value;

      value = cdiGetenvInt("CDI_REGULARGRID");
      if ( value >= 0 ) cdiDataUnreduced = (int) value;

      value = cdiGetenvInt("CDI_SORTNAME");
      if ( value >= 0 ) cdiSortName = (int) value;

      value = cdiGetenvInt("CDI_HAVE_MISSVAL");
      if ( value >= 0 ) cdiHaveMissval = (int) value;

      value = cdiGetenvInt("CD_LEVELTYPE");
      if ( value >= 0 ) cdiDefaultLeveltype = (int) value;

      value = cdiGetenvInt("CDI_LEVELTYPE");
      if ( value >= 0 ) cdiDefaultLeveltype = (int) value;

      envString = getenv("CD_MISSVAL");
      if ( envString ) cdiDefaultMissval = atof(envString);

      envString = getenv("CDI_MISSVAL");
      if ( envString ) cdiDefaultMissval = atof(envString);

      envString = getenv("NC_MISSING_VALUE");
      if ( envString ) cdiNcMissingValue = atoi(envString);

      envString = getenv("SPLIT_LTYPE_105");
      if ( envString ) cdiSplitLtype105 = atoi(envString);

      envString = getenv("IGNORE_ATT_COORDINATES");
      if ( envString ) cdiIgnoreAttCoordinates = atoi(envString);

      envString = getenv("CDI_SKIP_RECORDS");
      if ( envString )
	{
	  cdiSkipRecords = atoi(envString);
	  cdiSkipRecords = cdiSkipRecords > 0 ? cdiSkipRecords : 0;
	}

      envString = getenv("GRIB_INVENTORY_MODE");
      if ( envString )
	{
	  if ( strncmp(envString, "time", 4) == 0 )
	    {
	      cdiInventoryMode = 2;
	      if ( CDI_Debug )
		Message("Inventory mode was set to timestep!");
	    }
	}

      envString = getenv("CDI_CALENDAR");
      if ( envString )
	{
	  if      ( strncmp(envString, "standard", 8) == 0 )
	    cdiDefaultCalendar = CALENDAR_STANDARD;
	  else if ( strncmp(envString, "proleptic", 9) == 0 )
	    cdiDefaultCalendar = CALENDAR_PROLEPTIC;
	  else if ( strncmp(envString, "360days", 7) == 0 )
	    cdiDefaultCalendar = CALENDAR_360DAYS;
	  else if ( strncmp(envString, "365days", 7) == 0 )
	    cdiDefaultCalendar = CALENDAR_365DAYS;
	  else if ( strncmp(envString, "366days", 7) == 0 )
	    cdiDefaultCalendar = CALENDAR_366DAYS;
	  else if ( strncmp(envString, "none", 4) == 0 )
	    cdiDefaultCalendar = CALENDAR_NONE;

	  if ( CDI_Debug )
	    Message("Default calendar set to %s!", envString);
	}
      gribSetCalendar(cdiDefaultCalendar);

      envString = getenv("PARTAB_INTERN");
      if ( envString ) cdiPartabIntern = atoi(envString);

      envString = getenv("PARTAB_PATH");
      if ( envString ) cdiPartabPath = strdup(envString);

      envString = getenv("STREAM_DEBUG");
      if ( envString ) STREAM_Debug = atoi(envString);
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


stream_t *stream_to_pointer(int idx)
{
  return ( stream_t *) reshGetVal ( idx, &streamOps );
}


static
void streamDefaultValue ( stream_t * streamptr )
{
  int i;

  streamptr->self              = UNDEFID;
  streamptr->accesstype        = UNDEFID;
  streamptr->accessmode        = 0;
  streamptr->filetype          = UNDEFID;
  streamptr->byteorder         = UNDEFID;
  streamptr->fileID            = 0;
  streamptr->dimgroupID        = UNDEFID;
  streamptr->filemode          = 0;
  streamptr->numvals           = 0;
  streamptr->filename          = NULL;
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
  streamptr->sortname          = cdiSortName;
  streamptr->have_missval      = cdiHaveMissval;
  streamptr->comptype          = COMPRESS_NONE;
  streamptr->complevel         = 0;

  basetimeInit(&streamptr->basetime);

  for ( i = 0; i < MAX_GRIDS_PS; i++ ) streamptr->xdimID[i]   = UNDEFID;
  for ( i = 0; i < MAX_GRIDS_PS; i++ ) streamptr->ydimID[i]   = UNDEFID;
  for ( i = 0; i < MAX_ZAXES_PS; i++ ) streamptr->zaxisID[i]  = UNDEFID;
  for ( i = 0; i < MAX_GRIDS_PS; i++ ) streamptr->ncxvarID[i] = UNDEFID;
  for ( i = 0; i < MAX_GRIDS_PS; i++ ) streamptr->ncyvarID[i] = UNDEFID;
  for ( i = 0; i < MAX_GRIDS_PS; i++ ) streamptr->ncavarID[i] = UNDEFID;

  streamptr->curfile           = 0;
  streamptr->nfiles            = 0;
  streamptr->fnames            = NULL;

  streamptr->gribContainers    = NULL;
  streamptr->vlistIDorig       = UNDEFID;
}


stream_t *stream_new_entry(void)
{
  stream_t *streamptr;

  cdiInitialize(); /* ***************** make MT version !!! */

  streamptr = (stream_t *) xmalloc(sizeof(stream_t));
  streamDefaultValue ( streamptr );
  streamptr->self = reshPut (( void * ) streamptr, &streamOps );

  return streamptr;
}


void stream_delete_entry(stream_t *streamptr)
{
  int idx;

  xassert ( streamptr );

  idx = streamptr->self;
  free ( streamptr );
  reshRemove ( idx, &streamOps );

  if ( STREAM_Debug )
    Message("Removed idx %d from stream list", idx);
}


void stream_check_ptr(const char *caller, stream_t *streamptr)
{
  if ( streamptr == NULL )
    Errorc("stream undefined!");
}



void cdiDefGlobal(const char *string, int val)
{
  if ( strcmp(string, "REGULARGRID") == 0 )
    {
      cdiDataUnreduced = val;
    }
  else if ( strcmp(string, "SORTNAME") == 0 )
    {
      cdiSortName = val;
    }
  else if ( strcmp(string, "HAVE_MISSVAL") == 0 )
    {
      cdiHaveMissval = val;
    }
  else
    {
      Warning("Unsupported global key: %s", string);
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

  return (cdiDefaultMissval);
}


void cdiCheckContents(int streamID)
{
  int index, nzaxis, zaxisID;
  int vlistID;
  stream_t *streamptr;

  streamptr = ( stream_t *) reshGetVal ( streamID, &streamOps );

  stream_check_ptr(__func__, streamptr);

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
  stream_t *streamptr;

  streamptr = ( stream_t *) reshGetVal ( streamID, &streamOps );

  return (streamptr->fileID);
}


void streamDefineTaxis(int streamID)
{
  stream_t *streamptr;

  streamptr = ( stream_t *) reshGetVal ( streamID, &streamOps );

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
	Error("time axis undefined");
    }
}


void streamDefDimgroupID(int streamID, int dimgroupID)
{
  stream_t *streamptr;

  streamptr = ( stream_t *) reshGetVal ( streamID, &streamOps );

  streamptr->dimgroupID = dimgroupID;
}


int streamInqDimgroupID(int streamID)
{
  stream_t *streamptr;

  streamptr = ( stream_t *) reshGetVal ( streamID, &streamOps );

  return (streamptr->dimgroupID);
}


void cdiDefAccesstype(int streamID, int type)
{
  stream_t *streamptr;

  streamptr = ( stream_t *) reshGetVal ( streamID, &streamOps );

  if ( streamptr->accesstype == UNDEFID )
    {
      streamptr->accesstype = type;
    }
  else
    {
      if ( streamptr->accesstype != type )
	{
	  if ( streamptr->accesstype == TYPE_REC )
	    Error("Changing access type from REC to VAR not allowed!");
	  else
	    Error("Changing access type from VAR to REC not allowed!");
	}
    }
}


int cdiInqAccesstype(int streamID)
{
  stream_t *streamptr;

  streamptr = ( stream_t *) reshGetVal ( streamID, &streamOps );

  return (streamptr->accesstype);
} 


int streamInqNvars ( int streamID )
{
  stream_t * streamptr;
  streamptr = ( stream_t * ) reshGetVal ( streamID, &streamOps );
  return ( streamptr->nvars );
}


int  streamCompareP ( void * streamptr1, void * streamptr2 )
{
  stream_t * s1 = ( stream_t * ) streamptr1;
  stream_t * s2 = ( stream_t * ) streamptr2;
  int differ = -1;
  int equal  = 0;
  int len;

  xassert ( s1 );
  xassert ( s2 );

  if ( s1->filetype  != s2->filetype  ) return differ;
  if (  namespaceAdaptKey2 ( s1->vlistIDorig ) != 			     
	namespaceAdaptKey2 ( s2->vlistIDorig )) return differ;
  if ( s1->byteorder != s2->byteorder ) return differ;
  if ( s1->comptype  != s2->comptype  ) return differ;
  if ( s1->complevel != s2->complevel ) return differ; 

  if ( s1->filename )
    {
      len = strlen ( s1->filename ) + 1;
      if ( memcmp ( s1->filename, s2->filename, len )) 
	return differ;
    }
  else if ( s2->filename ) 
    return differ;

  return equal;
}


void streamDestroyP ( void * streamptr )
{
  int id;
  stream_t * sp = ( stream_t * ) streamptr;

  xassert ( sp );

  id = sp->self;
  streamClose ( id );
}


void streamPrintP   ( void * streamptr, FILE * fp )
{
  stream_t * sp = ( stream_t * ) streamptr;

  if ( !sp ) return;

  fprintf ( fp, "#\n");
  fprintf ( fp, "# streamID %d\n", sp->self);
  fprintf ( fp, "#\n"); 
  fprintf ( fp, "self          = %d\n", sp->self );
  fprintf ( fp, "accesstype    = %d\n", sp->accesstype );
  fprintf ( fp, "accessmode    = %d\n", sp->accessmode ); 
  fprintf ( fp, "filetype      = %d\n", sp->filetype );
  fprintf ( fp, "byteorder     = %d\n", sp->byteorder );
  fprintf ( fp, "fileID        = %d\n", sp->fileID );
  fprintf ( fp, "dimgroupID    = %d\n", sp->dimgroupID );
  fprintf ( fp, "filemode      = %d\n", sp->filemode );
  fprintf ( fp, "//off_t numvals;\n" );
  fprintf ( fp, "filename      = %s\n", sp->filename );
  fprintf ( fp, "//Record   *record;\n" );
  fprintf ( fp, "nrecs         = %d\n", sp->nrecs );
  fprintf ( fp, "nvars         = %d\n", sp->nvars );
  fprintf ( fp, "varlocked     = %d\n", sp->varlocked );
  fprintf ( fp, "//SVARINFO *vars;\n" );
  fprintf ( fp, "varsAllocated = %d\n", sp->varsAllocated );
  fprintf ( fp, "varinit       = %d\n", sp->varinit );
  fprintf ( fp, "curTsID       = %d\n", sp->curTsID );
  fprintf ( fp, "rtsteps       = %d\n", sp->rtsteps );
  fprintf ( fp, "//long ntsteps;\n" );
  fprintf ( fp, "numTimestep   = %d\n", sp->numTimestep );
  fprintf ( fp, "//  TSTEPS   *tsteps;\n" );
  fprintf ( fp, "tstepsTableSize= %d\n", sp->tstepsTableSize );
  fprintf ( fp, "tstepsNextID  = %d\n", sp->tstepsNextID );
  fprintf ( fp, "//BaseTime  basetime;\n" );
  fprintf ( fp, "ncmode        = %d\n", sp->ncmode );
  fprintf ( fp, "vlistID       = %d\n", sp->vlistID );
  fprintf ( fp, "//  int       xdimID[MAX_GRIDS_PS];\n" );
  fprintf ( fp, "//  int       ydimID[MAX_GRIDS_PS];\n" );
  fprintf ( fp, "//  int       zaxisID[MAX_ZAXES_PS];\n" );
  fprintf ( fp, "//  int       ncxvarID[MAX_GRIDS_PS];\n" );
  fprintf ( fp, "//  int       ncyvarID[MAX_GRIDS_PS];\n" );
  fprintf ( fp, "//  int       ncavarID[MAX_GRIDS_PS];\n" );
  fprintf ( fp, "historyID     = %d\n", sp->historyID );
  fprintf ( fp, "globalatts    = %d\n", sp->globalatts );
  fprintf ( fp, "localatts     = %d\n", sp->localatts );
  fprintf ( fp, "//  VCT       vct;\n" );
  fprintf ( fp, "unreduced     = %d\n", sp->unreduced );
  fprintf ( fp, "sortname      = %d\n", sp->sortname );
  fprintf ( fp, "have_missval  = %d\n", sp->have_missval );
  fprintf ( fp, "ztype         = %d\n", sp->comptype );
  fprintf ( fp, "zlevel        = %d\n", sp->complevel );
  fprintf ( fp, "curfile       = %d\n", sp->curfile );
  fprintf ( fp, "nfiles        = %d\n", sp->nfiles );
  fprintf ( fp, "//  char    **fnames;\n" );
  fprintf ( fp, "//  void     *gribContainers;\n" );
  fprintf ( fp, "vlistIDorig   = %d\n", sp->vlistIDorig );
}


int streamSize(void)
{
  return reshCountType ( &streamOps );
}


void streamGetIndexList ( int nstreams, int * streamIndexList )
{
  reshGetResHListOfType ( nstreams, streamIndexList, &streamOps );
}


#ifdef USE_MPI
static int
streamTxCode ( void )
{
  return STREAM;
}


int streamNint = 11 ;


static
int streamGetPackSize ( void * voidP, MPI_Comm comm )
{
  stream_t * streamP = ( stream_t * ) voidP;
  int packBufferSize = 0, size;

  xmpi ( MPI_Pack_size ( streamNint, MPI_INT, comm, &size ));
  packBufferSize += size;
  xmpi ( MPI_Pack_size ( 1, MPI_DOUBLE, comm, &size ));
  packBufferSize += 2 * size;


  xmpi ( MPI_Pack_size (( int ) strlen ( streamP->filename ) + 1,
			MPI_CHAR, comm, &size ));
  packBufferSize += size;
  xmpi ( MPI_Pack_size ( 1, MPI_DOUBLE, comm, &size ));
  packBufferSize += size;

  return packBufferSize;
}


static
void streamPack ( void * streamptr, void * packBuffer, int packBufferSize,
                  int * packBufferPos, MPI_Comm comm )
{
  stream_t * streamP = ( stream_t * ) streamptr;
  int intBuffer[streamNint];  
  double d;

  intBuffer[0]  = streamP->self;
  intBuffer[1]  = streamP->filetype;
  intBuffer[2]  =  ( int ) strlen ( streamP->filename ) + 1;
  intBuffer[3]  = streamP->vlistID;
  intBuffer[4]  = streamP->vlistIDorig;
  intBuffer[5]  = streamP->byteorder;
  intBuffer[6]  = streamP->comptype;
  intBuffer[7]  = streamP->complevel; 
  intBuffer[8]  = cdiDataUnreduced;
  intBuffer[9]  = cdiSortName;
  intBuffer[10] = cdiHaveMissval;
  
  xmpi ( MPI_Pack ( intBuffer, streamNint, MPI_INT, 
		    packBuffer, packBufferSize, packBufferPos, comm ));
  d = xchecksum ( DATATYPE_INT , streamNint, intBuffer );
  xmpi ( MPI_Pack ( &d, 1, MPI_DOUBLE,
		    packBuffer, packBufferSize, packBufferPos, comm ));

  xmpi ( MPI_Pack ( &cdiDefaultMissval, 1, MPI_DOUBLE,
		    packBuffer, packBufferSize, packBufferPos, comm ));

  xmpi ( MPI_Pack ( streamP->filename, intBuffer[2], MPI_CHAR, 
		    packBuffer, packBufferSize, packBufferPos, comm )); 
  d = xchecksum  ( DATATYPE_TXT, intBuffer[2], &streamP->filename );
  xmpi ( MPI_Pack ( &d, 1, MPI_DOUBLE,
		    packBuffer, packBufferSize, packBufferPos, comm ));
}

#endif
/*
 * Local Variables:
 * c-file-style: "Java"
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * show-trailing-whitespace: t
 * require-trailing-newline: t
 * End:
 */
