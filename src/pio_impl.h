#ifndef _PIO_IMPL_H
#define _PIO_IMPL_H

#include <stdbool.h>

#ifndef NOMPI
#include "mpi.h"

typedef enum 
{
  IO_Open_file,
  IO_Close_file,
  IO_Get_fp,
  IO_Set_fp,
  IO_Send_buffer,
  IO_Send_buffer_size,
  IO_Finish
} IO_Server_command;

const char *command2charP[7] = {"IO_Open_file", "IO_Close_file",
				"IO_Get_fp","IO_Set_fp",
				"IO_Send_buffer", "IO_Send_buffer_size",
				"IO_Finish"};

struct dBuffer
{
  size_t wr_pointer;
  size_t size;
  unsigned char *buffer;
};

struct listElem 
{
  int idx;
  void *val;
  struct listElem * next;
};

typedef struct listElem node;

typedef int ( * valDestroyFunction ) ( void * );
typedef bool ( * keyCompareFunction ) ( void *, void * );

typedef struct 
{
  node *head;
  node *tail;
  valDestroyFunction valDestroy;
  keyCompareFunction keyCompare;
  int count;
} queue;

// pio_dbuffer.c
int dbuffer_init ( struct dBuffer**, size_t );
int dbuffer_push ( struct dBuffer*, unsigned char*, size_t );
size_t dbuffer_data_size ( struct dBuffer* );
int dbuffer_reset ( struct dBuffer* );
void dbuffer_cleanup ( struct dBuffer ** );
size_t dbuffer_free ( struct dBuffer* );

// pio_queue.c
queue *queueInit ( valDestroyFunction, keyCompareFunction );
void queueDestroy ( queue * );
void queuePush ( queue *, void * );
void *queueIdx2val ( queue *, int );
int queueDelNode ( queue *, int );

// pio_posixfpguardsendrecv.c
void fpgPOSIXFPGUARDSENDRECV ( int );
int  fowPOSIXFPGUARDSENDRECV ( const char* );
int   fcPOSIXFPGUARDSENDRECV ( int );
size_t   fwPOSIXFPGUARDSENDRECV ( int, int, const void*, size_t );
int initPOSIXFPGUARDSENDRECV ( int* );

#endif
#endif
