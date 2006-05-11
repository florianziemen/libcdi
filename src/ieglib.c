#if defined (HAVE_CONFIG_H)
#  include "config.h"
#endif

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>

#include "dmemory.h"

#include "ieg.h"
#include "error.h"
#include "file.h"
#include "binary.h"
#include "swap.h"


#define SINGLE_PRECISION  4
#define DOUBLE_PRECISION  8


static int initIegLib = 0;
static int iegDefaultDprec = 0;


/*
 * A version string.
 */

#undef  LIBVERSION
#define LIBVERSION      1.0.0
#define XSTRING(x)	#x
#define STRING(x)	XSTRING(x)
static const char ieg_libvers[] = STRING(LIBVERSION) " of "__DATE__" "__TIME__;

const char *iegLibraryVersion(void)
{
  return (ieg_libvers);
}


int IEG_Debug = 0;    /* If set to 1, debugging */



void iegLibInit()
{
  static char func[] = "iegLibInit";
  char *envString;
  char *envName = "IEG_PRECISION";


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
	    case 'r':
	      {
		switch ( (int) envString[pos+1] )
		  {
		  case '4': iegDefaultDprec = SINGLE_PRECISION; break;
		  case '8': iegDefaultDprec = DOUBLE_PRECISION; break;
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

  initIegLib = 1;
}


void iegDebug(int debug)
{
  static char func[] = "iegDebug";

  IEG_Debug = debug;

  if ( IEG_Debug )
    Message(func, "debug level %d", debug);
}


void iegInit(IEGREC *iegp)
{
  iegp->checked    = 0;
  iegp->byteswap   = 0;
  iegp->dprec      = 0;
  iegp->refval     = 0;
  iegp->datasize   = 0;
  iegp->buffersize = 0;
  iegp->buffer     = NULL;

  memset(iegp->ipdb, 0, sizeof(iegp->ipdb));
  memset(iegp->igdb, 0, sizeof(iegp->igdb));
  memset(iegp->vct,  0, sizeof(iegp->vct));
}


IEGREC *iegNew(void)
{
  static char func[] = "iegNew";
  IEGREC *iegp;

  if ( ! initIegLib ) iegLibInit();

  iegp = (IEGREC *) malloc(sizeof(IEGREC));

  iegInit(iegp);

  return (iegp);
}


void iegDelete(IEGREC *iegp)
{
  static char func[] = "iegDelete";

  if ( iegp )
    {
      if ( iegp->buffer ) free(iegp->buffer);
      free(iegp);
    }
}


int iegCheckFiletype(int fileID, int *swap)
{
  static char func[] = "iegCheckFiletype";
  size_t blocklen = 0;
  size_t sblocklen = 0;
  size_t data = 0;
  size_t dimx = 0, dimy = 0;
  int fact = 0, found = 0;
  unsigned char buffer[1048], *pbuf;

  if ( fileRead(fileID, buffer, 4) != 4 ) return (found);

  blocklen  = get_UINT32(buffer);
  sblocklen = get_SUINT32(buffer);

  if ( IEG_Debug )
    Message(func, "blocklen = %d sblocklen = %d", blocklen, sblocklen);

  if ( blocklen == 636 || blocklen == 640 )
    {
     *swap = 0;
      fact = 4;
      if ( fileRead(fileID, buffer, blocklen+8) != blocklen+8 ) return (found);
      pbuf = buffer+(37+4)*4;    dimx = (size_t) get_UINT32(pbuf);
      pbuf = buffer+(37+5)*4;    dimy = (size_t) get_UINT32(pbuf);
      pbuf = buffer+blocklen+4;  data = (size_t) get_UINT32(pbuf);
    }
  else if ( blocklen == 1040 || blocklen == 1036 )
    {
     *swap = 0;
      fact = 8;
      if ( fileRead(fileID, buffer, blocklen+8) != blocklen+8 ) return (found);
      pbuf = buffer+(37+4)*4;    dimx = (size_t) get_UINT32(pbuf);
      pbuf = buffer+(37+5)*4;    dimy = (size_t) get_UINT32(pbuf);
      pbuf = buffer+blocklen+4;  data = (size_t) get_UINT32(pbuf);
    }
  else if ( sblocklen == 636 || sblocklen == 640 )
    {
     *swap = 1;
      fact = 4;
      if ( fileRead(fileID, buffer, sblocklen+8) != sblocklen+8 ) return (found);
      pbuf = buffer+(37+4)*4;     dimx = (size_t) get_SUINT32(pbuf);
      pbuf = buffer+(37+5)*4;     dimy = (size_t) get_SUINT32(pbuf);
      pbuf = buffer+sblocklen+4;  data = (size_t) get_SUINT32(pbuf);
    }
  else if ( sblocklen == 1040 || sblocklen == 1036 )
    {
     *swap = 1;
      fact = 8;
      if ( fileRead(fileID, buffer, sblocklen+8) != sblocklen+8 ) return (found);
      pbuf = buffer+(37+4)*4;     dimx = (size_t) get_SUINT32(pbuf);
      pbuf = buffer+(37+5)*4;     dimy = (size_t) get_SUINT32(pbuf);
      pbuf = buffer+sblocklen+4;  data = (size_t) get_SUINT32(pbuf);
    }

  fileRewind(fileID);

  if      ( data && dimx*dimy*fact == data ) found = 1;
  else if ( data && dimx*dimy*8    == data ) found = 1;

  if ( IEG_Debug )
    {
      Message(func, "swap = %d fact = %d", *swap, fact);
      Message(func, "dimx = %lu dimy = %lu data = %lu", dimx, dimy, data);
    }

  return (found);
}


void iegCopyMeta(IEGREC *diegp, IEGREC *siegp)
{
  /*  diegp->byteswap = siegp->byteswap; */
  diegp->dprec    = siegp->dprec;
  diegp->refval   = siegp->refval;

  memcpy(diegp->ipdb, siegp->ipdb, sizeof(siegp->ipdb));
  memcpy(diegp->igdb, siegp->igdb, sizeof(siegp->igdb));
  memcpy(diegp->vct,  siegp->vct,  sizeof(siegp->vct));
}


int iegInqData(IEGREC *iegp, int prec, void *data)
{
  static char func[] = "iegInqData";
  size_t datasize;
  size_t i;
  int ierr = 0;
  int dprec;
  void *buffer;
  int byteswap = iegp->byteswap;


  datasize = iegp->datasize;

  buffer = iegp->buffer;

  dprec = iegp->dprec;

  switch ( dprec )
    {
    case SINGLE_PRECISION:
      {
	if ( sizeof(REAL4) == 4 )
	  {
	    if ( byteswap ) swap4byte(buffer, datasize);

	    if ( dprec == prec )
	      memcpy(data, buffer, datasize*sizeof(REAL4));
	    else
	      for (i = 0; i < datasize; i++)
		((double *) data)[i] = (double) ((float *) buffer)[i];
	  }
	else
	  {
	    Error(func, "not implemented for %d byte float", sizeof(REAL4));
	  }	
	break;
      }
    case DOUBLE_PRECISION:
	if ( sizeof(REAL8) == 8 )
	  {
	    if ( byteswap ) swap8byte(buffer, datasize);

	    if ( dprec == prec )
	      memcpy(data, buffer, datasize*sizeof(REAL8));
	    else
	      for (i = 0; i < datasize; i++)
		((float *) data)[i] = (float) ((double *) buffer)[i];
	  }
	else
	  {
	    Error(func, "not implemented for %d byte float", sizeof(REAL8));
	  }	
	break;
    default:
      {
	Error(func, "unexpected data precision %d", dprec);
      }
    }

  return (ierr);
}


int iegInqDataSP(IEGREC *iegp, float *data)
{
  return (iegInqData(iegp, SINGLE_PRECISION, (void *) data));
}


int iegInqDataDP(IEGREC *iegp, double *data)
{
  return (iegInqData(iegp, DOUBLE_PRECISION, (void *) data));
}


int iegDefData(IEGREC *iegp, int prec, const void *data)
{
  static char func[] = "iegDefData";
  size_t datasize;
  size_t blocklen;
  size_t buffersize;
  size_t i;
  int dprec;
  void *buffer;


  if ( iegDefaultDprec ) dprec = iegDefaultDprec;
  else                   dprec = iegp->dprec;

  if ( ! dprec ) dprec = prec;

  iegp->dprec = dprec;

  datasize = IEG_G_NumLon(iegp->igdb)*IEG_G_NumLat(iegp->igdb);
  blocklen = datasize * dprec;

  iegp->datasize = datasize;

  buffersize = iegp->buffersize;

  if ( buffersize != blocklen )
    {
      buffersize = blocklen;
      buffer = iegp->buffer;
      buffer = realloc(buffer, buffersize);
      iegp->buffer = buffer;
      iegp->buffersize = buffersize;
    }
  else
    buffer = iegp->buffer;

  switch ( dprec )
    {
    case SINGLE_PRECISION:
      {
	if ( dprec == prec )
	  memcpy(buffer, data, datasize*sizeof(REAL4));
	else
	  for (i = 0; i < datasize; i++)
	    ((float *) buffer)[i] = (float) ((double *) data)[i];

	break;
      }
    case DOUBLE_PRECISION:
      {
	if ( dprec == prec )
	  memcpy(buffer, data, datasize*sizeof(REAL8));
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


int iegDefDataSP(IEGREC *iegp, const float *data)
{
  return (iegDefData(iegp, SINGLE_PRECISION, (void *) data));
}


int iegDefDataDP(IEGREC *iegp, const double *data)
{
  return (iegDefData(iegp, DOUBLE_PRECISION, (void *) data));
}


int iegRead(int fileID, IEGREC *iegp)
{
  static char func[] = "iegRead";
  size_t datasize;
  size_t blocklen, blocklen2;
  size_t i;
  char tmpbuf[800];
  int dprec = 0;
  void *buffer;
  int buffersize;
  int byteswap;
  int status;

  if ( ! iegp->checked )
    {
      status = iegCheckFiletype(fileID, &iegp->byteswap);
      if ( status == 0 ) Error(func, "Not a IEG file!");
      iegp->checked = 1;
    }

  byteswap = iegp->byteswap;

  /* read header record */
  blocklen = binReadF77Block(fileID, byteswap);

  if ( fileEOF(fileID) ) return (-1);

  if ( IEG_Debug )
    Message(func, "blocklen = %lu", blocklen);

  if ( blocklen == 636 || blocklen == 640 )
    dprec = 4;
  else if ( blocklen == 1040 || blocklen == 1036 )
    dprec = 8;
  else
    {
      Warning(func, "unexpecteted header size %d!", (int) blocklen);
      return (-1);
    }

  iegp->dprec = dprec;

  binReadInt32(fileID, byteswap, 37, (INT32 *) tmpbuf);
  for ( i = 0; i < 37; i++ ) iegp->ipdb[i] = (int) ((INT32 *) tmpbuf)[i];

  binReadInt32(fileID, byteswap, 18, (INT32 *) tmpbuf);
  for ( i = 0; i < 18; i++ ) iegp->igdb[i] = (int) ((INT32 *) tmpbuf)[i];

  if ( blocklen == 636 || blocklen == 1036 )
    {
      fileRead(fileID, tmpbuf, 4);
      if ( byteswap ) swap4byte(tmpbuf, 1);
      iegp->refval = (double) ((float *) tmpbuf)[0];
    }
  else
    {
      fileRead(fileID, tmpbuf, 8);
      if ( byteswap ) swap8byte(tmpbuf, 1);
      iegp->refval = (double) ((double *) tmpbuf)[0];
    }

  binReadInt32(fileID, byteswap, 3, (INT32 *) tmpbuf);
  for ( i = 0; i < 3; i++ ) iegp->igdb[18+i] = (int) ((INT32 *) tmpbuf)[i];

  if ( dprec == SINGLE_PRECISION )
    {
      fileRead(fileID, tmpbuf, 400);
      if ( byteswap ) swap4byte(tmpbuf, 100);
      for ( i = 0; i < 100; i++ )
	iegp->vct[i] = (double) ((float *) tmpbuf)[i];
    }
  else
    {
      fileRead(fileID, tmpbuf, 800);
      if ( byteswap ) swap8byte(tmpbuf, 100);
      for ( i = 0; i < 100; i++ )
	iegp->vct[i] = (double) ((double *) tmpbuf)[i];
    }
  
  /*
  fprintf(stderr, "refval %g\n", iegp->refval);

  for ( i = 0; i < 100; i++ )
    fprintf(stderr, "%3d %g\n", i, iegp->vct[i]);
  
  {
    int i;
    for ( i = 0; i < 37; i++ )
      fprintf(stderr, "pdb: %d %d\n", i, iegp->ipdb[i]);
    for ( i = 0; i < 22; i++ )
      fprintf(stderr, "gdb: %d %d\n", i, iegp->igdb[i]);
  }
  */
  blocklen2 = binReadF77Block(fileID, byteswap);

  if ( blocklen2 != blocklen )
    {
      Warning(func, "header blocklen differ!");
      return (-1);
    }

  iegp->datasize = IEG_G_NumLon(iegp->igdb)*IEG_G_NumLat(iegp->igdb);

  if ( IEG_Debug )
    Message(func, "datasize = %lu", iegp->datasize);

  blocklen = binReadF77Block(fileID, byteswap);

  buffersize = iegp->buffersize;

  if ( buffersize < (int) blocklen )
    {
      buffersize = blocklen;
      buffer = iegp->buffer;
      buffer = realloc(buffer, buffersize);
      iegp->buffer = buffer;
      iegp->buffersize = buffersize;
    }
  else
    buffer = iegp->buffer;

  datasize = iegp->datasize;

  if ( dprec != (int) (blocklen/datasize) )
    {
      Warning(func, "data precision differ! (h = %d; d = %d)",
	      (int) dprec, (int) (blocklen/datasize));
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


int iegWrite(int fileID, IEGREC *iegp)
{
  static char func[] = "iegWrite";
  size_t datasize;
  size_t blocklen;
  size_t i;
  int dprec;
  float refval;
  char tmpbuf[800];
  float fvct[100];
  void *buffer;
  int byteswap = iegp->byteswap;


  dprec  = iegp->dprec;

  /* write header record */
  if ( dprec == SINGLE_PRECISION )
    blocklen = 636;
  else
    blocklen = 1036;

  binWriteF77Block(fileID, byteswap, blocklen);

  for ( i = 0; i < 37; i++ ) ((INT32 *) tmpbuf)[i] = (INT32) iegp->ipdb[i];
  binWriteInt32(fileID, byteswap, 37, (INT32 *) tmpbuf);

  for ( i = 0; i < 18; i++ ) ((INT32 *) tmpbuf)[i] = (INT32) iegp->igdb[i];
  binWriteInt32(fileID, byteswap, 18, (INT32 *) tmpbuf);

  refval = iegp->refval;
  binWriteReal4(fileID, byteswap, 1, (REAL4 *) &refval);

  for ( i = 0; i < 3; i++ ) ((INT32 *) tmpbuf)[i] = (INT32) iegp->igdb[18+i];
  binWriteInt32(fileID, byteswap, 3, (INT32 *) tmpbuf);

  if ( dprec == SINGLE_PRECISION )
    {
      for ( i = 0; i < 100; i++ ) fvct[i] = (float) iegp->vct[i];
      binWriteReal4(fileID, byteswap, 100, fvct);
    }
  else
    {
      binWriteReal8(fileID, byteswap, 100, iegp->vct);
    }
  
  binWriteF77Block(fileID, byteswap, blocklen);

  datasize = iegp->igdb[4]*iegp->igdb[5];
  blocklen = datasize * dprec;

  binWriteF77Block(fileID, byteswap, blocklen);

  iegp->datasize = datasize;

  buffer = iegp->buffer;

  switch ( dprec )
    {
    case SINGLE_PRECISION:
      {
	binWriteReal4(fileID, byteswap, datasize, (REAL4 *) buffer);
	break;
      }
    case DOUBLE_PRECISION:
      {
	binWriteReal8(fileID, byteswap, datasize, (REAL8 *) buffer);
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
