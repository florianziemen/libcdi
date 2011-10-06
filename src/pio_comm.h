#ifndef PIO_INFO_
#define PIO_INFO_

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#ifdef USE_MPI

#include <mpi.h>
#include "cdi.h"

typedef struct {
  int IOModus;
  int nProcsIO;

  MPI_Comm commGlob;
  int sizeGlob;
  int rankGlob;

  MPI_Comm commPio;
  int sizePio;
  int rankPio;

  MPI_Comm commModel;
  int sizeModel;
  int rankModel;

  MPI_Comm commNode;
  int sizeNode;
  int rankNode;

  int specialRankNode;
  char hostname [ MPI_MAX_PROCESSOR_NAME + 1 ];
  int hostID;

  MPI_Comm commColl;
  int sizeColl;
  int rankColl;

  MPI_Comm commCalc;
  int sizeCalc;
  int rankCalc;
 
  MPI_Comm * commsIO;  
} pioInfo_t;

enum { MAXNPROCSIO = 256,
       MAXIOMODUS  = PIO_POSIX_FPGUARD_SENDRECV};

extern pioInfo_t * pioinfo2;

void     commInit        ( void );
void     commDestroy     ( void );
void     commDefCommGlob ( MPI_Comm ); 
MPI_Comm commInqCommGlob ( void );
int      commInqSizeGlob ( void );
int      commInqRankGlob ( void );
void     commDefNProcsIO ( int );
int      commInqNProcsIO ( void );
void     commDefIOModus  ( int );
int      commInqIOModus  ( void );
void     commPrint ( FILE * );


#endif

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
