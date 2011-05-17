#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#ifdef USE_MPI
#include "pio_rpc.h"
#include "pio_util.h"
#include "cdi.h"

MPI_Comm commGlob;
int sizeGlob;
int rankGlob;
int nProcsIO;
int nProcsCalc;
MPI_Comm * commsIO;
MPI_Comm * commCalc;
 
 
void  commsIOCreate ( MPI_Comm commGlob, int nIOP )
{   
  int * ranks;
  int i;
  MPI_Group groupGlob, currGroupIO;
  int currIORank;
  char name[10];

  nProcsIO = nIOP;
  MPI_Comm_rank ( commGlob, &rankGlob );

  commsIO = ( MPI_Comm * ) xmalloc ( nProcsIO * sizeof ( MPI_Comm ));
  strncpy ( name, "commsIO_", 8 ); 
  name[9] = '\0';
  
  ranks = ( int * ) xmalloc (( nProcsCalc + 1 ) * sizeof ( int ));
  for ( i = 0; i < nProcsCalc; i++ )
    * ( ranks + i ) = i;
  
  MPI_Comm_group ( commGlob, &groupGlob ); 

  for ( i = 0; i < nProcsIO; i++ )
    {
      currIORank = nProcsCalc + i;
      
      * ( ranks + nProcsCalc ) = currIORank;	  
	  
      MPI_Group_incl ( groupGlob, nProcsCalc + 1, ranks, &currGroupIO );
      MPI_Comm_create ( commGlob, currGroupIO,  commsIO + i );
      if ( rankGlob < nProcsCalc || rankGlob == nProcsCalc + i )
	{
	  sprintf ( &name[8], "%d", i ); 
	  MPI_Comm_set_name ( * ( commsIO + i ), name );
	}
      if ( rankGlob == currIORank )
	commCalc = commsIO + i;
    }

  if ( ddebug >= 2 )
    {

      if ( rankGlob < nProcsCalc )
	for ( i = 0; i < nProcsIO; i++ ) xdebugComm ( commsIO + i );
      else
	xdebugComm ( commCalc );
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

  if ( ddebug >= 1 ) xdebug ();
  
  for ( i = 0; i < nProcsIO; i++ )
    if ( rankGlob < nProcsCalc || rankGlob == nProcsCalc + i )
      MPI_Comm_free ( commsIO + i );;
}


/*****************************************************************************/


void unpackGrid ( char * buffer, size_t bufferSize, int * pos, MPI_Comm * comm )
{ 
  int gridID;
  int type;
  int int1, int2, size;
  double * doublebuffer;

  xdebug ();

  MPI_Unpack ( buffer, bufferSize, pos, &type, 1, MPI_INT, * comm );
  MPI_Unpack ( buffer, bufferSize, pos, &int2, 1, MPI_INT, * comm );
  gridID = gridCreate ( int1, int2 );

  MPI_Unpack ( buffer, bufferSize, pos, &int1, 1, MPI_INT, * comm );
  gridDefXsize(gridID, int1);
  MPI_Unpack ( buffer, bufferSize, pos, &int1, 1, MPI_INT, * comm );
  gridDefYsize(gridID, int1);

  if ( type == GRID_UNSTRUCTURED || type == GRID_CURVILINEAR )
    size = gridInqSize ( gridID );
  else
    size = gridInqXsize ( gridID );


  doublebuffer = xmalloc ( size * sizeof ( double ));


  MPI_Unpack ( buffer, bufferSize, pos, doublebuffer, size, MPI_DOUBLE, * comm );

  gridDefXvals ( gridID, doublebuffer );


  if ( type == GRID_UNSTRUCTURED || type == GRID_CURVILINEAR )
    size = gridInqSize ( gridID );
  else
    size = gridInqYsize ( gridID );


  doublebuffer = xmalloc ( size * sizeof ( double ));


  MPI_Unpack ( buffer, bufferSize, pos, doublebuffer, size, MPI_DOUBLE, * comm );

  gridDefYvals ( gridID, doublebuffer );
}


/*****************************************************************************/


void rpcUnpackResources ( char * buffer, size_t size, MPI_Comm * comm )
{ 
  int token1, token2;
  int pos = 0;
  char text[4096];

  while ( pos < size )
    {
      MPI_Unpack ( buffer, size, &pos, &token1, 1, MPI_INT, * comm );
      
      switch ( token1 )
	{
	case END:
	  xdebug ( "#### end of packed data" );
	  reshArrayPrint ();
	  return;
	case GRID:
	  unpackGrid ( buffer, size, &pos, comm );
	  break;
	case ZAXIS:
	  break;
	case TAXIS: 
	  break;
	case INSTITUTE:
	  break;
	case MODEL:
	  break;
	case STREAM:
	  break;
	case VLIST:
	  break;
	default: 
	  xabort ( "no valid datatype" );
	}

      MPI_Unpack ( buffer, size, &pos, &token2, 1, MPI_INT, * comm );

      if ( token2 != SEPARATOR )
	{
	  sprintf ( text, "problems unpacking type %d", token1); 
	  xabort ( text);
	}
      else
	{
	  xdebug ( "#### success unpacking type %d", token1 );
	}
    }
}

#endif
