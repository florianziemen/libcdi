#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>

#ifdef USE_MPI
#include "pio_util.h"

char commands[][12] = { "FINALIZE\0",
                       "METADATA\0"};
#endif


/****************************************************/

void pcdiAbort ( char * errorString, const char * filename,  
		 const char *functionname, int line )
{
  int rank;

#ifdef USE_MPI
  MPI_Comm_rank ( MPI_COMM_WORLD, &rank );
  fprintf ( stderr, "ERROR, pe%d in %s, %s, line %d, errorString: \"%s\"\n",
	    rank, functionname, filename, line, errorString );
  MPI_Abort ( MPI_COMM_WORLD, 1 );
#else
  fprintf ( stderr, "ERROR, %s, %s, line %d, errorString: \"%s\"\n",
            functionname, filename, line, errorString );
  abort();
#endif
}

/*****************************************************************************/

void * pcdiXmalloc ( size_t size, const char *filename, int line )
{
  void * value = calloc (1, size );

  if ( value == NULL )
    pcdiAbort ( "malloc failed", filename, line, -1 );

  return value;
}

void * pcdiXcalloc ( size_t nmemb, size_t size, const char *filename, int line )
{
  void * value = calloc ( nmemb, size );

  if ( value == NULL )
    pcdiAbort ( "calloc failed", filename, line, -1 );

  return value;
}

void * pcdiXrealloc ( void *p, size_t size, const char *filename, int line )
{
  void * value = realloc ( p, size );

  if ( value == NULL )
    pcdiAbort ( "realloc failed", filename, line, -1 );

  return value;
}

/***************************************************************/

#ifdef USE_MPI
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
}

/*****************************************************************************/

void pcdiXMPIStat ( int iret, const char *filename, int line, MPI_Status *status )
{
  char errorString[MPI_MAX_ERROR_STRING + 1];
  int len, rank;

  MPI_Comm_rank ( MPI_COMM_WORLD, &rank );

  if ( iret == MPI_ERR_IN_STATUS )
    {
      switch ( status->MPI_ERROR )
        {
          fprintf ( stderr, "------- checking error in request ----------\n" );
        case MPI_SUCCESS :
          fprintf ( stderr, "-------- mpi_success -----------\n" );
          break;
        case MPI_ERR_PENDING:
          fprintf ( stderr, "-------- mpi_err_pending ----------\n");
          break;
        default:
          MPI_Error_string ( status->MPI_ERROR, errorString, &len );
          errorString[len] = '\0';
          fprintf ( stderr,"MPI ERROR in request, pe%d, %s, line %d,"
                    "return value: %d, error_string: %s\n",
                    rank, filename, line, iret, errorString );
          MPI_Abort ( MPI_COMM_WORLD, iret );
        }
    }
  else
    xmpi ( iret );

  return;
}
#endif

/****************************************************/

void pcdiDebug (  const char *filename, const char *functionname, int line )
{
#ifdef USE_MPI
  int rank = 0;

  MPI_Comm_rank ( MPI_COMM_WORLD, &rank );
  fprintf ( stdout,
            "pe%d in %s, %s, line %d\n",
            rank, functionname, filename, line );
#else
  fprintf ( stdout,
            "%s, %s, line %d\n",
            functionname, filename, line );
#endif
}

/*****************************************************************************/

void pcdiDebug2 (  const char *filename, const char *functionname, int line, char * text )
{
#ifdef USE_MPI
  int rank;

  MPI_Comm_rank ( MPI_COMM_WORLD, &rank );

  fprintf ( stdout,
            "pe%d in %s, %s, line %d: %s\n",
            rank, functionname, filename, line, text );
#else
  fprintf ( stdout,
            "%s, %s, line %d\n",
            functionname, filename, line );
#endif
}

/****************************************************/

#ifdef USE_MPI
void pcdiDebugComm ( const char *filename, const char *functionname, int line, MPI_Comm *comm )
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

}
#endif

/****************************************************/

#ifdef USE_MPI
void pcdiDebugMsg ( const char *filename, const char *functionname, int line,
                  int tag, int source, int nfinished )
{
  int rank;

  MPI_Comm_rank ( MPI_COMM_WORLD, &rank );

  fprintf ( stdout,
            "pe%d in %s, %s, line %d: command %s, source %d, finalized=%d\n",
            rank, functionname, filename, line,
            &commands[tag][0], source, nfinished );
}
#endif
/****************************************************/

#ifdef USE_MPI
void pcdiDebugMsg2 ( const char *filename, const char *functionname, int line,
                   int tag, int source, char * text )
{
  int rank;

  MPI_Comm_rank ( MPI_COMM_WORLD, &rank );

  fprintf ( stdout,
            "pe%d in %s, %s, line %d: command %s, source %d, %s\n",
            rank, functionname, filename, line,
            &commands[tag][0], source, text );
}
#endif

/****************************************************/

#ifdef USE_MPI
void pcdiDebugText ( const char *filename, const char *functionname, int line,
		     char * text )
{
  int rank;

  MPI_Comm_rank ( MPI_COMM_WORLD, &rank );

  fprintf ( stdout,
            "pe%d in %s, %s, line %d: %s\n",
            rank, functionname, filename, line, text );
}
#endif
