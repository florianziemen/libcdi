#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/cdi.h"

#define PARIO 1

#ifdef PARIO
#define HAVE_LIBPTHREAD 1
#endif

#if  defined  (HAVE_LIBPTHREAD)
#  include <pthread.h>
#endif

typedef struct {
  int streamID;
  int *varID, *levelID, *nmiss;
  double *array;
}
read_arg_t;


typedef struct work_st{
  void (*routine) (void*);
  void *arg;
} work_t;


typedef struct {
  int varID, levelID, nmiss;
  double *array;
  int array_size;
  int recID, nrecs;
  read_arg_t read_arg;
  int used;
  work_t *work;
#if  defined  (HAVE_LIBPTHREAD)
  pthread_t thrID;
  pthread_attr_t attr;
  pthread_mutex_t lock;  
  pthread_cond_t not_empty;
  pthread_cond_t empty;
#endif
} iothread_t;


typedef struct {
  int varID, levelID, nmiss;
  double *array;
  int array_size;
  int recID, nrecs;
  read_arg_t read_arg;
  iothread_t *iothread;
#if  defined  (HAVE_LIBPTHREAD)
  pthread_t thrID;
  pthread_attr_t attr;
#endif
}
par_io_t;


void readRecord(void *arg)
{
  int streamID;
  int *varID, *levelID, *nmiss;
  double *array;
  read_arg_t *read_arg = (read_arg_t *) arg;

  streamID = read_arg->streamID;
  varID    = read_arg->varID;
  levelID  = read_arg->levelID;
  nmiss    = read_arg->nmiss;
  array    = read_arg->array;

  /* fprintf(stderr, "streamInqRecord: streamID = %d\n", streamID); */
  streamInqRecord(streamID, varID, levelID);
  fprintf(stderr, "readRecord: streamID = %d %d %d\n", streamID, *varID, *levelID);
  streamReadRecord(streamID, array, nmiss);
  /* fprintf(stderr, "readRecord: varID %d levelID %d\n", *varID, *levelID); */

  // return (NULL);
}

/* This function is the work function of the thread */
void *do_work(void *p)
{
  iothread_t *iothread = (iothread_t *) p;
  work_t *work;	//The q element

  while ( 1 )
    {
      pthread_mutex_lock(&(iothread->lock));  //get the q lock.

      while ( iothread->used == 0 )  // if the size is 0 then wait.
	{
	  // wait until the condition says its no emtpy and give up the lock. 
	  pthread_mutex_unlock(&(iothread->lock));  //get the lock.
	  pthread_cond_wait(&(iothread->not_empty), &(iothread->lock));
	}

      work = iothread->work;
      iothread->used = 0;

      // the q is empty again, now signal that its empty.
      pthread_cond_signal(&(iothread->empty));

      pthread_mutex_unlock(&(iothread->lock));

      fprintf(stderr, "do_work\n");
      (work->routine) (work->arg);   //actually do work.
      free(work);
    }
}


iothread_t *create_iothread()
{
  iothread_t *iothread = NULL;

  iothread = (iothread_t *) malloc(sizeof(iothread_t));
  if ( iothread == NULL )
    {
      fprintf(stderr, "Out of memory creating a new iothread!\n");
      return (NULL);
    }

  //initialize mutex and condition variables.  
  if ( pthread_mutex_init(&iothread->lock, NULL) ) 
    {
      fprintf(stderr, "Mutex initiation error!\n");
      return (NULL);
    }

  if ( pthread_cond_init(&(iothread->empty),NULL) )
    {
      fprintf(stderr, "CV initiation error!\n");	
      return (NULL);
    }

  if ( pthread_cond_init(&(iothread->not_empty),NULL) )
    {
      fprintf(stderr, "CV initiation error!\n");	
      return (NULL);
    }

  pthread_attr_init(&(iothread->attr));
  pthread_attr_setdetachstate(&(iothread->attr), PTHREAD_CREATE_JOINABLE);

  //make thread
  if ( pthread_create(&(iothread->thrID), &(iothread->attr), do_work, (void *)iothread) )
    {
      fprintf(stderr, "Thread initiation error!\n");	
      return NULL;		
    }

  return (iothread);
}

typedef void (*dispatch_fn)(void *);

void dispatch(iothread_t *iothread, dispatch_fn dispatch_to_here, void *arg)
{
  work_t *work;

  //make a work element.  
  work = (work_t*) malloc(sizeof(work_t));
  if ( work == NULL )
    {
      fprintf(stderr, "Out of memory creating a work struct!\n");
      return;	
    }

  work->routine = dispatch_to_here;
  work->arg     = arg;

  pthread_mutex_lock(&(iothread->lock));

  if ( iothread->used == 0 )
    {
      iothread->work = work;
      pthread_cond_signal(&(iothread->not_empty));  //I am not empty.  
    } 

  iothread->used++;

  pthread_mutex_unlock(&(iothread->lock));
}


void destroy_iothread(iothread_t *iothread)
{
  pthread_mutex_destroy(&(iothread->lock));
  pthread_cond_destroy(&(iothread->empty));
  pthread_cond_destroy(&(iothread->not_empty));

  return;
}


void stream_read_record_par(int streamID, int *varID, int *levelID, double *array, int *nmiss, par_io_t *parIO)
{
  int lpario = 0;
  int recID = 0, nrecs = 0;
#if  defined  (HAVE_LIBPTHREAD)
  pthread_t thrID;
  /* pthread_attr_t attr; */
  int rval;
#endif

#if  defined  (HAVE_LIBPTHREAD)
  if ( parIO )
    {
      lpario = 1;
      recID = parIO->recID;
      nrecs = parIO->nrecs;
      thrID = parIO->thrID;
    }
#endif

  if ( recID == 0 || lpario == 0 )
    {
      read_arg_t read_arg;
      read_arg.streamID = streamID;
      read_arg.varID    = varID;
      read_arg.levelID  = levelID;
      read_arg.nmiss    = nmiss;
      read_arg.array    = array;

      readRecord(&read_arg);
    }
#if  defined  (HAVE_LIBPTHREAD)
  else
    {
      /* fprintf(stderr, "parIO1: %ld streamID %d %d %d\n", (long)thrID, streamID, recID, nrecs); */
      /*
      rval = pthread_join(thrID, NULL);
      if ( rval != 0 )
	{
	  fprintf(stderr, "pthread_join failed!\n");
	  exit(-1);
	}
      */

      *varID    = parIO->varID;
      *levelID  = parIO->levelID;
      *nmiss    = parIO->nmiss;
      /* fprintf(stderr, "parIO2: %ld streamID %d %d %d\n", (long)thrID, streamID, *varID, *levelID); */
      memcpy(array, parIO->array, parIO->array_size*sizeof(double));
    }

  if ( lpario && nrecs > 1 )
    {
      read_arg_t *read_arg = &(parIO->read_arg);
      if ( (recID+1) < nrecs )
	{
	  if ( recID == 0 )
	    {
	      pthread_attr_init(&parIO->attr);
	      pthread_attr_setdetachstate(&parIO->attr, PTHREAD_CREATE_JOINABLE);

	      parIO->iothread = create_iothread();
	    }

	  read_arg->streamID = streamID;
	  read_arg->varID    = &parIO->varID;
	  read_arg->levelID  = &parIO->levelID;
	  read_arg->nmiss    = &parIO->nmiss;
	  read_arg->array    = parIO->array;
	  sleep(1);
	  dispatch(parIO->iothread, readRecord, read_arg);		
	}
      else
	{
	  destroy_iothread(parIO->iothread);
	  pthread_attr_destroy(&parIO->attr);
	}
    }
#endif
}


void stream_read_record(int streamID, int *varID, int *levelID, double *data, int *nmiss)
{
  streamInqRecord(streamID, varID, levelID);
  streamReadRecord(streamID, data, nmiss);
}


int main(int argc, char *argv[])
{
  int taxisID, vlistID, varID, levelID, streamID, tsID;
  int nmiss, vdate, vtime;
  int nrecs, recID;
  int gridsize, i;
  double *data;
  double fmin, fmax, fmean;
  par_io_t parIO;
  char *fname = NULL;

  if ( argc != 2 ) 
    {
      fprintf(stderr, "usage: %s filename\n", argv[0]);
      return (-1);
    }

  fname = argv[1];

  /* Open the dataset */
  streamID = streamOpenRead(fname);
  if ( streamID < 0 )
    {
      fprintf(stderr, "%s\n", cdiStringError(streamID));
      return(1);
    }

  /* Get the variable list of the dataset */
  vlistID = streamInqVlist(streamID);

  /* Get the Time axis from the variable list */
  taxisID = vlistInqTaxis(vlistID);
  
  gridsize = vlistGridsizeMax(vlistID);
  data = (double *) malloc(gridsize*sizeof(double));
#ifdef PARIO
  parIO.array = (double *) malloc(gridsize*sizeof(double));
  parIO.array_size = gridsize;
#endif

  /* Loop over all time steps */
  tsID = 0;
  while ( (nrecs = streamInqTimestep(streamID, tsID)) )
    {
      /* Get the verification date and time */
      vdate = taxisInqVdate(taxisID);
      vtime = taxisInqVtime(taxisID);

      /* Read all records */
      for ( recID = 0; recID < nrecs; recID++ )
	{ 
	  parIO.recID = recID; parIO.nrecs = nrecs;
#ifdef PARIO
	  stream_read_record_par(streamID, &varID, &levelID, data, &nmiss, &parIO);
#else
	  stream_read_record(streamID, &varID, &levelID, data, &nmiss);
#endif
	  gridsize = gridInqSize(vlistInqVarGrid(vlistID, varID));
	  fmin  =  1.e33;
	  fmax  = -1.e33;
	  fmean = 0;
	  for ( i = 0; i < gridsize; ++i )
	    {
	      if ( data[i] < fmin ) fmin = data[i];
	      if ( data[i] > fmax ) fmax = data[i];
	      fmean += data[i];
	    }
	  fmean /= gridsize;
	  fprintf(stderr, "%3d %3d %d %g %g %g\n", varID, levelID, gridsize, fmin, fmean, fmax);
	}

      tsID++;
    }
  //printf("%3d %3d %d %g %g %g\n", varID, levelID, gridsize, fmin, fmean, fmax);

	  /* Close the input stream */
  streamClose(streamID);

  free(data);

  return 0;
}
