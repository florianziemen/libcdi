#ifndef _SCT_TIMER_HELPER_H
#define _SCT_TIMER_HELPER_H

#ifndef EXTERN
#define EXTERN extern
#endif

#define TEST_RUN 0
#define MODEL_RUN 1

#define TIMER_MAX 128
// size of the timer arrays
#define TIMER_MAX_STEPS 16
// TIMER_LOG undefined: no timer output to the log 
// TIMER_LOG defined:   output timer output to the log
#define TIMER_LOG
EXTERN int timer_ioTotal;
EXTERN int timer_getTimeStepData;
EXTERN int timer_winStart;
EXTERN int timer_mpi_get;
EXTERN int timer_winComplete;
EXTERN int timer_yaxt_exchange;
EXTERN int timer_postwritebatch;
EXTERN int timer_readgetbuffers;
EXTERN int timer_readgetbuffers_arr[TIMER_MAX_STEPS];
EXTERN int timer_writeStream;
EXTERN int timer_preWriteChunk;
EXTERN int timer_writeDeco;
EXTERN int timer_streamWriteVarChunk;
EXTERN int timer_writeSlice;
EXTERN int timer_cdipioFileWrite;
EXTERN int timer_mpiAllGather;
EXTERN int timer_mpiFileWriteAtAll;
EXTERN int timer_StreamMappingNew;
EXTERN int timer_defTimeStep;

EXTERN int context_handle;

EXTERN int timer_globTotal;

extern int test_cdi_pio;
#endif
