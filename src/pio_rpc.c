#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#ifdef USE_MPI
#include "pio_rpc.h"
#include "cdi.h"
#include "pio_comm.h"
#include "pio_util.h"
#include "institution.h"
#include "model.h"
#include "vlist.h"
#include "namespace.h"


extern void   gridUnpack ( char *, int, int *, int, MPI_Comm );
extern void  zaxisUnpack ( char *, int, int *, int, MPI_Comm );
extern void  taxisUnpack ( char *, int, int *, int, MPI_Comm );
extern double cdiDefaultMissval;  
extern int streamNint;


/*****************************************************************************/

static
void streamUnpack ( char * unpackBuffer, int unpackBufferSize, 
		   int * unpackBufferPos, int nspTarget, MPI_Comm comm )
{
  int intBuffer[streamNint], streamID;
  double d;
  char filename[CDI_MAX_NAME];

  xmpi ( MPI_Unpack ( unpackBuffer, unpackBufferSize, unpackBufferPos, 
		      intBuffer, streamNint, MPI_INT, comm ));
  xmpi ( MPI_Unpack ( unpackBuffer, unpackBufferSize, unpackBufferPos, 
		      &d, 1, MPI_DOUBLE, comm ));
  xassert ( xchecksum ( DATATYPE_INT, streamNint, intBuffer ) == d );

  xmpi ( MPI_Unpack ( unpackBuffer, unpackBufferSize, unpackBufferPos, 
		      &cdiDefaultMissval, 1, MPI_DOUBLE, comm ));
  xmpi ( MPI_Unpack ( unpackBuffer, unpackBufferSize, unpackBufferPos, 
		      &filename, intBuffer[2], MPI_CHAR, comm ));
  xmpi ( MPI_Unpack ( unpackBuffer, unpackBufferSize, unpackBufferPos, 
		      &d, 1, MPI_DOUBLE, comm ));
  xassert ( d == xchecksum ( DATATYPE_TXT, intBuffer[2], filename ));

  streamID = streamOpenWrite ( filename, intBuffer[1] );
  xassert ( streamID >= 0 &&
            namespaceAdaptKey ( intBuffer[0], nspTarget ) == streamID );

  streamDefVlist ( streamID, namespaceAdaptKey ( intBuffer[4], nspTarget ));
  xassert ( streamInqVlist ( streamID ) == 
            namespaceAdaptKey ( intBuffer[3], nspTarget ));

  streamDefByteorder ( streamID, intBuffer[5] );
  streamDefCompType  ( streamID, intBuffer[6] );
  streamDefCompLevel ( streamID, intBuffer[7] ); 
  cdiDefGlobal  ( "REGULARGRID", intBuffer[8] );
  cdiDefGlobal     ( "SORTNAME", intBuffer[9] );
  cdiDefGlobal ( "HAVE_MISSVAL", intBuffer[10] );
}


/*****************************************************************************/


void rpcUnpackResources ( char * unpackBuffer, int unpackBufferSize, 
			  MPI_Comm comm )
{ 
  int token1, token2, nspTarget;
  int unpackBufferPos = 0;
  char text[4096];
	  
  while ( unpackBufferPos < unpackBufferSize )
    {
      xmpi ( MPI_Unpack ( unpackBuffer, unpackBufferSize, &unpackBufferPos, 
			  &token1, 1, MPI_INT, comm ));
      
      switch ( token1 )
	{
	case END:
	  return;
	case START:
	  xmpi ( MPI_Unpack ( unpackBuffer, unpackBufferSize, &unpackBufferPos, 
			      &nspTarget, 1, MPI_INT, comm ));
	  break;
	case GRID:
	  gridUnpack      ( unpackBuffer, unpackBufferSize, &unpackBufferPos, 
			    nspTarget, comm );
	  break;
	case ZAXIS: 
	  zaxisUnpack     ( unpackBuffer, unpackBufferSize, &unpackBufferPos, 
			    nspTarget, comm );
	  break;
	case TAXIS:
	  taxisUnpack     ( unpackBuffer, unpackBufferSize, &unpackBufferPos, 
			    nspTarget, comm );
	  break;
	case INSTITUTE:
          instituteUnpack ( unpackBuffer, unpackBufferSize, &unpackBufferPos, 
			    nspTarget, comm);
	  break;
	case MODEL:
          modelUnpack     ( unpackBuffer, unpackBufferSize, &unpackBufferPos, 
			    nspTarget, comm);
	  break;
	case STREAM:
	  streamUnpack    ( unpackBuffer, unpackBufferSize, &unpackBufferPos, 
			    nspTarget, comm );
	  break;
	case VLIST:
          vlistUnpack     (unpackBuffer, unpackBufferSize, &unpackBufferPos, 
			   nspTarget, comm);
	  break;
	default: 
	  xabort ( "TOKEN MAPS NO VALID DATATYPE" );
	}

      xmpi ( MPI_Unpack ( unpackBuffer, unpackBufferSize, &unpackBufferPos, 
			  &token2, 1, MPI_INT, comm ));
      xassert ( token2 == SEPARATOR );
    }
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
