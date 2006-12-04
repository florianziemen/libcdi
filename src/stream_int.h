#ifndef _STREAM_INT_H
#define _STREAM_INT_H

#if defined (HAVE_CONFIG_H)
#  include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <sys/types.h>

#ifndef strdupx
#ifndef strdup
char *strdup(const char *s);
#endif
#define strdupx  strdup
/*
#define strdupx(s)			          \
({					      	  \
   const char *__old = (s);			  \
   size_t __len = strlen(__old) + 1;		  \
   char *__new = (char *) malloc(__len);	  \
   (char *) memcpy(__new, __old, __len);	  \
})
*/
#endif

#ifndef M_PI
#define M_PI		3.14159265358979323846	/* pi */
#endif


#ifndef  _ERROR_H
#  include "error.h"
#endif
#ifndef _BASETIME_H
#  include "basetime.h"
#endif
#ifndef  _TAXIS_H
#  include "taxis.h"
#endif
#ifndef  _CDI_LIMITS_H
#  include "cdi_limits.h"
#endif
#ifndef  _SERVICE_H
#  include "service.h"
#endif
#ifndef  _EXTRA_H
#  include "extra.h"
#endif
#ifndef  _IEG_H
#  include "ieg.h"
#endif

#ifndef DBL_IS_EQUAL
/*
#define DBL_IS_EQUAL(x,y) (fabs(x - y) <= 2.0*(y*DBL_EPSILON + DBL_MIN))
*/
#define DBL_IS_EQUAL(x,y) (!(fabs(x - y) > 0))
#endif

#ifndef INT
#  define  INT(x)  ((int)(x))
#endif

#ifndef NINT
#  define  NINT(x)  ((x) < 0 ? (int)((x)-.5) : (int)((x)+.5))
#endif

#define  FALSE  0
#define  TRUE   1

#define  TYPE_REC  0
#define  TYPE_VAR  1


typedef struct
{
  void     *buffer;
  size_t    buffersize;
  off_t     position;
  int       recsize;
  int       size;
  int       dataread;
  int       code;
  int       level;
  int       date;
  int       time;
  int       gridID;
  int       zaxisID;
  int       used;
  int       nrec;
  int       varID;
  int       levelID;
  int       recid;
  int       prec;
  int       sec0[2];
  int       sec1[1024];
  int       sec2[4096];
  int       sec3[2];
  int       sec4[512];
#if  defined  (HAVE_LIBSERVICE)
  SRVREC   *srvp;
#endif
#if  defined  (HAVE_LIBEXTRA)
  EXTREC   *extp;
#endif
#if  defined  (HAVE_LIBIEG)
  IEGREC   *iegp;
#endif
}
Record;


typedef struct
{
  off_t     position;
  size_t    size;
  int       zip;
  int       ilevel;
  int       ilevel2;
  int       ltype;
  short     table;
  short     used;
  short     code;
  short     varID;
  short     levelID;
}
RECORD;


typedef struct {
  RECORD   *records;
  int       recordSize;  /* number of allocated records           */
  int      *recIDs;      /* IDs of non constant records           */
  int       nrecs;       /* number of used records                */
                         /* tsID=0 nallrecs                       */
                         /* tsID>0 number of non constant records */
  int       nallrecs;    /* number of all records                 */
  int       curRecID;    /* current record ID                     */
  long      next;
  off_t     position;    /* timestep file position                */
  TAXIS     taxis;
}
TSTEPS;


typedef struct {
  int       ncvarid;
  int       nlevs;
  int      *level;       /* record IDs */
  int      *lindex;      /* level index */
  int       defmiss;     /* TRUE if missval is defined in file */

  int       isUsed;
  int       gridID;
  int       zaxisID;
  int       timeID;      /* ( TIME_VARIABLE or TIME_CONSTANT ) */
  int       nlevel;
}
SVARINFO;


typedef struct {
  int       ilev;
  int       mlev;
  int       ilevID;
  int       mlevID;
}
VCT;


typedef struct {
  int       self;
  int       accesstype;   /* TYPE_REC or TYPE_VAR */
  int       accessmode;
  int       filetype;
  int       byteorder;
  int       fileID;
  int       dimgroupID;
  int       filemode;
  off_t     numvals;
  char     *filename;
  char     *ctlname;
  FILE     *ctlfp;
  Record   *record;
  int       nrecs;        /* number of records                  */
  int       nvars;        /* number of variables                */
  int       varlocked;    /* variables locked                   */
  SVARINFO *vars;
  int       varsAllocated;
  int       varinit;
  int       curTsID;      /* current timestep ID */
  int       rtsteps;      /* number of tsteps accessed       */
  long      ntsteps;      /* number of tsteps : only set if all records accessed */
  int       numTimestep;  /* number of tsteps : only set if all records accessed */
  TSTEPS   *tsteps;
  int       tstepsTableSize;
  int       tstepsNextID;
  BaseTime  basetime;
  int       ncmode;
  int       vlistID;
  int       xdimID[MAX_GRIDS_PS];
  int       ydimID[MAX_GRIDS_PS];
  int       zaxisID[MAX_ZAXIS_PS];
  int       ncxvarID[MAX_GRIDS_PS];
  int       ncyvarID[MAX_GRIDS_PS];
  int       ncavarID[MAX_GRIDS_PS];
  int       historyID;
  int       globalatts;
  int       localatts;
  VCT       vct;
  int       unreduced;
  int       compressType;
  int       compressLevel;
}
STREAM;


extern int CDI_Debug;      /* If set to 1, debuggig (default 0)            */
extern double cdiDefaultMissval;
extern int cdiDefaultInstID;
extern int cdiDefaultModelID;
extern int cdiDefaultTableID;
extern int cdiDefaultLeveltype;
extern int cdiNcMissingValue;
extern int cdiSplitLtype105;

STREAM *stream_to_pointer(int idx);
STREAM *stream_new_entry(void);
void stream_delete_entry(STREAM *streamptr);
void stream_check_ptr(const char *func, STREAM *streamptr);

int     streamInqFileID(int streamID);

int     zaxisInqLevelID(int zaxisID, double level);
char   *gridNamePtr(int gridtype);
char   *zaxisNamePtr(int leveltype);

void    streamCheckID(const char *func, int streamID);

void    streamDefineTaxis(int streamID);

int     streamsNewEntry(int filetype);
void    streamsInitEntry(int streamID);
int     streamNewVar(int streamID, int gridID, int zaxisID);

int     tstepsNewEntry(int streamID);

char   *strfiletype(int filetype);

void    cdiGenVars(int streamID);

void    cdiCheckContents(int streamID);

void    cdiCreateRecords(int streamID, int tsID);

int     recordNewEntry(int streamID, int tsID);

void    cdiCreateTimesteps(int streamID);

void    recordInitEntry(RECORD *record);

void    cdiCheckZaxis(int zaxisID);

void    cdiPrintDatatypes(void);

void    cdiDefAccesstype(int streamID, int type);
int     cdiInqAccesstype(int streamID);

void    streamDefDimgroupID(int streamID, int dimgroupID);
int     streamInqDimgroupID(int streamID);

int getByteswap(int byteorder);

#endif  /* _STREAM_INT_H */
