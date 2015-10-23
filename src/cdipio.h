/*
  CDI PIO C header file

  Include this file in applications to make use of the parallel I/O interfaces
  of CDI.
*/

#ifndef  CDIPIO_H_
#define  CDIPIO_H_

#include <mpi.h>

/* parallel IO IOMode */

#define  PIO_NONE                 0
#define  PIO_MPI                  1
#define  PIO_WRITER               2
#define  PIO_ASYNCH               3
#define  PIO_FPGUARD              4

#define  PIO_MINIOMODE                  PIO_NONE
#define  PIO_MAXIOMODE                  PIO_FPGUARD

#define PIO_ROLE_CLIENT 0
#define PIO_ROLE_COLLECTOR 1
#define PIO_ROLE_WRITER 2
#define PIO_ROLE_WRITER_COLLECTOR 3
#define PIO_ROLE_FPGUARD 4

/* parallel IO routines */
#include <yaxt.h>

void     pioEndDef             ( void );
void     pioEndTimestepping    ( void );
void     pioFinalize           ( void );
/* cdiPioNoPostCommSetup: Dummy function to use as argument to pioInit
 * if no actions are necessary after I/O servers initialize communication */
void cdiPioNoPostCommSetup(void);
/*      pioInit: initialize I/O server processes and communication */
MPI_Comm pioInit(MPI_Comm commSuper, int nProcsIO, int IOMode,
                 int *pioNamespace, float partInflate,
                 void (*postCommSetupActions)(void));
/*      cdiPioInit: initialize I/O server processes and communication */
MPI_Comm cdiPioInit(MPI_Comm commSuper, int IOMode, int clientServerRole,
                    int *pioNamespace, float partInflate,
                    void (*postCommSetupActions)(void));
void     pioWriteTimestep(void);
void     cdiPioRDMAProgress(void);

void     streamWriteVarPart    (int streamID, int varID,
                                const void *data, int nmiss,
                                Xt_idxlist partDesc);
void     streamWriteScatteredVarPart(int streamID, int varID, const void *data,
                                     int numBlocks, const int blocklengths[],
                                     const int displacements[],
                                     int nmiss, Xt_idxlist partDesc);
/* cdiPioCSRLastN: return role codes appropriate to use the last
   \textit{nProcsIO} tasks as I/O servers */
int cdiPioCSRLastN(MPI_Comm commSuper, int IOMode, int nProcsIO);

/* cdiPioCSRFirstN: return role codes appropriate to use the first
   \textit{nProcsIO} tasks as I/O servers */
int cdiPioCSRFirstN(MPI_Comm commSuper, int IOMode, int nProcsIO);

#endif
