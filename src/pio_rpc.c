#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#ifdef USE_MPI
#include "pio_rpc.h"
#include "pio_util.h"
#include "cdi.h"
#include "institution.h"
#include "model.h"
#include "vlist.h"

MPI_Comm commGlob;
int sizeGlob;
int rankGlob;
int nProcsIO;
int nProcsCalc;
MPI_Comm * commsIO;
MPI_Comm commCalc;
 
 
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

  if ( ddebug >= 1 ) xdebug ();
  
  for ( i = 0; i < nProcsIO; i++ )
    if ( rankGlob < nProcsCalc || rankGlob == nProcsCalc + i )
      MPI_Comm_free ( commsIO + i );;
}


/*****************************************************************************/


void unpackGrid ( char * unpackBuffer, int unpackBufferSize, 
		  int * unpackBufferPos, MPI_Comm comm )
{ 
  int gridID;
  int type, size, xsize, ysize;
  double * xvals, * yvals;

  xmpi ( MPI_Unpack ( unpackBuffer, unpackBufferSize, unpackBufferPos, 
		      &type, 1, MPI_INT, comm ));
  xmpi ( MPI_Unpack ( unpackBuffer, unpackBufferSize, unpackBufferPos, 
		      &size, 1, MPI_INT, comm ));
  gridID = gridCreate ( type, size );

  xmpi ( MPI_Unpack ( unpackBuffer, unpackBufferSize, unpackBufferPos, 
		      &xsize, 1, MPI_INT, comm ));
  gridDefXsize(gridID, xsize);
 
  xmpi ( MPI_Unpack ( unpackBuffer, unpackBufferSize, unpackBufferPos, 
		      &ysize, 1, MPI_INT, comm ));
  gridDefYsize(gridID, ysize);

  size = type == GRID_UNSTRUCTURED || type == GRID_CURVILINEAR ? 
    size : xsize;
  xvals = xmalloc ( size * sizeof ( double ));
  xmpi ( MPI_Unpack ( unpackBuffer, unpackBufferSize, unpackBufferPos, 
		      xvals, size, MPI_DOUBLE, comm ));
  gridDefXvals ( gridID, xvals );
  free ( xvals );

  size = type == GRID_UNSTRUCTURED || type == GRID_CURVILINEAR ? 
    size : ysize;
  yvals = xmalloc ( size * sizeof ( double ));
  xmpi ( MPI_Unpack ( unpackBuffer, unpackBufferSize, unpackBufferPos, 
		      yvals, size, MPI_DOUBLE, comm ));
  gridDefYvals ( gridID, yvals );
  free ( yvals );
}


/*****************************************************************************/


void unpackZaxis ( char * unpackBuffer, int unpackBufferSize, 
		   int * unpackBufferPos, MPI_Comm comm )
{ 
  int zaxisID;
  int type, size;
  double * vals;

  xmpi ( MPI_Unpack ( unpackBuffer, unpackBufferSize, unpackBufferPos, 
		      &type, 1, MPI_INT, comm ));
  xmpi ( MPI_Unpack ( unpackBuffer, unpackBufferSize, unpackBufferPos, 
		      &size, 1, MPI_INT, comm ));
  zaxisID = zaxisCreate ( type, size );

  vals = xmalloc ( size * sizeof ( double ));
  xmpi ( MPI_Unpack ( unpackBuffer, unpackBufferSize, unpackBufferPos, 
		      vals, size, MPI_DOUBLE, comm ));
  zaxisDefLevels ( zaxisID, vals );
  free ( vals );
}


/*****************************************************************************/


void unpackTaxis ( char * unpackBuffer, int unpackBufferSize, 
		   int * unpackBufferPos, MPI_Comm comm )
{ 
  int taxisID;
  int type;

  xmpi ( MPI_Unpack ( unpackBuffer, unpackBufferSize, unpackBufferPos, 
		      &type, 1, MPI_INT, comm ));
  taxisID = taxisCreate ( type );
}

/*****************************************************************************/


void unpackStream ( char * unpackBuffer, int unpackBufferSize, 
		    int * unpackBufferPos, MPI_Comm comm )
{ 
  int streamID;
  int filenameLen, filetype, vlistID;
  char * filename;

  xmpi ( MPI_Unpack ( unpackBuffer, unpackBufferSize, unpackBufferPos, 
		      &filenameLen, 1, MPI_INT, comm ));
  filename = xmalloc ( filenameLen * sizeof ( char )); 
  xmpi ( MPI_Unpack ( unpackBuffer, unpackBufferSize, unpackBufferPos, 
		      filename, filenameLen, MPI_CHAR, comm ));
  xmpi ( MPI_Unpack ( unpackBuffer, unpackBufferSize, unpackBufferPos, 
		      &filetype, 1, MPI_INT, comm )); 
  streamID = streamOpenWrite ( filename, filetype );
  if ( streamID < 0 ) xabort ( "can not open output file" );
  free ( filename );
  
  xmpi ( MPI_Unpack ( unpackBuffer, unpackBufferSize, unpackBufferPos, 
		      &vlistID, 1, MPI_INT, comm )); 
  streamDefVlist ( streamID, vlistID );
}


/*****************************************************************************/


void rpcUnpackResources ( char * unpackBuffer, int unpackBufferSize, 
			  MPI_Comm comm )
{ 
  int token1, token2;
  int unpackBufferPos = 0;
  char text[4096];

  while ( unpackBufferPos < unpackBufferSize )
    {
      xmpi ( MPI_Unpack ( unpackBuffer, unpackBufferSize, &unpackBufferPos, 
			  &token1, 1, MPI_INT, comm ));
      
      switch ( token1 )
	{
	case END:
	  xdebug ( "#### end of packed data" );
	  if ( rankGlob == sizeGlob - 1 )
	    reshArrayPrint ( "reshArrayIOServer" );
	  return;
	case GRID:
	  unpackGrid ( unpackBuffer, unpackBufferSize, &unpackBufferPos, comm );
	  break;
	case ZAXIS:
	  unpackZaxis ( unpackBuffer, unpackBufferSize, &unpackBufferPos, comm );
	  break;
	case TAXIS: 
	  unpackTaxis ( unpackBuffer, unpackBufferSize, &unpackBufferPos, comm );
	  break;
	case INSTITUTE:
          instituteUnpack ( unpackBuffer, unpackBufferSize, &unpackBufferPos, comm);
	  break;
	case MODEL:
          modelUnpack ( unpackBuffer, unpackBufferSize, &unpackBufferPos, comm);
	  break;
	case STREAM:
	  unpackStream ( unpackBuffer, unpackBufferSize, &unpackBufferPos, comm );
	  break;
	case VLIST:
          vlistUnpack(unpackBuffer, unpackBufferSize, &unpackBufferPos, comm);
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
