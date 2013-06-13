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
#include "cdi_int.h"
#include "vlist.h"
#include "namespace.h"


extern void   gridUnpack ( char *, int, int *, int, MPI_Comm );
extern void  zaxisUnpack ( char *, int, int *, int, MPI_Comm );
extern void  taxisUnpack ( char *, int, int *, int, MPI_Comm );
extern int streamNint;


/*****************************************************************************/

struct streamAssoc
{
  int streamID, vlistID, vlistIDorig;
};

static struct streamAssoc
streamUnpack ( char * unpackBuffer, int unpackBufferSize, 
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
  xdebug ("streamID=%d, intBuffer[0]=%d, nspTarget=%d,"
          " namespaceAdaptKey(a,b)=%d",
          streamID, intBuffer[0], nspTarget,  
          namespaceAdaptKey ( intBuffer[0], nspTarget ));
  xdebug("streamID=%d\n", streamID);
  xassert ( streamID >= 0 &&
            namespaceAdaptKey ( intBuffer[0], nspTarget ) == streamID );
  xdebug ("streamID=%d, vlistID=%d", streamID, namespaceAdaptKey ( intBuffer[4], nspTarget ));

  streamDefByteorder ( streamID, intBuffer[5] );
  streamDefCompType  ( streamID, intBuffer[6] );
  streamDefCompLevel ( streamID, intBuffer[7] ); 
  cdiDefGlobal  ( "REGULARGRID", intBuffer[8] );
  cdiDefGlobal     ( "SORTNAME", intBuffer[9] ); 
  cdiDefGlobal ( "HAVE_MISSVAL", intBuffer[10] );
  struct streamAssoc retval = { streamID, intBuffer[3], intBuffer[4] };
  return retval;
}


/*****************************************************************************/


void rpcUnpackResources ( char * unpackBuffer, int unpackBufferSize, 
			  MPI_Comm comm )
{ 
  int token1, token2, nspTarget;
  int unpackBufferPos = 0;
  int numAssociations = 0, sizeAssociations = 16;
  struct streamAssoc *associations
    = xmalloc(sizeof (associations[0]) * sizeAssociations);

  xdebug("%s", "START");
	  
  while ( unpackBufferPos < unpackBufferSize )
    {
      xmpi ( MPI_Unpack ( unpackBuffer, unpackBufferSize, &unpackBufferPos, 
			  &token1, 1, MPI_INT, comm ));

      if (token1 == END)
        break;
      switch ( token1 )
	{
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
          if (sizeAssociations == numAssociations)
            associations
              = xrealloc(associations,
                         sizeof (associations[0]) * (sizeAssociations *= 2));
	  associations[numAssociations]
            = streamUnpack(unpackBuffer, unpackBufferSize, &unpackBufferPos,
                           nspTarget, comm );
          ++numAssociations;
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
  for (int i = 0; i < numAssociations; ++i)
    {
      cdiStreamSetupVlist(stream_to_pointer(associations[i].streamID),
                          namespaceAdaptKey(associations[i].vlistID,
                                            nspTarget),
                          namespaceAdaptKey(associations[i].vlistIDorig,
                                            nspTarget));
    }
  free(associations);
  xdebug("%s", "RETURN");
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
