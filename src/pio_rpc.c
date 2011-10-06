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

int nProcsIO;
int nProcsCalc;
MPI_Comm * commsIO;
MPI_Comm commCalc;
 
 
void  commsIOCreate ()
{   
  int * ranks;
  int i;
  MPI_Group groupGlob, currGroupIO;
  int currIORank;
  char name[10];
  int rankGlob = commInqRankGlob ();

  MPI_Comm commGlob = commInqCommGlob ();
  nProcsIO = commInqNProcsIO ();
  nProcsCalc = commInqNProcsModel ();

  commsIO = ( MPI_Comm * ) xmalloc ( nProcsIO * sizeof ( MPI_Comm ));
  strncpy ( name, "commsIO_", 8 ); 
  name[9] = '\0';

  xdebug ("######### nProcsCalc=%d", nProcsCalc );
  
  ranks = ( int * ) xmalloc (( nProcsCalc + 1 ) * sizeof ( int ));
  for ( i = 0; i < nProcsCalc; i++ )
    * ( ranks + i ) = i;
  
  MPI_Comm_group ( commInqCommGlob (), &groupGlob ); 

  for ( i = 0; i < nProcsIO; i++ )
    {
      currIORank = nProcsCalc + i;
      
      * ( ranks + nProcsCalc ) = currIORank;	  
	  
      MPI_Group_incl ( groupGlob, nProcsCalc + 1, ranks, &currGroupIO );
      MPI_Comm_create ( commInqCommGlob (), currGroupIO,  commsIO + i );
      if ( rankGlob < nProcsCalc || rankGlob == nProcsCalc + i )
	{
	  sprintf ( &name[8], "%d", i ); 
	  MPI_Comm_set_name ( * ( commsIO + i ), name );
	}
      if ( rankGlob == currIORank )
	commCalc = commsIO[i];
    }

  if ( ddebug >= 2 )
    {

      if ( rankGlob < nProcsCalc )
	for ( i = 0; i < nProcsIO; i++ ) xdebugComm ( commsIO + i );
      else
	xdebugComm ( &commCalc );
    }

  MPI_Group_free ( &currGroupIO );
  MPI_Group_free ( &groupGlob );
  free ( ranks );
}  


/*****************************************************************************/

/**
  @brief frees the MPI_Communicators for the 
  communication between the calculator PEs and the I/O PEs and within the 
  group of I/O PEs.

  @param

  @return
*/

void commsIOCleanup ()
{
  int i;
  int rankGlob = commInqRankGlob ();
  
  for ( i = 0; i < nProcsIO; i++ )
    if ( rankGlob < nProcsCalc || rankGlob == nProcsCalc + i )
        MPI_Comm_free ( commsIO + i );
}


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
  if ( xchecksum ( DATATYPE_INT, streamNint, intBuffer ) != d ) xabort ("");

  xmpi ( MPI_Unpack ( unpackBuffer, unpackBufferSize, unpackBufferPos, 
		      &cdiDefaultMissval, 1, MPI_DOUBLE, comm ));
  xmpi ( MPI_Unpack ( unpackBuffer, unpackBufferSize, unpackBufferPos, 
		      &filename, intBuffer[2], MPI_CHAR, comm ));
  xmpi ( MPI_Unpack ( unpackBuffer, unpackBufferSize, unpackBufferPos, 
		      &d, 1, MPI_DOUBLE, comm ));
  if ( d != xchecksum ( DATATYPE_TXT, intBuffer[2], filename )) xabort ("");

  streamID = streamOpenWrite ( filename, intBuffer[1] );
  if ( streamID < 0 ) xabort ( "can not open output file" );
  if ( namespaceAdaptKey ( intBuffer[0], nspTarget ) != streamID )
    xabort ("");

  streamDefVlist ( streamID, namespaceAdaptKey ( intBuffer[4], nspTarget ));
  if ( streamInqVlist ( streamID ) != namespaceAdaptKey ( intBuffer[3], nspTarget ))
    xabort ("");

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
	  xabort ( "no valid datatype" );
	}

      xmpi ( MPI_Unpack ( unpackBuffer, unpackBufferSize, &unpackBufferPos, 
			  &token2, 1, MPI_INT, comm ));

      if ( token2 != SEPARATOR )
	{
	  sprintf ( text, "problems unpacking type %d", token1); 
	  xabort ( text);
	}
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
