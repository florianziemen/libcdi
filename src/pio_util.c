#include "pio_util.h"


char commands[][12] = { "FINALIZE\0",
		       "METADATA\0"};


/****************************************************/

void pcdiAbort ( char *errorString, const char *filename, int line, int rank )
{
  fprintf ( stderr, "testERROR, pe%d, %s, line %d, errorString: \"%s\"\n",
	    rank, filename, line, errorString );
  MPI_Abort ( MPI_COMM_WORLD, 1 );
}

/*****************************************************************************/  

void * pcdiXmalloc ( size_t size, const char *filename, int line )
{ 
  void * value = malloc ( size );

  if ( value == NULL )
    pcdiAbort ( "malloc failed", filename, line, -1 );
  
  return value;
}

/***************************************************************/ 

void pcdiXMPI ( int iret, const char *filename, int line )
{
  char errorString1[MPI_MAX_ERROR_STRING + 1];
  char errorString2[MPI_MAX_ERROR_STRING + 1];
  int len, errorClass, rank;

  MPI_Comm_rank ( MPI_COMM_WORLD, &rank );

  if ( iret != MPI_SUCCESS )
    {
      MPI_Error_class ( iret, &errorClass );
      MPI_Error_string ( errorClass, errorString1, &len );
      errorString1[len] = '\0';
      MPI_Error_string ( iret, errorString2, &len);
      errorString2[len] = '\0';
      
      fprintf ( stderr, "MPI ERROR, pe%d, %s, line %d,"
		"errorClass: \"%s\""
		"errorString: \"%s\"\n",
		rank, filename, line, 
		errorString1, errorString2);

      MPI_Abort ( MPI_COMM_WORLD, iret );
    }

  return;
}

/*****************************************************************************/

void myDebug (  const char *filename, const char *functionname, int line )
{
  int rank;

  MPI_Comm_rank ( MPI_COMM_WORLD, &rank );

  fprintf ( stdout, 
	    "pe%d in %s, %s, line %d\n", 
	    rank, functionname, filename, line );
  return;
}

/****************************************************/ 

void myDebug2 (  const char *filename, const char *functionname, int line, char * text )
{
  int rank;

  MPI_Comm_rank ( MPI_COMM_WORLD, &rank );

  fprintf ( stdout, 
	    "pe%d in %s, %s, line %d: %s\n", 
	    rank, functionname, filename, line, text );
  return;
}

/****************************************************/ 

void myDebugComm ( const char *filename, const char *functionname, int line, MPI_Comm *comm )
{
  int rank, size, len, rankGlob;
  char *name;

  name = ( char * ) xmalloc ( MPI_MAX_OBJECT_NAME );
  memset ( name, 0, ( MPI_MAX_OBJECT_NAME ) * sizeof ( char ));
  MPI_Comm_get_name ( * comm, name, &len ); 
  MPI_Comm_size ( * comm, &size );
  MPI_Comm_rank ( * comm, &rank ); 
  MPI_Comm_rank ( MPI_COMM_WORLD, &rankGlob );   
  fprintf ( stdout, 
	    "pe%d in %s, %s, line %d: comm: name=%s, size=%d, rank=%d\n", 
	    rankGlob, functionname, filename, line,
	    name, size, rank );
  free ( name );

  return;
  
}

/****************************************************/

void myDebugMsg ( const char *filename, const char *functionname, int line,
		  int tag, int source, int nfinished )
{
  int rank;

  MPI_Comm_rank ( MPI_COMM_WORLD, &rank );

  fprintf ( stdout, 
	    "pe%d in %s, %s, line %d: command %s, source %d, finalized=%d\n", 
	    rank, functionname, filename, line,
	    &commands[tag][0], source, nfinished );
  return;
}

/****************************************************/

void myDebugMsg2 ( const char *filename, const char *functionname, int line,
		   int tag, int source, char * text )
{
  int rank;
  char s[MAXSTRING];

  MPI_Comm_rank ( MPI_COMM_WORLD, &rank );

  fprintf ( stdout, 
	    "pe%d in %s, %s, line %d: command %s, source %d, %s\n", 
	    rank, functionname, filename, line,
	    &commands[tag][0], source, text );
  return;
}

/****************************************************/
