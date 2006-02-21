#if defined (HAVE_CONFIG_H)
#  include "config.h"
#endif

#include <ctype.h>

#include "dmemory.h"
#include "cdi.h"
#include "stream_int.h"
#include "cdf.h"
#include "stream_grb.h"
#include "stream_cdf.h"
#include "stream_hdf.h"
#include "stream_srv.h"
#include "stream_ext.h"
#include "stream_ieg.h"
#include "file.h"
#include "grib.h"
#include "cdf.h"
#include "hdf.h"
#include "service.h"
#include "extra.h"
#include "ieg.h"
#include "vlist.h"


void cdiPrintDefaults(void)
{
  fprintf (stderr, "default instID     :  %d\n", cdiDefaultInstID); 
  fprintf (stderr, "default modelID    :  %d\n", cdiDefaultModelID); 
  fprintf (stderr, "default tableID    :  %d\n", cdiDefaultTableID); 
  fprintf (stderr, "default missval    :  %g\n", cdiDefaultMissval); 
}


void cdiDebug(int level)
{
  static char func[] = "cdiDebug";

  if ( level == 1 || level &  2 ) CDI_Debug = 1;

  if ( CDI_Debug ) Message(func, "debug level %d", level);

  if ( level == 1 || level &  4 ) memDebug(1);

  if ( level == 1 || level &  8 ) fileDebug(1);

  if ( level == 1 || level & 16 )
    {
#if  defined  (HAVE_LIBGRIB)
      gribSetDebug(1);
#endif
#if  defined  (HAVE_LIBNETCDF)
      cdfDebug(1);
#endif
#if  defined  (HAVE_LIBHDF5)
      hdfDebug(1);
#endif
#if  defined  (HAVE_LIBSERVICE)
      srvDebug(1);
#endif
#if  defined  (HAVE_LIBEXTRA)
      extDebug(1);
#endif
#if  defined  (HAVE_LIBIEG)
      iegDebug(1);
#endif
    }

  if ( CDI_Debug )
    {
      cdiPrintDefaults();
      cdiPrintDatatypes();
    }
}


#undef  IsBigendian
#define IsBigendian()  ( u_byteorder.c[sizeof(long) - 1] )

#define  CDI_BIGENDIAN            0   /* Data type BIGENDIAN     */
#define  CDI_LITTLEENDIAN         1   /* Data type LITTLEENDIAN  */


static int getByteorder(int byteswap)
{
  static union {unsigned long l; unsigned char c[sizeof(long)];} u_byteorder = {1};
  int byteorder = -1;

  if ( IsBigendian() )
    {
      if ( byteswap ) byteorder = CDI_LITTLEENDIAN;
      else            byteorder = CDI_BIGENDIAN;
    }
  else
    {
      if ( byteswap ) byteorder = CDI_BIGENDIAN;
      else            byteorder = CDI_LITTLEENDIAN;
    }

  return (byteorder);
}


static int getFiletype(const char *filename, int *byteorder)
{
  static char func[] = "getFiletype";
  int filetype = CDI_EUFTYPE;
  int fileID;
  int swap = 0;
  long recpos;
  char buffer[4];

  fileID = fileOpen(filename, "r");

  if ( fileID == CDI_UNDEFID )
    {
      if ( strncmp(filename, "http:", 5) == 0 )
	return (FILETYPE_NC);
      else
	return (CDI_ESYSTEM);
    }

  if ( fileRead(fileID, buffer, 4) != 4 ) return (CDI_EUFTYPE);

  fileRewind(fileID);

  if ( strncmp(buffer, "GRIB", 4) == 0 )
    {
      filetype = FILETYPE_GRB;
      if ( CDI_Debug ) Message(func, "found GRIB file = %s", filename);
    }
  else if ( strncmp(buffer, "CDF\001", 4) == 0 )
    {
      filetype = FILETYPE_NC;
      if ( CDI_Debug ) Message(func, "found CDF1 file = %s", filename);
    }
  else if ( strncmp(buffer, "CDF\002", 4) == 0 )
    {
      filetype = FILETYPE_NC2;
      if ( CDI_Debug ) Message(func, "found CDF2 file = %s", filename);
    }
  else if ( strncmp(buffer+1, "HDF", 3) == 0 )
    {
      filetype = FILETYPE_H5;
      if ( CDI_Debug ) Message(func, "found HDF file = %s", filename);
    }
#if  defined  (HAVE_LIBSERVICE)
  else if ( srvCheckFiletype(fileID, &swap) )
    {
      filetype = FILETYPE_SRV;
      if ( CDI_Debug ) Message(func, "found SRV file = %s", filename);
    }
#endif
#if  defined  (HAVE_LIBEXTRA)
  else if ( extCheckFiletype(fileID, &swap) )
    {
      filetype = FILETYPE_EXT;
      if ( CDI_Debug ) Message(func, "found EXT file = %s", filename);
    }
#endif
#if  defined  (HAVE_LIBIEG)
  else if ( iegCheckFiletype(fileID, &swap) )
    {
      filetype = FILETYPE_IEG;
      if ( CDI_Debug ) Message(func, "found IEG file = %s", filename);
    }
#endif
  else if ( gribFileSeek(fileID, &recpos) == 0 )
    {
      filetype = FILETYPE_GRB;
      if ( CDI_Debug ) Message(func, "found seek GRIB file = %s", filename);
    }

  fileClose(fileID);

  *byteorder = getByteorder(swap);

  return (filetype);
}


/*
@Function  streamInqFiletype
@Title     Get the filetype

@Prototype int streamInqFiletype(int streamID)
@Parameter
    @Item  streamID  Stream identifier

@Description
The function {\tt streamInqFiletype} gets the filetype of a stream.

@Result
{\tt streamInqFiletype} return the type of the file format,
one of the set of predefined CDI file format types.
The valid CDI file format types are {\tt FILETYPE_GRB}, {\tt FILETYPE_NC},
{\tt FILETYPE_NC2}, {\tt FILETYPE_SRV}, and {\tt FILETYPE_EXT}.

@EndFunction
*/
int streamInqFiletype(int streamID)
{
  static char func[] = "streamInqFiletype";

  streamCheckID(func, streamID);

  return (streams[streamID].filetype);
}


int getByteswap(int byteorder)
{
  static union {unsigned long l; unsigned char c[sizeof(long)];} u_byteorder = {1};
  int byteswap = 0;

  if ( IsBigendian() )
    {
      if ( byteorder == CDI_LITTLEENDIAN ) byteswap = TRUE;
    }
  else
    {
      if ( byteorder == CDI_BIGENDIAN ) byteswap = TRUE;
    }

  return (byteswap);
}


void streamDefByteorder(int streamID, int byteorder)
{
  static char func[] = "streamDefByteorder";
  int filetype, fileID;

  streamCheckID(func, streamID);

  streams[streamID].byteorder = byteorder;
  filetype = streams[streamID].filetype;
  fileID   = streams[streamID].fileID;

  switch (filetype)
    {
#if  defined  (HAVE_LIBSERVICE)
    case FILETYPE_SRV:
      {
	SRVREC *srvp = streams[streamID].record->srvp;
	srvp->byteswap = getByteswap(byteorder);

	break;
      }
#endif
#if  defined  (HAVE_LIBEXTRA)
    case FILETYPE_EXT:
      {
	EXTREC *extp = streams[streamID].record->extp;
	extp->byteswap = getByteswap(byteorder);

	break;
      }
#endif
#if  defined  (HAVE_LIBIEG)
    case FILETYPE_IEG:
      {
	IEGREC *iegp = streams[streamID].record->iegp;
	iegp->byteswap = getByteswap(byteorder);

	break;
      }
#endif
    }
}


int streamInqByteorder(int streamID)
{
  static char func[] = "streamInqByteorder";

  streamCheckID(func, streamID);

  return (streams[streamID].byteorder);
}


char *streamFilesuffix(int filetype)
{
  static char *fileSuffix[] = {"", ".grb", ".g2", ".nc", ".nc2", ".srv", ".ext", ".h5", ".ieg"};
  int size = sizeof(fileSuffix)/sizeof(char *);

  if ( filetype > 0 && filetype < size )
    return (fileSuffix[filetype]);
  else
    return (fileSuffix[0]);
}


char *streamFilename(int streamID)
{
  static char func[] = "streamFilename";

  streamCheckID(func, streamID);

  return (streams[streamID].filename);
}


int cdiInqTimeSize(int streamID)
{
  static char func[] = "cdiInqTimeSize";
  int ntsteps;
  int tsID = 0, nrecs;

  streamCheckID(func, streamID);

  ntsteps = streams[streamID].ntsteps;

  if ( ntsteps == CDI_UNDEFID )
    while ( (nrecs = streamInqTimestep(streamID, tsID++)) )

  ntsteps = streams[streamID].ntsteps;

  return (ntsteps);
}


int cdiInqContents(int streamID)
{
  static char func[] = "cdiInqContents";
  int filetype;
  int vlistID;
  int taxisID;
  int status = 0;

  streamCheckID(func, streamID);

  filetype = streams[streamID].filetype;

  switch (filetype)
    {
#if  defined  (HAVE_LIBGRIB)
    case FILETYPE_GRB:
      {
        status = grbInqContents(streamID);
	break;
      }
#endif
#if  defined  (HAVE_LIBSERVICE)
    case FILETYPE_SRV:
      {
        status = srvInqContents(streamID);
	break;
      }
#endif
#if  defined  (HAVE_LIBEXTRA)
    case FILETYPE_EXT:
      {
        status = extInqContents(streamID);
	break;
      }
#endif
#if  defined  (HAVE_LIBIEG)
    case FILETYPE_IEG:
      {
        status = iegInqContents(streamID);
	break;
      }
#endif
#if  defined  (HAVE_LIBNETCDF)
    case FILETYPE_NC:
    case FILETYPE_NC2:
      {
        status = cdfInqContents(streamID);
	break;
      }
#endif
#if  defined  (HAVE_LIBHDF5)
    case FILETYPE_H5:
      {
        hdfInqContents(streamID);
	break;
      }
#endif
    default:
      {
	if ( CDI_Debug )
	  Message(func, "%s library not available!", strfiletype(filetype));

	status = CDI_ELIBNAVAIL;
      }
    }

  if ( status == 0 )
    {
      vlistID = streamInqVlist(streamID);
      taxisID = vlistInqTaxis(vlistID);
      if ( taxisID != -1 )
	ptaxisCopy(taxisPtr(taxisID), &streams[streamID].tsteps[0].taxis);
    }

  return (status);
}


int streamOpen(const char *filename, const char *filemode, int filetype)
{
  static char func[] = "streamOpen";
  int fileID = CDI_UNDEFID;
  int streamID = CDI_ESYSTEM;
  int status;
  Record *record = NULL;

  if ( CDI_Debug )
    Message(func, "Open %s mode %c file %s", strfiletype(filetype), (int) *filemode, filename);

  if ( ! filename || ! filemode || filetype < 0 ) return (CDI_EINVAL);

  switch (filetype)
    {
#if  defined  (HAVE_LIBGRIB)
    case FILETYPE_GRB:
      {
        fileID = gribOpen(filename, filemode);
	record = (Record *) malloc(sizeof(Record));
	record->buffer = NULL;
	break;
      }
#endif
#if  defined  (HAVE_LIBSERVICE)
    case FILETYPE_SRV:
      {
        fileID = fileOpen(filename, filemode);
	record = (Record *) malloc(sizeof(Record));
	record->buffer = NULL;
	record->srvp   = srvNew();
	break;
      }
#endif
#if  defined  (HAVE_LIBEXTRA)
    case FILETYPE_EXT:
      {
        fileID = fileOpen(filename, filemode);
	record = (Record *) malloc(sizeof(Record));
	record->buffer = NULL;
	record->extp   = extNew();
	break;
      }
#endif
#if  defined  (HAVE_LIBIEG)
    case FILETYPE_IEG:
      {
        fileID = fileOpen(filename, filemode);
	record = (Record *) malloc(sizeof(Record));
	record->buffer = NULL;
	record->iegp   = iegNew();
	break;
      }
#endif
#if  defined  (HAVE_LIBNETCDF)
    case FILETYPE_NC:
      {
	fileID = cdfOpen(filename, filemode);
	break;
      }
    case FILETYPE_NC2:
      {
	fileID = cdfOpen64(filename, filemode);
	break;
      }
#endif
#if  defined  (HAVE_LIBHDF5)
    case FILETYPE_H5:
      {
	fileID = hdfOpen(filename, filemode);
	break;
      }
#endif
    default:
      {
	if ( CDI_Debug ) Message(func, "%s library not available!", strfiletype(filetype));
	return (CDI_ELIBNAVAIL);
      }
    }

  if ( fileID < 0 )
    {
      streamID = fileID;
    }
  else
    {
      streamID = streamsNewEntry(filetype);

      streams[streamID].record   = record;
      streams[streamID].filetype = filetype;
      streams[streamID].filemode = tolower(*filemode);
      streams[streamID].filename = strdupx(filename);
      streams[streamID].fileID   = fileID;

      if ( streams[streamID].filemode == 'r' )
	{
	  VLIST *vlistptr;
	  streams[streamID].vlistID = vlistNew();
	  /* cdiReadByteorder(streamID); */
	  status = cdiInqContents(streamID);
	  if ( status < 0 ) return (status);
	  vlistptr = vlist_to_pointer(streams[streamID].vlistID);
	  vlistptr->ntsteps = streamNtsteps(streamID);
	}

      if ( streams[streamID].filemode == 'w' )
	{
	  if ( filetype == FILETYPE_H5 ) hdfDefDimgroup(streamID);
	}
    }
 
  return (streamID);
}


int streamOpenA(const char *filename, const char *filemode, int filetype)
{
  static char func[] = "streamOpenA";
  int fileID = CDI_UNDEFID;
  int streamID = CDI_ESYSTEM;
  int status;
  Record *record = NULL;

  if ( CDI_Debug )
    Message(func, "Open %s mode %c file %s", strfiletype(filetype), (int) *filemode, filename);

  if ( ! filename || ! filemode || filetype < 0 ) return (CDI_EINVAL);

  switch (filetype)
    {
#if  defined  (HAVE_LIBGRIB)
    case FILETYPE_GRB:
      {
        fileID = gribOpen(filename, "r");
	record = (Record *) malloc(sizeof(Record));
	record->buffer = NULL;
	break;
      }
#endif
#if  defined  (HAVE_LIBSERVICE)
    case FILETYPE_SRV:
      {
        fileID = fileOpen(filename, "r");
	record = (Record *) malloc(sizeof(Record));
	record->buffer = NULL;
	record->srvp   = srvNew();
	break;
      }
#endif
#if  defined  (HAVE_LIBEXTRA)
    case FILETYPE_EXT:
      {
        fileID = fileOpen(filename, "r");
	record = (Record *) malloc(sizeof(Record));
	record->buffer = NULL;
	record->extp   = extNew();
	break;
      }
#endif
#if  defined  (HAVE_LIBIEG)
    case FILETYPE_IEG:
      {
        fileID = fileOpen(filename, "r");
	record = (Record *) malloc(sizeof(Record));
	record->buffer = NULL;
	record->iegp   = iegNew();
	break;
      }
#endif
#if  defined  (HAVE_LIBNETCDF)
    case FILETYPE_NC:
      {
	fileID = cdfOpen(filename, "r");
	break;
      }
    case FILETYPE_NC2:
      {
	fileID = cdfOpen64(filename, "r");
	break;
      }
#endif
#if  defined  (HAVE_LIBHDF5)
    case FILETYPE_H5:
      {
	fileID = hdfOpen(filename, "r");
	break;
      }
#endif
    default:
      {
	if ( CDI_Debug ) Message(func, "%s library not available!", strfiletype(filetype));
	return (CDI_ELIBNAVAIL);
      }
    }

  if ( fileID == CDI_UNDEFID || fileID == CDI_ELIBNAVAIL )
    {
      streamID = fileID;
      return (streamID);
    }
  else
    {
      VLIST *vlistptr;
      streamID = streamsNewEntry(filetype);

      streams[streamID].record   = record;
      streams[streamID].filetype = filetype;
      streams[streamID].filemode = tolower(*filemode);
      streams[streamID].filename = strdupx(filename);
      streams[streamID].fileID   = fileID;

      streams[streamID].vlistID = vlistNew();
      /* cdiReadByteorder(streamID); */
      status = cdiInqContents(streamID);
      if ( status < 0 ) return (status);
      vlistptr = vlist_to_pointer(streams[streamID].vlistID);
      vlistptr->ntsteps = cdiInqTimeSize(streamID);
    }
 
  switch (filetype)
    {
#if  defined  (HAVE_LIBGRIB)
      case FILETYPE_GRB:
	{
	  gribClose(fileID);
	  break;
	}
#endif
#if  defined  (HAVE_LIBSERVICE)
      case FILETYPE_SRV:
	{
	  fileClose(fileID);
	  break;
	}
#endif
#if  defined  (HAVE_LIBEXTRA)
      case FILETYPE_EXT:
	{
	  fileClose(fileID);
	  break;
	}
#endif
#if  defined  (HAVE_LIBIEG)
      case FILETYPE_IEG:
	{
	  fileClose(fileID);
	  break;
	}
#endif
#if  defined  (HAVE_LIBNETCDF)
      case FILETYPE_NC:
      case FILETYPE_NC2:
	{
	  cdfClose(fileID);
	  break;
	}
#endif
#if  defined  (HAVE_LIBHDF5)
      case FILETYPE_H5:
	{
	  hdfClose(fileID);
	  break;
	}
#endif
      default:
	{
	  if ( CDI_Debug ) Message(func, "%s library not available!", strfiletype(filetype));
	  return (CDI_ELIBNAVAIL);
	}
    }

  switch (filetype)
    {
#if  defined  (HAVE_LIBGRIB)
    case FILETYPE_GRB:
      {
        fileID = gribOpen(filename, filemode);
	break;
      }
#endif
#if  defined  (HAVE_LIBSERVICE)
    case FILETYPE_SRV:
      {
        fileID = fileOpen(filename, filemode);
	break;
      }
#endif
#if  defined  (HAVE_LIBEXTRA)
    case FILETYPE_EXT:
      {
        fileID = fileOpen(filename, filemode);
	break;
      }
#endif
#if  defined  (HAVE_LIBIEG)
    case FILETYPE_IEG:
      {
        fileID = fileOpen(filename, filemode);
	break;
      }
#endif
#if  defined  (HAVE_LIBNETCDF)
    case FILETYPE_NC:
      {
	fileID = cdfOpen(filename, filemode);
	break;
      }
    case FILETYPE_NC2:
      {
	fileID = cdfOpen64(filename, filemode);
	break;
      }
#endif
#if  defined  (HAVE_LIBHDF5)
    case FILETYPE_H5:
      {
	fileID = hdfOpen(filename, filemode);
	break;
      }
#endif
    default:
      {
	if ( CDI_Debug ) Message(func, "%s library not available!", strfiletype(filetype));
	return (CDI_ELIBNAVAIL);
      }
    }

  if ( fileID == CDI_UNDEFID )
    streamID = CDI_UNDEFID;
  else
    streams[streamID].fileID   = fileID;

  return (streamID);
}


/*
@Function  streamOpenRead
@Title     Open a file for reading

@Prototype int streamOpenRead(const char *filename)
@Parameter
    @Item  filename  The file name of the dataset to be read

@Description
The function {\tt streamOpenRead} opens an existing file for reading.

@Result
Upon successful completion {\tt streamOpenRead} return an identifier to the
open stream. Otherwise, a negative number with the error status is returned.

@Errors
@List
   @Item  CDI_ESYSTEM     Operating system error
   @Item  CDI_EINVAL      Invalid argument
   @Item  CDI_EUFILETYPE  Unsupported file type
   @Item  CDI_ELIBNAVAIL  Library not available
@EndList

@Example
Here is an example using {\tt streamOpenRead} to open an existing netCDF
file named {\tt foo.nc} for reading:

@Source
#include <cdi.h>
   ...
int streamID;
   ...
streamID = streamOpenRead("foo.nc");
if ( streamID < 0 ) handle_error(streamID);
   ...
@EndSource
@EndFunction
*/
int streamOpenRead(const char *filename)
{
  int filetype, byteorder;
  int streamID;

  filetype = getFiletype(filename, &byteorder);

  if ( filetype < 0 ) return (filetype);

  streamID = streamOpen(filename, "r", filetype);

  streams[streamID].byteorder = byteorder;

  return (streamID);
}


int streamOpenAppend(const char *filename)
{
  int filetype, byteorder;
  int streamID;

  filetype = getFiletype(filename, &byteorder);

  if ( filetype < 0 ) return (filetype);

  streamID = streamOpenA(filename, "a", filetype);

  streams[streamID].byteorder = byteorder;

  return (streamID);
}


/*
@Function  streamOpenWrite
@Title     Open a file for writing

@Prototype int streamOpenWrite(const char *filename, int filetype)
@Parameter
    @Item  filename  The file name of the dataset to be read
    @Item  filetype  The type of the file format, one of the set of predefined CDI file format types.
                     The valid CDI file format types are {\tt FILETYPE_GRB}, {\tt FILETYPE_NC},
                     {\tt FILETYPE_NC2}, {\tt FILETYPE_SRV}, and {\tt FILETYPE_EXT}.

@Description
The function {\tt streamOpenWrite} create a new file for writing.

@Result
Upon successful completion {\tt streamOpenWrite} return an identifier to the
open stream. Otherwise, a negative number with the error status is returned.

@Errors
@List
   @Item  CDI_ESYSTEM     Operating system error
   @Item  CDI_EINVAL      Invalid argument
   @Item  CDI_EUFILETYPE  Unsupported file type
   @Item  CDI_ELIBNAVAIL  Library not available
@EndList

@Example
Here is an example using {\tt streamOpenWrite} to create a new netCDF file 
named {\tt foo.nc} for writing:

@Source
#include <cdi.h>
   ...
int streamID;
   ...
streamID = streamOpenWrite("foo.nc", FILETYPE_NC);
if ( streamID < 0 ) handle_error(streamID);
   ...
@EndSource
@EndFunction
*/
int streamOpenWrite(const char *filename, int filetype)
{
  return (streamOpen(filename, "w", filetype));
}


/*
@Function  streamClose
@Title     Close an Open file

@Prototype  void streamClose(int streamID)
@Parameter
    @Item  streamID  Identifier, from a previous call to {\tt streamOpenRead}
                     or {\tt streamOpenWrite}

@Description
The function {\tt streamClose} closes an open file.

@EndFunction
*/
void streamClose(int streamID)
{
  static char func[] = "streamClose";
  int filetype;
  int fileID;
  int index;
  int vlistID;

  streamCheckID(func, streamID);

  if ( CDI_Debug )
    Message(func, "fileID = %d filename = %s", streamID, streams[streamID].filename);

  fileID   = streams[streamID].fileID;
  filetype = streams[streamID].filetype;
  vlistID  = streamInqVlist(streamID);

  if ( fileID == CDI_UNDEFID )
    Warning(func, "File %s not open!", streams[streamID].filename);
  else
    switch (filetype)
      {
#if  defined  (HAVE_LIBGRIB)
      case FILETYPE_GRB:
	{
	  gribClose(fileID);
	  break;
	}
#endif
#if  defined  (HAVE_LIBSERVICE)
      case FILETYPE_SRV:
	{
	  fileClose(fileID);
	  srvDelete(streams[streamID].record->srvp);
	  break;
	}
#endif
#if  defined  (HAVE_LIBEXTRA)
      case FILETYPE_EXT:
	{
	  fileClose(fileID);
	  extDelete(streams[streamID].record->extp);
	  break;
	}
#endif
#if  defined  (HAVE_LIBIEG)
      case FILETYPE_IEG:
	{
	  fileClose(fileID);
	  iegDelete(streams[streamID].record->iegp);
	  break;
	}
#endif
#if  defined  (HAVE_LIBNETCDF)
      case FILETYPE_NC:
      case FILETYPE_NC2:
	{
	  cdfClose(fileID);
	  break;
	}
#endif
#if  defined  (HAVE_LIBHDF5)
      case FILETYPE_H5:
	{
	  hdfClose(fileID);
	  break;
	}
#endif
      default:
	{
	  Error(func, "%s library not available!", strfiletype(filetype));
	  break;
	}
      }

  if ( streams[streamID].record )
    {
      if ( streams[streamID].record->buffer )
	free(streams[streamID].record->buffer);

      free(streams[streamID].record);
    }  

  streams[streamID].filetype = 0;
  if ( streams[streamID].filename ) free(streams[streamID].filename);

  for ( index = 0; index < streams[streamID].nvars; index++ )
    {
      if ( streams[streamID].vars[index].level )
	free(streams[streamID].vars[index].level);
      if ( streams[streamID].vars[index].lindex )
	free(streams[streamID].vars[index].lindex);
    }
  free(streams[streamID].vars);

  for ( index = 0; index < streams[streamID].ntsteps; index++ )
    {
      if ( streams[streamID].tsteps[index].records )
	free(streams[streamID].tsteps[index].records);
      if ( streams[streamID].tsteps[index].recIDs )
	free(streams[streamID].tsteps[index].recIDs);
    }
    
  if ( streams[streamID].tsteps ) free(streams[streamID].tsteps);

  streamsInitEntry(streamID);

  if ( vlistID != -1 )
    {
      vlistFree(vlistID);
    }
}


/*
@Function  streamDefTimestep
@Title     Define timestep

@Prototype int streamDefTimestep(int streamID, int tsID)
@Parameter
    @Item  streamID  Stream identifier
    @Item  tsID      Timestep identifier

@Description
The function {\tt streamDefTimestep} defines the timestep of a stream.

@Result
{\tt streamDefTimestep} return the number of records of the timestep.

@EndFunction
*/
int streamDefTimestep(int streamID, int tsID)
{
  static char func[] = "streamDefTimestep";
  int newtsID;
  int taxisID;
  int vlistID;

  if ( CDI_Debug )
    Message(func, "streamID = %d  tsID = %d", streamID, tsID);

  streamCheckID(func, streamID);

  vlistID = streamInqVlist(streamID);
  taxisID = vlistInqTaxis(vlistID);
  if ( taxisID == CDI_UNDEFID )
    {
      Warning(func, "taxisID undefined for fileID = %d! Using absolute time axis.", streamID);
      taxisID = taxisNew(TAXIS_ABSOLUTE);
      vlistDefTaxis(vlistID, taxisID);
    }

  newtsID = tstepsNewEntry(streamID);

  if ( tsID != newtsID )
    Error(func, "Internal problem: tsID = %d newtsID = %d", tsID, newtsID);

  streams[streamID].curTsID = tsID;

  ptaxisCopy(&streams[streamID].tsteps[tsID].taxis, taxisPtr(taxisID));

  streams[streamID].ntsteps = tsID + 1;

  if ( (streams[streamID].filetype == FILETYPE_NC || streams[streamID].filetype == FILETYPE_NC2)
       && vlistHasTime(vlistID) )
    cdfDefTimestep(streamID, tsID);

  if ( streams[streamID].filetype == FILETYPE_H5 && vlistHasTime(vlistID) )
    hdfDefTimestep(streamID, tsID);

  cdiCreateRecords(streamID, tsID);

  return (streams[streamID].ntsteps);
}


/*
@Function  streamInqTimestep
@Title     Get timestep

@Prototype int streamInqTimestep(int streamID, int tsID)
@Parameter
    @Item  streamID  Stream identifier
    @Item  tsID      Timestep identifier

@Description
The function {\tt streamInqTimestep} gets the timestep of a stream.

@Result
{\tt streamInqTimestep} return the number of records of the timestep.

@EndFunction
*/
int streamInqTimestep(int streamID, int tsID)
{
  static char func[] = "streamInqTimestep";
  int filetype;
  int nrecs = 0;
  int taxisID;
  int vlistID;

  streamCheckID(func, streamID);

  vlistID = streamInqVlist(streamID);

  if ( tsID < streams[streamID].rtsteps )
    {
      streams[streamID].curTsID = tsID;
      nrecs = streams[streamID].tsteps[tsID].nrecs;
      streams[streamID].tsteps[tsID].curRecID = CDI_UNDEFID;
      taxisID = vlistInqTaxis(vlistID);
      if ( taxisID == -1 )
	Error(func, "Timestep undefined for fileID = %d", streamID);
      ptaxisCopy(taxisPtr(taxisID), &streams[streamID].tsteps[tsID].taxis);

      return (nrecs);
    }

  if ( tsID >= streams[streamID].ntsteps && streams[streamID].ntsteps != CDI_UNDEFID )
    {
      return (0);
    }

  filetype = streams[streamID].filetype;

  if ( CDI_Debug )
    Message(func, "streamID = %d  tsID = %d  filetype = %d", streamID, tsID, filetype);

  switch (filetype)
    {
#if  defined  (HAVE_LIBGRIB)
    case FILETYPE_GRB:
      {
        nrecs = grbInqTimestep(streamID, tsID);
	break;
      }
#endif
#if  defined  (HAVE_LIBSERVICE)
    case FILETYPE_SRV:
      {
        nrecs = srvInqTimestep(streamID, tsID);
	break;
      }
#endif
#if  defined  (HAVE_LIBEXTRA)
    case FILETYPE_EXT:
      {
        nrecs = extInqTimestep(streamID, tsID);
	break;
      }
#endif
#if  defined  (HAVE_LIBIEG)
    case FILETYPE_IEG:
      {
        nrecs = iegInqTimestep(streamID, tsID);
	break;
      }
#endif
#if  defined  (HAVE_LIBNETCDF)
    case FILETYPE_NC:
    case FILETYPE_NC2:
      {
        nrecs = cdfInqTimestep(streamID, tsID);
	break;
      }
#endif
    default:
      {
	Error(func, "%s library not available!", strfiletype(filetype));
	break;
      }
    }

  taxisID = vlistInqTaxis(vlistID);
  if ( taxisID == -1 )
    Error(func, "Timestep undefined for fileID = %d", streamID);

  ptaxisCopy(taxisPtr(taxisID), &streams[streamID].tsteps[tsID].taxis);

  return (nrecs);
}


/*
@Function  streamReadVar
@Title     Read a variable

@Prototype void streamReadVar(int streamID, int varID, double *data, int *nmiss)
@Parameter
    @Item  streamID  Stream identifier
    @Item  varID     Variable identifier
    @Item  data      Data
    @Item  nmiss     Number of missing values

@EndFunction
*/
void streamReadVar(int streamID, int varID, double *data, int *nmiss)
{
  static char func[] = "streamReadVar";
  int filetype;

  if ( CDI_Debug )
    Message(func, "streamID = %d  varID = %d", streamID, varID);

  streamCheckID(func, streamID);

  filetype = streams[streamID].filetype;

  *nmiss = 0;

  switch (filetype)
    {
#if  defined  (HAVE_LIBGRIB)
    case FILETYPE_GRB:
      {
        grbReadVarDP(streamID, varID, data, nmiss);
	break;
      }
#endif
#if  defined  (HAVE_LIBSERVICE)
    case FILETYPE_SRV:
      {
        srvReadVarDP(streamID, varID, data, nmiss);
	break;
      }
#endif
#if  defined  (HAVE_LIBEXTRA)
    case FILETYPE_EXT:
      {
        extReadVarDP(streamID, varID, data, nmiss);
	break;
      }
#endif
#if  defined  (HAVE_LIBIEG)
    case FILETYPE_IEG:
      {
        iegReadVarDP(streamID, varID, data, nmiss);
	break;
      }
#endif
#if  defined  (HAVE_LIBNETCDF)
    case FILETYPE_NC:
    case FILETYPE_NC2:
      {
        cdfReadVarDP(streamID, varID, data, nmiss);
	break;
      }
#endif
    default:
      {
	Error(func, "%s library not available!", strfiletype(filetype));
	break;
      }
    }
}


/*
@Function  streamWriteVar
@Title     Write a variable

@Prototype void streamWriteVar(int streamID, int varID, double *data, int nmiss)
@Parameter
    @Item  streamID  Stream identifier
    @Item  varID     Variable identifier
    @Item  data      Data
    @Item  nmiss     Number of missing values

@EndFunction
*/
void streamWriteVar(int streamID, int varID, double *data, int nmiss)
{
  static char func[] = "streamWriteVar";
  int filetype;

  if ( CDI_Debug )
    Message(func, "streamID = %d varID = %d", streamID, varID);

  streamCheckID(func, streamID);

  streamDefineTaxis(streamID);

  filetype = streams[streamID].filetype;

  switch (filetype)
    {
#if  defined  (HAVE_LIBGRIB)
    case FILETYPE_GRB:
      {
        grbWriteVarDP(streamID, varID, data, nmiss);
	break;
      }
#endif
#if  defined  (HAVE_LIBSERVICE)
    case FILETYPE_SRV:
      {
        srvWriteVarDP(streamID, varID, data);
	break;
      }
#endif
#if  defined  (HAVE_LIBEXTRA)
    case FILETYPE_EXT:
      {
        extWriteVarDP(streamID, varID, data);
	break;
      }
#endif
#if  defined  (HAVE_LIBIEG)
    case FILETYPE_IEG:
      {
        iegWriteVarDP(streamID, varID, data);
	break;
      }
#endif
#if  defined  (HAVE_LIBNETCDF)
    case FILETYPE_NC:
    case FILETYPE_NC2:
      {
	if ( streams[streamID].accessmode == 0 ) cdfEndDef(streamID);
        cdfWriteVarDP(streamID, varID, data, nmiss);
	break;
      }
#endif
    default:
      {
	Error(func, "%s library not available!", strfiletype(filetype));
	break;
      }
    }
}


/*
@Function  streamReadVarSlice
@Title     Read a horizontal slice of a variable

@Prototype void streamReadVarSlice(int streamID, int varID, int levelID, double *data, int *nmiss)
@Parameter
    @Item  streamID  Stream identifier
    @Item  varID     Variable identifier
    @Item  levelID   Level identifier
    @Item  data      Data
    @Item  nmiss     Number of missing values

@EndFunction
*/
void streamReadVarSlice(int streamID, int varID, int levelID, double *data, int *nmiss)
{
  static char func[] = "streamReadVarSlice";
  int filetype;
  int ierr = 0;

  streamCheckID(func, streamID);

  filetype = streams[streamID].filetype;

  *nmiss = 0;

  switch (filetype)
    {
#if  defined  (HAVE_LIBGRIB)
    case FILETYPE_GRB:
      {
        grbReadVarSliceDP(streamID, varID, levelID, data, nmiss);
	break;
      }
#endif
#if  defined  (HAVE_LIBSERVICE)
    case FILETYPE_SRV:
      {
        srvReadVarSliceDP(streamID, varID, levelID, data, nmiss);
	break;
      }
#endif
#if  defined  (HAVE_LIBEXTRA)
    case FILETYPE_EXT:
      {
        extReadVarSliceDP(streamID, varID, levelID, data, nmiss);
	break;
      }
#endif
#if  defined  (HAVE_LIBIEG)
    case FILETYPE_IEG:
      {
        iegReadVarSliceDP(streamID, varID, levelID, data, nmiss);
	break;
      }
#endif
#if  defined  (HAVE_LIBNETCDF)
    case FILETYPE_NC:
    case FILETYPE_NC2:
      {
        ierr = cdfReadVarSliceDP(streamID, varID, levelID, data, nmiss);
	break;
      }
#endif
    default:
      {
	Error(func, "%s library not available!", strfiletype(filetype));
	break;
      }
    }
}


/*
@Function  streamWriteVarSlice
@Title     Write a horizontal slice of a variable

@Prototype void streamWriteVarSlice(int streamID, int varID, int levelID, double *data, int nmiss)
@Parameter
    @Item  streamID  Stream identifier
    @Item  varID     Variable identifier
    @Item  levelID   Level identifier
    @Item  data      Data
    @Item  nmiss     Number of missing values

@EndFunction
*/
void streamWriteVarSlice(int streamID, int varID, int levelID, double *data, int nmiss)
{
  static char func[] = "streamWriteVarSlice";
  int filetype;
  int ierr = 0;

  if ( CDI_Debug )
    Message(func, "streamID = %d varID = %d", streamID, varID);

  streamCheckID(func, streamID);

  filetype = streams[streamID].filetype;

  switch (filetype)
    {
#if  defined  (HAVE_LIBGRIB)
    case FILETYPE_GRB:
      {
        grbWriteVarSliceDP(streamID, varID, levelID, data, nmiss);
	break;
      }
#endif
#if  defined  (HAVE_LIBSERVICE)
    case FILETYPE_SRV:
      {
        srvWriteVarSliceDP(streamID, varID, levelID, data);
	break;
      }
#endif
#if  defined  (HAVE_LIBEXTRA)
    case FILETYPE_EXT:
      {
        extWriteVarSliceDP(streamID, varID, levelID, data);
	break;
      }
#endif
#if  defined  (HAVE_LIBIEG)
    case FILETYPE_IEG:
      {
        iegWriteVarSliceDP(streamID, varID, levelID, data);
	break;
      }
#endif
#if  defined  (HAVE_LIBNETCDF)
    case FILETYPE_NC:
    case FILETYPE_NC2:
      {
	if ( streams[streamID].accessmode == 0 ) cdfEndDef(streamID);
        ierr = cdfWriteVarSliceDP(streamID, varID, levelID, data, nmiss);
	break;
      }
#endif
#if  defined  (HAVE_LIBHDF5)
    case FILETYPE_H5:
      {
	ierr = hdfWriteVarSliceDP(streamID, varID, levelID, data, nmiss);
	break;
      }
#endif
    default:
      {
	Error(func, "%s library not available!", strfiletype(filetype));
	break;
      }
    }
}

void streamWriteContents(int streamID, char *cname)
{
  static char func[] = "streamWriteContents";
  FILE *cnp;
  int tsID, recID, varID, levelID;
  long recsize;
  int nrecs, nvars;
  int code, gridID, zaxisID, timeID, datatype;
  int ngrids, nzaxis;
  int filetype, gridtype;
  int xsize, ysize;
  int date, time;
  int i;
  off_t recpos, position;
  int vlistID;

  streamCheckID(func, streamID);

  vlistID = streamInqVlist(streamID);

  cnp = fopen(cname, "w");

  if ( cnp == NULL ) SysError(func, cname);

  fprintf(cnp, "#CDI library version %s\n", cdiLibraryVersion());
  fprintf(cnp, "#\n");

  fprintf(cnp, "filename: %s\n", streams[streamID].filename);
  filetype = streams[streamID].filetype;
  fprintf(cnp, "filetype: %s\n", strfiletype(filetype));

  fprintf(cnp, "#\n");
  fprintf(cnp, "#grids:\n");

  ngrids = vlistNgrids(vlistID);
  for ( i = 0; i < ngrids; i++ )
    {
      gridID   = vlistGrid(vlistID, i);
      gridtype = gridInqType(gridID);
      xsize    = gridInqXsize(gridID);
      ysize    = gridInqYsize(gridID);
      fprintf(cnp, "%4d:%4d:%4d:%4d\n", i+1, gridtype, xsize, ysize);
    }
  nzaxis = vlistNzaxis(vlistID);

  fprintf(cnp, "#\n");

  fprintf(cnp, "varID:code:gridID:zaxisID:timeID:datatype\n");

  nvars = vlistNvars(vlistID);
  for ( varID = 0; varID < nvars; varID++ )
    {
      code     = vlistInqVarCode(vlistID, varID);
      gridID   = vlistInqVarGrid(vlistID, varID);
      zaxisID  = vlistInqVarZaxis(vlistID, varID);
      timeID   = vlistInqVarTime(vlistID, varID);
      datatype = vlistInqVarDatatype(vlistID, varID);
      fprintf(cnp, "%4d:%4d:%4d:%4d:%4d:%4d:\n",
	      varID+1, code, gridID, zaxisID, timeID, datatype);
    }

  fprintf(cnp, "#\n");

  fprintf(cnp, "tsID:nrecs:date:time\n");
  
  tsID = 0;
  while (1)
    {
      nrecs = streams[streamID].tsteps[tsID].nallrecs;
      date  = streams[streamID].tsteps[tsID].taxis.vdate;
      time  = streams[streamID].tsteps[tsID].taxis.vtime;
      position = streams[streamID].tsteps[tsID].position;

      fprintf(cnp, "%4d:%4d:%4d:%4d:%ld\n",
	      tsID, nrecs, date, time, (long) position);

      if ( streams[streamID].tsteps[tsID].next )
	tsID++;
      else
	break;
    }

  fprintf(cnp, "#\n");

  fprintf(cnp, "tsID:recID:varID:levID:size:pos\n");

  tsID = 0;
  while (1)
    {
      nrecs = streams[streamID].tsteps[tsID].nallrecs;
      for ( recID = 0; recID < nrecs; recID++ )
	{
	  varID   = streams[streamID].tsteps[tsID].records[recID].varID;
	  levelID = streams[streamID].tsteps[tsID].records[recID].levelID;
	  recpos  = streams[streamID].tsteps[tsID].records[recID].position;
	  recsize = streams[streamID].tsteps[tsID].records[recID].size;
	  fprintf(cnp, "%4d:%4d:%4d:%4d:%4ld:%ld\n",
		  tsID, recID, varID, levelID, recsize, (long) recpos);
	}

      if ( streams[streamID].tsteps[tsID].next )
	tsID++;
      else
	break;
    }

  fclose(cnp);
}


void cdiDefTableID(int tableID)
{
  int modelID, instID;

  cdiDefaultTableID = tableID;

  modelID = tableInqModel(tableID);
  cdiDefaultModelID = modelID;

  instID = modelInqInstitut(modelID);
  cdiDefaultInstID = instID;
}


void cdiPrintVersion(void)
{
  fprintf(stderr, "    CDI library version : %s\n", cdiLibraryVersion());
#if  defined  (HAVE_LIBGRIB)
  fprintf(stderr, "   GRIB library version : %s\n", gribLibraryVersion());
#endif
#if  defined  (HAVE_LIBNETCDF)
#  if  defined  (HAVE_LIBNC_DAP)
  fprintf(stderr, " nc_dap library version : %s\n", cdfLibraryVersion());
#  else
  fprintf(stderr, " netCDF library version : %s\n", cdfLibraryVersion());
#  endif
#endif
#if  defined  (HAVE_LIBHDF5)
  fprintf(stderr, "   HDF5 library version : %s\n", hdfLibraryVersion());
#endif
#if  defined  (HAVE_LIBSERVICE)
  fprintf(stderr, "SERVICE library version : %s\n", srvLibraryVersion());
#endif
#if  defined  (HAVE_LIBEXTRA)
  fprintf(stderr, "  EXTRA library version : %s\n", extLibraryVersion());
#endif
#if  defined  (HAVE_LIBIEG)
  fprintf(stderr, "    IEG library version : %s\n", iegLibraryVersion());
#endif
  fprintf(stderr, "   FILE library version : %s\n", fileLibraryVersion());
}


int streamNtsteps(int streamID)
{
  static char func[] = "streamNtsteps";

  streamCheckID(func, streamID);

  return (streams[streamID].ntsteps);
}


/*
@Function  streamDefVlist
@Title     Define the variable list

@Prototype streamDefVlist(int streamID, int vlistID)
@Parameter
    @Item  streamID  Stream identifier
    @Item  vlistID   Variable list identifier

@Description
The function {\tt streamDefVlist} defines the variable list of a stream.

@EndFunction
*/
void streamDefVlist(int streamID, int vlistID)
{
  static char func[] = "streamDefVlist";
  int nvars, varID;
  int gridID, zaxisID;

  streamCheckID(func, streamID);

  if ( streams[streamID].vlistID == CDI_UNDEFID )
    {
      streams[streamID].vlistID = vlistDuplicate(vlistID);

      nvars = vlistNvars(vlistID);
      for ( varID = 0; varID < nvars; varID++ )
	{
	  gridID  = vlistInqVarGrid(vlistID, varID);
	  zaxisID = vlistInqVarZaxis(vlistID, varID);
	  streamNewVar(streamID, gridID, zaxisID);
	}

      if ( streams[streamID].filemode == 'w' )
	{
	  if ( streams[streamID].filetype == FILETYPE_NC || streams[streamID].filetype == FILETYPE_NC2 )
	    cdfDefVars(streamID);
	}
    }
  else
    {
      Warning(func, "vlist allready defined for %s!", streams[streamID].filename);
    }
}


/*
@Function  streamInqVlist
@Title     Get the variable list

@Prototype int streamInqVlist(int streamID)
@Parameter
    @Item  streamID  Stream identifier

@Description
The function {\tt streamInqVlist} gets the variable list of a stream.

@Result
{\tt streamInqVlist} return an identifier to the variable list.

@EndFunction
*/
int streamInqVlist(int streamID)
{
  static char func[] = "streamInqVlist";
  
  streamCheckID(func, streamID);

  return (streams[streamID].vlistID);
}
