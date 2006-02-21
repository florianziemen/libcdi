#ifndef _FILE_H
#define _FILE_H

#include <stdio.h>
#include <sys/types.h>


#define  FILE_UNDEFID   -1

#define  FILE_TYPE_STD   1
#define  FILE_TYPE_MMAP  2

const
char  *fileLibraryVersion(void);

void   fileDebug(int debug);

void  *filePtr(int fileID);

int    fileSetBufferType(int fileID, int type);
void   fileSetBufferSize(int fileID, long buffersize);

int    fileOpen(const char *filename, const char *mode);
int    fileClose(int fileID);

char  *fileInqName(int fileID);
int    fileInqMode(int fileID);

int    fileFlush(int fileID);
void   fileClearerr(int fileID);
int    fileEOF(int fileID);
int    filePtrEOF(void *fileptr);
void   fileRewind(int fileID);

off_t  fileGetPos(int fileID);
int    fileSetPos(int fileID, off_t offset, int whence);

int    fileGetc(int fileID);
int    filePtrGetc(void *fileptr);

size_t fileRead(int fileID, void *ptr, size_t size);
size_t fileWrite(int fileID, const void *ptr, size_t size);

#endif  /* _FILE_H */
