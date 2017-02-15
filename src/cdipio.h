/*
  CDI PIO C header file

  Include this file in applications to make use of the parallel I/O interfaces
  of CDI.
*/

#ifndef CDIPIO_H_
#define CDIPIO_H_

// clang-format off

//FINT_ON  <--- don't change or remove this line!!!
// Start of fortran interface for the following routines (make_fint.c)

#include <mpi.h>

/* parallel IO IOMode */

#define PIO_NONE 0
#define PIO_MPI 1
#define PIO_WRITER 2
#define PIO_ASYNCH 3
#define PIO_FPGUARD 4
#define PIO_MPI_FW_ORDERED 5
#define PIO_MPI_FW_AT_ALL 6
#define PIO_MPI_FW_AT_REBLOCK 7

#define PIO_MINIOMODE PIO_NONE
#define PIO_MAXIOMODE PIO_MPI_FW_AT_REBLOCK

#define PIO_ROLE_CLIENT 0
#define PIO_ROLE_COLLECTOR 1
#define PIO_ROLE_WRITER 2
#define PIO_ROLE_WRITER_COLLECTOR 3
#define PIO_ROLE_FPGUARD 4

/* parallel IO routines */
#include <yaxt.h>

void pioEndDef(void);
void pioEndTimestepping(void);
void pioFinalize(void);
/* cdiPioNoPostCommSetup: Dummy function to use as argument to pioInit
 * if no actions are necessary after I/O servers initialize communication */
void cdiPioNoPostCommSetup(void);
/*      pioInit: initialize I/O server processes and communication */
MPI_Comm pioInit(MPI_Comm commSuper, int nProcsIO, int IOMode, int *pioNamespace, float partInflate,
                 void (*postCommSetupActions)(void));
/*      cdiPioInit: initialize I/O server processes and communication */
MPI_Comm cdiPioInit(MPI_Comm commSuper, int confResH, int *pioNamespace);
void pioWriteTimestep(void);
void cdiPioRDMAProgress(void);

void streamWriteVarPart(int streamID, int varID, const void *data, int nmiss, Xt_idxlist partDesc);
void streamWriteScatteredVarPart(int streamID, int varID, const void *data, int numBlocks, const int blocklengths[],
                                 const int displacements[], int nmiss, Xt_idxlist partDesc);
/* cdiPioCSRLastN: return role codes appropriate to use the last
   \textit{nProcsIO} tasks as I/O servers */
int cdiPioCSRLastN(MPI_Comm commSuper, int IOMode, int nProcsIO);

/* cdiPioCSRFirstN: return role codes appropriate to use the first
   \textit{nProcsIO} tasks as I/O servers */
int cdiPioCSRFirstN(MPI_Comm commSuper, int IOMode, int nProcsIO);

/* cdiPioCSRBalanced: return role codes appropriate to use \textit{nProcsIO}
 * tasks distributed on evenly spaced ranks as I/O servers */
int cdiPioCSRBalanced(MPI_Comm commSuper, int IOMode, int nProcsIO);

/* cdiPioStr2IOMode: return integer code corresponding to string
 * representation of mode or -1 if no match was found */
int cdiPioStr2IOMode(const char *modeStr);

/* cdiPioStr2IOMode: return string corresponding to integer
 * code of mode or empty string if code is not valid */
const char *cdiPioIOMode2Str(int IOMode);

/* cdiPioConfCreate: create new configuration object and return its handle */
int cdiPioConfCreate(void);

/* cdiPioConfDestroy: delete configuration object */
void cdiPioConfDestroy(int confResH);

/* cdiPioConfSetPartInflate: set partition imbalance attribute of
 * configuration object */
void cdiPioConfSetPartInflate(int confResH, float partInflate);

/* cdiPioConfGetPartInflate: query partition imbalance attribute of
 * configuration object */
float cdiPioConfGetPartInflate(int confResH);

/* cdiPioConfSetIOMode: set IOMode attribute of configuration object */
void cdiPioConfSetIOMode(int confResH, int IOMode);

/* cdiPioConfGetIOMode: query IOMode attribute of configuration object */
int cdiPioConfGetIOMode(int confResH);

/* cdiPioConfSetCSRole: set role attribute of configuration object */
void cdiPioConfSetCSRole(int confResH, int CSRole);

/* cdiPioConfGetCSRole: query role attribute of configuration object */
int cdiPioConfGetCSRole(int confResH);

/* cdiPioConfSetPostCommSetupActions: set function to be called after
 * setup of client/server communications of configuration object */
void cdiPioConfSetPostCommSetupActions(int confResH, void (*postCommSetupActions)(void));

/* cdiPioConfGetPostCommSetupActions: get function to be called after
 * setup of client/server communications from configuration object */
void (*cdiPioConfGetPostCommSetupActions(int confResH))(void);

/* cdiPioConfSetLargePageAlign should block buffer be aligned to
 * large pages instead of normal pages? */
void cdiPioConfSetLargePageAlign(int confResH, int largePageAlign);

/* cdiPioConfSetLargePageAlign: should block buffer be aligned to
 * large pages instead of normal pages? */
int cdiPioConfGetLargePageAlign(int confResH);

/* cdiPioConfSetRecordAggBufLim: Set limit to pre-encoding step
 * aggregation of data in Mebibyte. Increasing this value trades fewer
 * communication operations between I/O servers for higher packet
 * sizes by transposing more data at once but increases total memory
 * consumed by I/O server processes.  Default size is 128MiB. */
void cdiPioConfSetRecordAggBufLim(int confResH, int lim_mb);

/* cdiPioConfGetRecordAggBufLim: Query size of pre-encoding
 * aggregation buffer in MiB */
int cdiPioConfGetRecordAggBufLim(int confResH);

/* cdiPioConfSetWriteAggBufLim: Set limit for write buffer aggregation
 * (default: 16MiB or value of BUFSIZE environment variable (whichever
 * is larger)). Before writing encoded data records to disk, data
 * up to this size is concatenated. For this reason this must be at
 * least equal in size to the largest GRIB record written.  Increasing
 * this size increases memory consumed by I/O server ranks
 * proportionally and can reduce the number of write operations. This
 * value is rounded to the next (large) page size in many implementations. */
void cdiPioConfSetWriteAggBufLim(int confResH, int lim_mb);

/* cdiPioConfGetWriteAggBufLim: Query the size of write
 * aggregation buffers. */
int cdiPioConfGetWriteAggBufLim(int confResH);

/* cdiPioConfSetAioQueueDepth: Set number of concurrent async I/O
 * requests to create. Depending on implementation, this might
 * increase throughput by increasing the number of concurrent
 * operations but also increases buffer size requirements.
 * (Default value: 4) */
void cdiPioConfSetAioQueueDepth(int confResH, int queue_depth);

/* cdiPioConfGetAioQueueDepth: Query depth of AIO queue. */
int cdiPioConfGetAioQueueDepth(int confResH);

/* cdiPioConfSetRedistCache: set doCache to anything non-zero if data
 * for internal data exchanges is to be cached. This makes sense when
 * the data passed via streamWriteVarPart or streamWriteScatteredVarPart
 * is always decomposed statically using the same partitioning
 * description objects and the sequence of calls to streamWriteVarPart
 * or streamWriteScatteredVarPart for a stream matches the sequence
 * of the previous sequence (divided by pioWriteTimestep) */
void cdiPioConfSetRedistCache(int confResH, int doCache);

/* cdiPioConfSetRedistCache: will data for internal data exchanges
 * be cached? */
int cdiPioConfGetRedistCache(int confResH);

/* cdiPioConfSetXmapCache: set doCache to any non-zero value if data
 * for internal data exchange maps is to be cached. This is helpful
 * when the different variables of a file are decomposed identically
 * and is active by default. Set doCache to 0 if decompositions are
 * dynamic in both variables and time */
void cdiPioConfSetXmapCache(int confResH, int doCache);

/* cdiPioConfGetXmapCache: query xmap caching status */
int cdiPioConfGetXmapCache(int confResH);

/* cdiPioConfSetXmapNew: set method to compute part intersections,
 * defaults to xt_xmap_dist_dir_new */
void cdiPioConfSetXmapNew(int confResH, Xt_xmap (*xmap_new)(Xt_idxlist src_idxlist, Xt_idxlist dst_idxlist, MPI_Comm comm));

/* cdiPioConfSetXmapNew: get method to compute part intersections */
Xt_xmap (*cdiPioConfGetXmapNew(int confResH))(Xt_idxlist src_idxlist, Xt_idxlist dst_idxlist, MPI_Comm comm);
/* convert index lists to stripes prior to intersection computation,
 * defaults to true */
void cdiPioConfSetStripeConversion(int confResH, int doStripify);

/* are index lists of parts converted stripes before being passed
 * to the xmap constructor? */
int cdiPioConfGetStripeConversion(int confResH);

/* cdiPioDistGridCreate: create a grid data structure where the
 * per-coordinate data is distributed over the client tasks
 * chunk_decomposition specifies how to distribute the data of each of
 * the following arrays:
 * x-values, y-values, x-bounds, y-bounds, area, mask and gme mask
 *
 * for 2D grids (all but gridtype == GRID_UNSTRUCTURED), four values specifiy the start and size of each dimension
 * where e.g. chunk_decomposition[1][0] would
 * specify the part size of the y-dimension and chunk_decomposition[0][0] the x dimension start
 * index. For unstructured grids only the x-dimension applies.
 */
int cdiPioDistGridCreate(int gridtype, int size, int xsize, int ysize, int nvertex, const int xy_decomposition_optional[][2],
                         Xt_idxlist partDesc2D, Xt_idxlist partDescX, Xt_idxlist partDescY);

// End of fortran interface
//FINT_OFF  <--- don't change or remove this line!!!

// clang-format on

#endif
