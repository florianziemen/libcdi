#ifndef ASYNC_WORKER_H
#define ASYNC_WORKER_H

typedef struct AsyncJob AsyncJob;

// a negative threadCount gives the number of cores that should remain unused by the worker threads, returns an error code
int AsyncWorker_init(int threadCount);

//executes work(data) in a worker thread, must be followed by a call to AsyncWorker_wait()
AsyncJob* AsyncWorker_requestWork(int (*work)(void* data), void* data);

// waits for the async job to finish and returns its result (or some other error code)
int AsyncWorker_wait(AsyncJob* job);

// return the number of workers that are currently idle
int AsyncWorker_availableWorkers();

// waits for all pending jobs to finish, stops all workers, returns a non-zero error code from a pending job if there were any
int AsyncWorker_finalize();

#endif
