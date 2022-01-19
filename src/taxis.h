#ifndef TAXIS_H
#define TAXIS_H

#include <stdbool.h>

#ifndef RESOURCE_HANDLE_H
#include "resource_handle.h"
#endif

typedef struct
{
  // Date format  YYYYMMDD
  // Time format    hhmmss
  int self;
  bool used;
  short has_bounds;
  int datatype;   // datatype
  int type;       // time type
  int64_t sdate;  // start date
  int stime;      // start time
  int64_t vdate;  // verification date
  int vtime;      // verification time
  int64_t rdate;  // reference date
  int rtime;      // reference time
  int64_t fdate;  // forecast reference date
  int ftime;      // forecast reference time
  int calendar;
  int unit;  // time unit
  int numavg;
  bool climatology;
  int64_t vdate_lb;  // lower bounds of vdate
  int vtime_lb;      // lower bounds of vtime
  int64_t vdate_ub;  // upper bounds of vdate
  int vtime_ub;      // upper bounds of vtime
  int fc_unit;       // forecast time unit
  double fc_period;  // forecast time period
  char *name;
  char *longname;
  char *units;
} taxis_t;

//      taxisInqSdate: Get the start date
int64_t taxisInqSdate(int taxisID);

//      taxisInqStime: Get the start time
int taxisInqStime(int taxisID);

void ptaxisInit(taxis_t *taxis);
void ptaxisCopy(taxis_t *dest, taxis_t *source);
taxis_t *taxisPtr(int taxisID);
void cdiSetForecastPeriod(double timevalue, taxis_t *taxis);
void cdiDecodeTimeval(double timevalue, taxis_t *taxis, int64_t *date, int *time);
double cdiEncodeTimeval(int64_t date, int time, taxis_t *taxis);

void ptaxisDefDatatype(taxis_t *taxisptr, int datatype);
void ptaxisDefName(taxis_t *taxisptr, const char *name);
void ptaxisDefLongname(taxis_t *taxisptr, const char *longname);
void ptaxisDefUnits(taxis_t *taxisptr, const char *units);
void taxisDestroyKernel(taxis_t *taxisptr);
#if !defined(SX)
extern const resOps taxisOps;
#endif

int taxisUnpack(char *unpackBuffer, int unpackBufferSize, int *unpackBufferPos, int originNamespace, void *context,
                int checkForSameID);

#endif /* TAXIS_H */
/*
 * Local Variables:
 * c-file-style: "Java"
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * show-trailing-whitespace: t
 * require-trailing-newline: t
 * End:
 */
