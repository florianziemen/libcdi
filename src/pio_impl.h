#ifndef _PIO_IMPL_H
#define _PIO_IMPL_H

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdbool.h>

#ifdef USE_MPI
#include "mpi.h"

typedef enum 
{
  IO_Open_file,
  IO_Close_file,
  IO_Get_fp,
  IO_Set_fp,
  IO_Send_buffer,
  IO_Finalize
} IO_Server_command;

struct dBuffer
{
  size_t wr_pointer;
  size_t size;
  unsigned char * buffer;
};

struct listElem 
{
  int idx;
  void * val;
  struct listElem * next;
};

typedef struct listElem node;

typedef int ( * valDestroyFunction ) ( void * );
typedef bool ( * keyCompareFunction ) ( void *, void * );

typedef struct 
{
  node * head;
  node * tail;
  valDestroyFunction valDestroy;
  keyCompareFunction keyCompare;
  int count;
} queue;

typedef struct
{
  int id;
  int command;
} rTag;

/* pio.c */
int setTag ( int, int );
rTag * getTag ( int );
void ungetTag ( rTag * );

/* pio_dbuffer.c */
int dbuffer_init ( struct dBuffer **, size_t );
int dbuffer_push ( struct dBuffer *, unsigned char *, size_t );
size_t dbuffer_data_size ( struct dBuffer * );
int dbuffer_reset ( struct dBuffer * );
void dbuffer_cleanup ( struct dBuffer ** );
size_t dbuffer_free ( struct dBuffer * );

/* pio_queue.c */
queue * queueInit ( valDestroyFunction, keyCompareFunction );
void queueDestroy ( queue * );
int queuePush ( queue *, void *, int, ... );
void * queueIdx2val ( queue *, int );
int queueDelNode ( queue *, int );

/* pio_mpinonb.c */
int fowMPINONB ( const char * );
int fcMPINONB ( int );
size_t fwMPINONB( int, int, const void *, size_t );
int initMPINONB ( void );

/* pio_posixasynch.c */
#ifndef _SX
void pwPOSIXASYNCH ( int );
int fowPOSIXASYNCH ( const char * );
int fcPOSIXASYNCH ( int );
size_t fwPOSIXASYNCH( int, int, const void *, size_t );
int initPOSIXASYNCH ( void );
#endif
/* pio_posixfpguardsendrecv.c */
void fpgPOSIXFPGUARDSENDRECV ( int );
int  fowPOSIXFPGUARDSENDRECV ( const char * );
int   fcPOSIXFPGUARDSENDRECV ( int );
size_t   fwPOSIXFPGUARDSENDRECV ( int, int, const void *, size_t );
int initPOSIXFPGUARDSENDRECV ( void );

/* pio_posixfpguardthread.c */
void * fpgPOSIXFPGUARDTHREAD ( void * );
int  fowPOSIXFPGUARDTHREAD ( const char * );
int   fcPOSIXFPGUARDTHREAD ( int );
size_t   fwPOSIXFPGUARDTHREAD ( int, int, const void *, size_t );
int initPOSIXFPGUARDTHREAD ( void );
void finalizePOSIXFPGUARDTHREAD ( void );

/* pio_posixfpguardthreadrefuse.c */
void * fpgPOSIXFPGUARDTHREADREFUSE ( void * );
int  fowPOSIXFPGUARDTHREADREFUSE ( const char * );
int   fcPOSIXFPGUARDTHREADREFUSE  ( int );
size_t   fwPOSIXFPGUARDTHREADREFUSE  ( int, int, const void *, size_t );
int initPOSIXFPGUARDTHREADREFUSE  ( void );
void finalizePOSIXFPGUARDTHREADREFUSE  ( void );

/* pio_posixnonb.c */
void pwPOSIXNONB ( int );
int fowPOSIXNONB ( const char * );
int fcPOSIXNONB ( int );
size_t fwPOSIXNONB( int, int, const void *, size_t );
int initPOSIXNONB ( void );

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
