#include "async_worker.h"

#include "cdi.h"
#include "error.h"

#include <pthread.h>
#include <stdbool.h>

#ifdef __APPLE__
#include <dispatch/dispatch.h>
#else
#include <semaphore.h>
#endif

typedef struct sema {
#ifdef __APPLE__
  dispatch_semaphore_t    sem;
#else
  sem_t                   sem;
#endif
} sema_t;


static inline int
sema_init(sema_t *s, int pshared, uint32_t value)
{
  int status = 0;
#ifdef __APPLE__
  dispatch_semaphore_t *sem = &s->sem;

  *sem = dispatch_semaphore_create(value);
#else
  status = sem_init(&s->sem, pshared, value);
#endif
  return status;
}

static inline int
sema_wait(sema_t *s)
{
#ifdef __APPLE__
  dispatch_semaphore_wait(s->sem, DISPATCH_TIME_FOREVER);
#else
  int r;

  do {
    r = sem_wait(&s->sem);
  } while (r == -1 && errno == EINTR);
#endif
  return 0;
}

static inline int
sema_post(sema_t *s)
{
#ifdef __APPLE__
  dispatch_semaphore_signal(s->sem);
#else
  sem_post(&s->sem);
#endif
  return 0;
}


struct AsyncJob {
  bool inUse;
  sema_t request, completion;
  int (*work)(void* data);
  void* data;
  int result;
};

typedef struct AsyncManager {
  int workerCount, idleWorkerCount;
  AsyncJob* communicators;
} AsyncManager;

static AsyncManager* gManager = NULL;

static
void* workerMain(void* arg)
{
  AsyncJob* communicator = arg;
  while(true)
    {
      while(sema_wait(&communicator->request)) ;
      if(communicator->work)
        {
          communicator->result = communicator->work(communicator->data);
          if(sema_post(&communicator->completion)) xabort("sema_post() failed");
        }
      else
        {
          if(sema_post(&communicator->completion)) xabort("sema_post() failed");
          break;
        }
    }
  return NULL;
}

static
void startWorker(AsyncJob* communicator)
{
  communicator->inUse = false;
  communicator->work = NULL;
  communicator->data = NULL;
  communicator->result = 0;
  if(sema_init(&communicator->request, 0, 0)) xabort("sema_init() failed");
  if(sema_init(&communicator->completion, 0, 0)) xabort("sema_init() failed");
  pthread_t worker;
  if(pthread_create(&worker, NULL, workerMain, communicator)) xabort("pthread_create() failed");
  if(pthread_detach(worker)) xabort("pthread_detach() failed");
}

int AsyncWorker_init(int threadCount)
{
  if(threadCount <= 0)
    {
      xabort("CPU core count discovery not implemented yet");
      return CDI_EINVAL;	//TODO: discover CPU core count, and set threadCount to a sensible positive value
    }
  if(gManager) return CDI_NOERR;

  gManager = malloc(sizeof*gManager);
  if(!gManager) return CDI_ESYSTEM;
  gManager->workerCount = threadCount;
  gManager->communicators = malloc(threadCount*sizeof*gManager->communicators);
  if(!gManager->communicators) xabort("memory allocation failure");
  for(int i = 0; i < threadCount; i++) startWorker(&gManager->communicators[i]);
  gManager->idleWorkerCount = threadCount;

  return CDI_NOERR;
}

AsyncJob* AsyncWorker_requestWork(int (*work)(void* data), void* data)
{
  if(!gManager) xabort("AsyncWorker_requestWork() called without calling AsyncWorker_init() first");
  if(!work) xabort("AsyncWorker_requestWork() called without a valid function pointer");	//need to catch this condition to stop users from terminating our worker threads

  //find an unused worker
  if(!gManager->idleWorkerCount) return NULL;
  AsyncJob* worker = NULL;
  for(int i = 0; i < gManager->workerCount; i++)
    {
      if(!gManager->communicators[i].inUse)
        {
          worker = &gManager->communicators[i];
          break;
        }
    }
  if(!worker) xabort("internal error: idleWorkerCount is not in sync with the worker states, please report this bug");

  //pass the request to that worker
  gManager->idleWorkerCount--;
  worker->inUse = true;
  worker->work = work;
  worker->data = data;
  worker->result = 0;
  if(sema_post(&worker->request)) xabort("sema_post() failed");

  return worker;
}

int AsyncWorker_wait(AsyncJob* job)
{
  if(!gManager) xabort("AsyncWorker_wait() called without calling AsyncWorker_init() first");
  if(job < gManager->communicators) return CDI_EINVAL;
  if(job >= gManager->communicators + gManager->workerCount) return CDI_EINVAL;
  if(!job->inUse) return CDI_EINVAL;

  while(sema_wait(&job->completion)) ;
  int result = job->result;

  //reset the communicator
  job->work = NULL;
  job->data = NULL;
  job->result = 0;
  job->inUse = false;
  gManager->idleWorkerCount++;

  return result;
}

int AsyncWorker_availableWorkers()
{
  if(!gManager) return 0;
  return gManager->idleWorkerCount;
}

int AsyncWorker_finalize()
{
  int result = CDI_NOERR;
  if(!gManager) return CDI_NOERR;
  for(int i = 0; i < gManager->workerCount; i++)
    {
      AsyncJob* curWorker = &gManager->communicators[i];

      //finish any pending job
      if(curWorker->inUse)
        {
          AsyncWorker_wait(curWorker);
          if(curWorker->result) result = curWorker->result;
        }

      //send the teardown signal
      curWorker->inUse = true;
      curWorker->work = NULL;
      curWorker->data = NULL;
      curWorker->result = 0;
      if(sema_post(&curWorker->request)) xabort("sema_post() failed");

      //wait for the worker to exit
      AsyncWorker_wait(curWorker);
    }
  free(gManager->communicators);
  free(gManager);
  gManager = NULL;
  return result;
}
