#ifndef _SCT_TIMER_HELPER_H
#define _SCT_TIMER_HELPER_H

#ifndef EXTERN
#define EXTERN extern
#endif

#undef TEST_SCT

#define TIMER_MAX 32
EXTERN int timer_ioTotal;
EXTERN int timer_getTimeStepData;
EXTERN int timer_winStart;
EXTERN int timer_mpi_get;
EXTERN int timer_winComplete;
EXTERN int timer_yaxt_exchange;
EXTERN int timer_postwritebatch;
EXTERN int timer_readgetbuffers;
EXTERN int timer_readgetbuffers_first;
EXTERN int timer_writeStream;
EXTERN int timer_preWriteChunk;
EXTERN int timer_writeDeco;
EXTERN int timer_streamWriteVarChunk;
EXTERN int timer_writeSlice;
EXTERN int timer_cdipioFileWrite;
EXTERN int timer_mpiAllGather;
EXTERN int timer_mpiFileWriteAtAll;

EXTERN int context_handle;

EXTERN int timer_globTotal;

EXTERN int timer_StreamMappingNew;
EXTERN int timer_defTimeStep;
EXTERN int timer_tmp2;
EXTERN int timer_tmp3;

#endif
