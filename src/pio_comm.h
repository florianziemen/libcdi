#ifndef PIO_INFO_
#define PIO_INFO_

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#ifdef USE_MPI

#include <mpi.h>

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

extern pioInfo_t * pioinfo2;

void pioInfoNew ( void );
void pioInfoDestroy ( void );


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
