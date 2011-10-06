#ifndef PIO_INFO_
#define PIO_INFO_

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#ifdef USE_MPI

#include <mpi.h>
#include <stdio.h>
#include "cdi.h"

typedef struct {
  int hostID;
  int isProcColl;
  int nNodes;
} nodeInfo_t;

typedef struct {
  int IOModus;
  int nProcsIO;
  int nProcsModel;
  int isProcIO;

  MPI_Comm commGlob;
  int sizeGlob;
  int rankGlob;
  int root;

  MPI_Comm commPio;
  int sizePio;
  int rankPio;

  MPI_Comm commNode;
  int sizeNode;
  int rankNode;
  char hostname [ MPI_MAX_PROCESSOR_NAME + 1 ];
  nodeInfo_t nodeInfo;
  int * nodeSizes;
  int * nodeMap;
  int specialRankNode;

  MPI_Comm commColl;
  int sizeColl;
  int rankColl;

  MPI_Comm commCalc;
  int sizeCalc;
  int rankCalc;
 
  MPI_Comm * commsIO;  
} pioInfo_t;

extern pioInfo_t * pioinfo2;

enum { MAXNPROCSIO = 256,
       MAXIOMODUS  = PIO_POSIX_FPGUARD_SENDRECV};

enum { NODEINFO = 9999, sizeNodeInfo = 3 };

void     commInit           ( void );
void     commDestroy        ( void );
void     commDefCommGlob    ( MPI_Comm ); 
MPI_Comm commInqCommGlob    ( void );
int      commInqSizeGlob    ( void );
int      commInqRankGlob    ( void );
void     commDefNProcsIO    ( int );
int      commInqNProcsIO    ( void );
int      commInqNProcsModel ( void );
void     commDefIOModus     ( int );
int      commInqIOModus     ( void );
void     commDefCommPio     ( void );
MPI_Comm commInqCommPio     ( void );
MPI_Comm commInqCommModel   ( void );
int      commInqIsProcIO    ( void );
void     commDefIsProcColl  ( int );
void     commDefCommNode    ( MPI_Comm );
void     commDefHostName    ( char *, size_t );
void     commDefHostID      ( int );
void     commDefNNodes      ( int );
void     commDefCommsIO     ( void );
void     commPrint          ( FILE * );

void     commSendNodeInfo   ( void );
void     commDefCommsIO     ( void );



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
