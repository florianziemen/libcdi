#include "pio_rpc.h"
#include "pio_util.h"

MPI_Comm commGlob;
int sizeGlob;
int rankGlob;
int nProcsIO;
int nProcsCalc;
MPI_Comm * commsIO;
MPI_Comm * commCalc; 
 
 
void  commsIOCreate ( MPI_Comm commGlob, int nIOP )
{   
  char fnName[] = "commsIOcreate()";
  int * ranks;
  int i, j;
  MPI_Group groupGlob, currGroupIO;
  int currIORank;
  char name[10];

  nProcsIO = nIOP;
  MPI_Comm_rank ( commGlob, &rankGlob );

  commsIO = ( MPI_Comm * ) myMalloc ( nProcsIO * sizeof ( MPI_Comm ),
				      __FILE__, __LINE__ );
 
  strncpy ( name, "commsIO_", 8 ); 
  name[9] = '\0';
  
  ranks = ( int * ) myMalloc (( nProcsCalc + 1 ) * sizeof ( int ), 
			      __FILE__, __LINE__ );

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
	for ( i = 0; i < nProcsIO; i++ )
	  myDebugComm ( __FILE__, fnName, __LINE__, commsIO + i );
      else
	myDebugComm ( __FILE__, fnName,__LINE__, commCalc );
    }

  MPI_Group_free ( &currGroupIO );
  MPI_Group_free ( &groupGlob );
  free ( ranks );

  return;
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
  char fnName[] = "commsIOCleanup()";
  int i;

  if ( ddebug >= 1 ) myDebug (  __FILE__, fnName,__LINE__ );
  
  for ( i = 0; i < nProcsIO; i++ )
    if ( rankGlob < nProcsCalc || rankGlob == nProcsCalc + i )
      MPI_Comm_free ( commsIO + i );

  return;
}
