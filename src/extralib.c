#if defined (HAVE_CONFIG_H)
#  include "config.h"
#endif

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>

#include "dmemory.h"

#include "extra.h"
#include "error.h"
#include "file.h"
#include "binary.h"
#include "swap.h"


#define SINGLE_PRECISION  4
#define DOUBLE_PRECISION  8

#define EXT_HEADER_LEN  4


static int initExtLib = 0;
static int extDefaultHprec = 0;
static int extDefaultDprec = 0;


/*
 * A version string.
 */

#undef  LIBVERSION
#define LIBVERSION      1.0.2
#define XSTRING(x)	#x
#define STRING(x)	XSTRING(x)
static const char ext_libvers[] = STRING(LIBVERSION) " of "__DATE__" "__TIME__;

const char *extLibraryVersion(void)
{
  return (ext_libvers);
}


int EXT_Debug = 0;    /* If set to 1, debugging */


void extDebug(int debug)
{
  static char func[] = "extDebug";

  EXT_Debug = debug;

  if ( EXT_Debug )
    Message(func, "debug level %d", debug);
}


void extLibInit()
{
  static char func[] = "extLibInit";
  char *envString;
  char *envName = "EXT_PRECISION";


  envString = getenv(envName);
  if ( envString )
    {
      int pos;
      int nrun;
      if ( strlen(envString) == 2 ) nrun = 1;
      else                          nrun = 2;

      pos = 0;
      while ( nrun-- )
	{
	  switch ( tolower((int) envString[pos]) )
	    {
	    case 'i':
	      {
		switch ( (int) envString[pos+1] )
		  {
		  case '4': extDefaultHprec = SINGLE_PRECISION; break;
		  case '8': extDefaultHprec = DOUBLE_PRECISION; break;
		  default:
		    Message(func, "Invalid digit in %s: %s", envName, envString);
		  }
		break;
	      }
	    case 'r':
	      {
		switch ( (int) envString[pos+1] )
		  {
		  case '4': extDefaultDprec = SINGLE_PRECISION; break;
		  case '8': extDefaultDprec = DOUBLE_PRECISION; break;
		  default:
		    Message(func, "Invalid digit in %s: %s", envName, envString);
		  }
		break;		
	      }
	    default:
	      Message(func, "Invalid character in %s: %s", envName, envString);
	    }
	  pos += 2;
	}
    }

  initExtLib = 1;
}


void extInit(EXTREC *extp)
{
  extp->checked    = 0;
  extp->byteswap   = 0;
  extp->hprec      = 0;
  extp->dprec      = 0;
  extp->datasize   = 0;
  extp->buffersize = 0;
  extp->buffer     = NULL;
}


EXTREC *extNew(void)
{
  static char func[] = "extNew";
  EXTREC *extp;

  if ( ! initExtLib ) extLibInit();

  extp = (EXTREC *) malloc(sizeof(EXTREC));

  extInit(extp);

  return (extp);
}


void extDelete(EXTREC *extp)
{
  static char func[] = "extDelete";

  if ( extp )
    {
      if ( extp->buffer ) free(extp->buffer);
      free(extp);
    }
}


int extCheckFiletype(int fileID, int *swap)
{
  static char func[] = "extCheckFiletype";
  size_t blocklen = 0;
  size_t sblocklen = 0;
  size_t data =  0;
  size_t dimxy = 0;
  int fact = 0, found = 0;
  unsigned char buffer[40], *pbuf;

  if ( fileRead(fileID, buffer, 4) != 4 ) return (found);

  blocklen  = (size_t) get_UINT32(buffer);
  sblocklen = (size_t) get_SUINT32(buffer);

  if ( EXT_Debug )
    Message(func, "blocklen = %d sblocklen = %d", blocklen, sblocklen);

  if ( blocklen == 16 )
    {
     *swap = 0;
      fact = blocklen/4;
      if ( fileRead(fileID, buffer, blocklen+8) != blocklen+8 ) return (found);
      pbuf = buffer+3*fact;      dimxy = (size_t) get_UINT32(pbuf);
      pbuf = buffer+blocklen+4;  data  = (size_t) get_UINT32(pbuf);
    }
  else if ( blocklen == 32 )
    {
     *swap = 0;
      fact = blocklen/4;
      if ( fileRead(fileID, buffer, blocklen+8) != blocklen+8 ) return (found);
      pbuf = buffer+3*fact;      dimxy = (size_t) get_UINT64(pbuf);
      pbuf = buffer+blocklen+4;  data  = (size_t) get_UINT32(pbuf);
    }
  else if ( sblocklen == 16 )
    {
     *swap = 1;
      fact = sblocklen/4;
      if ( fileRead(fileID, buffer, sblocklen+8) != sblocklen+8 ) return (found);
      pbuf = buffer+3*fact;       dimxy = (size_t) get_SUINT32(pbuf);
      pbuf = buffer+sblocklen+4;  data  = (size_t) get_SUINT32(pbuf);
    }
  else if ( sblocklen == 32 )
    {
     *swap = 1;
      fact = sblocklen/4;
      if ( fileRead(fileID, buffer, sblocklen+8) != sblocklen+8 ) return (found);
      pbuf = buffer+3*fact;       dimxy = (size_t) get_SUINT64(pbuf);
      pbuf = buffer+sblocklen+4;  data  = (size_t) get_SUINT32(pbuf);
    }

  fileRewind(fileID);

  if      ( data && dimxy*fact == data ) found = 1;
  else if ( data && dimxy*8    == data ) found = 1;

  if ( EXT_Debug )
    {
      Message(func, "swap = %d fact = %d", *swap, fact);
      Message(func, "dimxy = %lu data = %lu", dimxy, data);
    }

  return (found);
}


int extInqHeader(EXTREC *extp, int *header)
{
  static char func[] = "extInqHeader";
  size_t i;


  for ( i = 0; i < EXT_HEADER_LEN; i++ )
    header[i] = extp->header[i];
  
  if ( EXT_Debug )
    Message(func, "datasize = %lu", extp->datasize);

  return (0);
}


int extDefHeader(EXTREC *extp, const int *header)
{
  static char func[] = "extDefHeader";
  size_t i;


  for ( i = 0; i < EXT_HEADER_LEN; i++ )
    extp->header[i] = header[i];
  
  extp->datasize = header[3];

  if ( EXT_Debug )
    Message(func, "datasize = %lu", extp->datasize);

  return (0);
}


int extInqData(EXTREC *extp, int prec, void *data)
{
  static char func[] = "extInqData";
  size_t datasize;
  size_t i;
  int ierr = 0;
  int dprec;
  void *buffer;
  int byteswap = extp->byteswap;


  datasize = extp->datasize;

  buffer = extp->buffer;

  dprec = extp->dprec;

  switch ( dprec )
    {
    case SINGLE_PRECISION:
      {
	if ( sizeof(FLT32) == 4 )
	  {
	    if ( byteswap ) swap4byte(buffer, datasize);

	    if ( dprec == prec )
	      memcpy(data, buffer, datasize*sizeof(FLT32));
	    else
	      for (i = 0; i < datasize; i++)
		((double *) data)[i] = (double) ((float *) buffer)[i];
	  }
	else
	  {
	    Error(func, "not implemented for %d byte float", sizeof(FLT32));
	  }	
	break;
      }
    case DOUBLE_PRECISION:
	if ( sizeof(FLT64) == 8 )
	  {
	    if ( byteswap ) swap8byte(buffer, datasize);

	    if ( dprec == prec )
	      memcpy(data, buffer, datasize*sizeof(FLT64));
	    else
	      for (i = 0; i < datasize; i++)
		((float *) data)[i] = (float) ((double *) buffer)[i];
	  }
	else
	  {
	    Error(func, "not implemented for %d byte float", sizeof(FLT64));
	  }	
	break;
    default:
      {
	Error(func, "unexpected data precision %d", dprec);
      }
    }

  return (ierr);
}


int extInqDataSP(EXTREC *extp, float *data)
{
  return (extInqData(extp, SINGLE_PRECISION, (void *) data));
}


int extInqDataDP(EXTREC *extp, double *data)
{
  return (extInqData(extp, DOUBLE_PRECISION, (void *) data));
}


int extDefData(EXTREC *extp, int prec, const void *data)
{
  static char func[] = "extDefData";
  size_t datasize;
  size_t blocklen;
  size_t buffersize;
  size_t i;
  int dprec, hprec;
  int *header;
  void *buffer;


  if ( extDefaultDprec ) dprec = extDefaultDprec;
  else                   dprec = extp->dprec;

  if ( ! dprec ) dprec = prec;

  extp->dprec = dprec;

  if ( extDefaultHprec ) hprec = extDefaultHprec;
  else                   hprec = extp->hprec;

  if ( ! hprec ) hprec = dprec;
  
  extp->hprec = hprec;

  header = extp->header;

  datasize = header[3];
  blocklen = datasize * dprec;

  extp->datasize = datasize;

  buffersize = extp->buffersize;

  if ( buffersize != blocklen )
    {
      buffersize = blocklen;
      buffer = extp->buffer;
      buffer = realloc(buffer, buffersize);
      extp->buffer = buffer;
      extp->buffersize = buffersize;
    }
  else
    buffer = extp->buffer;

  switch ( dprec )
    {
    case SINGLE_PRECISION:
      {
	if ( dprec == prec )
	  memcpy(buffer, data, datasize*sizeof(FLT32));
	else
	  for (i = 0; i < datasize; i++)
	    ((float *) buffer)[i] = (float) ((double *) data)[i];

	break;
      }
    case DOUBLE_PRECISION:
      {
	if ( dprec == prec )
	  memcpy(buffer, data, datasize*sizeof(FLT64));
	else
	  for (i = 0; i < datasize; i++)
	    ((double *) buffer)[i] = (double) ((float *) data)[i];

	break;
      }
    default:
      {
	Error(func, "unexpected data precision %d", dprec);
      }
    }

  return (0);
}


int extDefDataSP(EXTREC *extp, const float *data)
{
  return (extDefData(extp, SINGLE_PRECISION, (void *) data));
}


int extDefDataDP(EXTREC *extp, const double *data)
{
  return (extDefData(extp, DOUBLE_PRECISION, (void *) data));
}


int extRead(int fileID, EXTREC *extp)
{
  static char func[] = "extRead";
  size_t datasize;
  size_t blocklen, blocklen2;
  size_t i;
  char tempheader[32];
  int hprec, dprec;
  void *buffer;
  int buffersize;
  int byteswap;
  int status;

  if ( ! extp->checked )
    {
      status = extCheckFiletype(fileID, &extp->byteswap);
      if ( status == 0 ) Error(func, "Not a EXTRA file!");
      extp->checked = 1;
    }

  byteswap = extp->byteswap;

  /* read header record */
  blocklen = binReadF77Block(fileID, byteswap);

  if ( fileEOF(fileID) ) return (-1);

  if ( EXT_Debug )
    Message(func, "blocklen = %lu", blocklen);

  hprec = blocklen / EXT_HEADER_LEN;

  extp->hprec = hprec;

  switch ( hprec )
    {
    case SINGLE_PRECISION:
      {
	binReadInt32(fileID, byteswap, EXT_HEADER_LEN, (INT32 *) tempheader);

	for ( i = 0; i < EXT_HEADER_LEN; i++ )
          extp->header[i] = (int) ((INT32 *) tempheader)[i];

	break;
      }
    case DOUBLE_PRECISION:
      {
	binReadInt64(fileID, byteswap, EXT_HEADER_LEN, (INT64 *) tempheader);

	for ( i = 0; i < EXT_HEADER_LEN; i++ )
          extp->header[i] = (int) ((INT64 *) tempheader)[i];

	break;
      }
    default:
      {
	Error(func, "unexpected header precision %d", hprec);
      }
    }

  blocklen2 = binReadF77Block(fileID, byteswap);

  if ( blocklen2 != blocklen )
    {
      Warning(func, "header blocklen differ!");
      return (-1);
    }

  extp->datasize = extp->header[3];

  if ( EXT_Debug )
    Message(func, "datasize = %lu", extp->datasize);

  blocklen = binReadF77Block(fileID, byteswap);

  buffersize = extp->buffersize;

  if ( buffersize < (int) blocklen )
    {
      buffersize = blocklen;
      buffer = extp->buffer;
      buffer = realloc(buffer, buffersize);
      extp->buffer = buffer;
      extp->buffersize = buffersize;
    }
  else
    buffer = extp->buffer;

  datasize = extp->datasize;

  dprec = blocklen / datasize;

  extp->dprec = dprec;

  if ( dprec != SINGLE_PRECISION && dprec != DOUBLE_PRECISION )
    {
      Warning(func, "unexpected data precision %d", dprec);
      return (-1);
    }

  fileRead(fileID, buffer, blocklen);

  blocklen2 = binReadF77Block(fileID, byteswap);

  if ( blocklen2 != blocklen )
    {
      Warning(func, "data blocklen differ!");
      return (-1);
    }

  return (0);
}


int extWrite(int fileID, EXTREC *extp)
{
  static char func[] = "extWrite";
  size_t datasize;
  size_t blocklen;
  size_t i;
  int dprec, hprec;
  char tempheader[32];
  int *header;
  void *buffer;
  int byteswap = extp->byteswap;


  dprec  = extp->dprec;
  hprec  = extp->hprec;
  header = extp->header;

  /* write header record */
  blocklen = EXT_HEADER_LEN * hprec;

  binWriteF77Block(fileID, byteswap, blocklen);

  switch ( hprec )
    {
    case SINGLE_PRECISION:
      {
	for (i = 0; i < EXT_HEADER_LEN; i++)
          ((INT32 *) tempheader)[i] = (INT32) header[i];

	binWriteInt32(fileID, byteswap, EXT_HEADER_LEN, (INT32 *) tempheader);

	break;
      }
    case DOUBLE_PRECISION:
      {
	for (i = 0; i < EXT_HEADER_LEN; i++)
          ((INT64 *) tempheader)[i] = (INT64) header[i];

	binWriteInt64(fileID, byteswap, EXT_HEADER_LEN, (INT64 *) tempheader);

	break;
      }
    default:
      {
	Error(func, "unexpected header precision %d", hprec);
      }
    }
  
  binWriteF77Block(fileID, byteswap, blocklen);

  datasize = header[3];
  blocklen = datasize * dprec;

  binWriteF77Block(fileID, byteswap, blocklen);

  extp->datasize = datasize;

  buffer = extp->buffer;

  switch ( dprec )
    {
    case SINGLE_PRECISION:
      {
	binWriteFlt32(fileID, byteswap, datasize, (FLT32 *) buffer);
	break;
      }
    case DOUBLE_PRECISION:
      {
	binWriteFlt64(fileID, byteswap, datasize, (FLT64 *) buffer);
	break;
      }
    default:
      {
	Error(func, "unexpected data precision %d", dprec);
      }
    }

  binWriteF77Block(fileID, byteswap, blocklen);

  return (0);
}
