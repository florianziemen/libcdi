#include <stddef.h>

#include "cdi.h"
#include "calendar.h"
#include "dmemory.h"
#include "error.h"
#include "taxis.h"
#include "cdi_cksum.h"
#include "cdi_int.h"
#include "namespace.h"
#include "serialize.h"
#include "resource_handle.h"
#include "resource_unpack.h"

static int DefaultTimeType = TAXIS_ABSOLUTE;
static int DefaultTimeUnit = TUNIT_HOUR;

static const char *Timeunits[] = {
  "undefined", "seconds", "minutes", "quarters", "30minutes", "hours", "3hours", "6hours", "12hours", "days", "months", "years",
};

static int taxisCompareP(void *taxisptr1, void *taxisptr2);
static void taxisDestroyP(void *taxisptr);
static void taxisPrintKernel(taxis_t *taxisptr, FILE *fp);
static int taxisGetPackSize(void *taxisptr, void *context);
static void taxisPack(void *taxisptr, void *buf, int size, int *position, void *context);
static int taxisTxCode(void);

const resOps taxisOps
    = { taxisCompareP, taxisDestroyP, (void (*)(void *, FILE *)) taxisPrintKernel, taxisGetPackSize, taxisPack, taxisTxCode };

#define container_of(ptr, type, member) ((type *) (void *) ((unsigned char *) ptr - offsetof(type, member)))

struct refcount_string
{
  int ref_count;
  char string[];
};

static char *
new_refcount_string(size_t len)
{
  struct refcount_string *container = (struct refcount_string *) Malloc(sizeof(*container) + len + 1);
  container->ref_count = 1;
  return container->string;
}

static void
delete_refcount_string(void *p)
{
  if (p)
    {
      struct refcount_string *container = container_of(p, struct refcount_string, string);
      if (!--(container->ref_count)) Free(container);
    }
}

static char *
dup_refcount_string(char *p)
{
  if (p)
    {
      struct refcount_string *container = container_of(p, struct refcount_string, string);
      ++(container->ref_count);
    }
  return p;
}

#undef container_of

static int TAXIS_Debug = 0;  // If set to 1, debugging

const char *
taxisNamePtr(int taxisID)
{
  taxis_t *taxisptr = (taxis_t *) reshGetVal(taxisID, &taxisOps);
  return taxisptr->name;
}

const char *
tunitNamePtr(int unitID)
{
  int size = sizeof(Timeunits) / sizeof(*Timeunits);
  return (unitID > 0 && unitID < size) ? Timeunits[unitID] : Timeunits[0];
}

static void
date_init(CdiDate *date)
{
  date->year = 0;
  date->month = 0;
  date->day = 0;
}

static void
time_init(CdiTime *time)
{
  time->hour = 0;
  time->minute = 0;
  time->second = 0;
  time->ms = 0;
}

static void
datetime_init(CdiDateTime *datetime)
{
  date_init(&datetime->date);
  time_init(&datetime->time);
}

static bool
datetime_isNE(CdiDateTime datetime1, CdiDateTime datetime2)
{
  if (datetime1.date.year == datetime2.date.year
      && datetime1.date.month == datetime2.date.month
      && datetime1.date.day == datetime2.date.day)
    return false;

  if (datetime1.time.hour == datetime2.time.hour
      && datetime1.time.minute == datetime2.time.minute
      && datetime1.time.second == datetime2.time.second
      && datetime1.time.ms == datetime2.time.ms)
    return false;

  return true;
}

static void
taxisDefaultValue(taxis_t *taxisptr)
{
  taxisptr->self = CDI_UNDEFID;
  taxisptr->used = false;
  taxisptr->datatype = CDI_DATATYPE_FLT64;
  taxisptr->type = DefaultTimeType;
  taxisptr->sdate = 0;
  taxisptr->stime = 0;
  taxisptr->vdate = 0;
  taxisptr->vtime = 0;
  datetime_init(&taxisptr->vdatetime);
  taxisptr->rdate = CDI_UNDEFID;
  taxisptr->rtime = 0;
  taxisptr->fdate = CDI_UNDEFID;
  taxisptr->ftime = 0;
  taxisptr->calendar = CDI_Default_Calendar;
  taxisptr->unit = DefaultTimeUnit;
  taxisptr->numavg = 0;
  taxisptr->climatology = false;
  taxisptr->has_bounds = false;
  taxisptr->vdate_lb = 0;
  taxisptr->vtime_lb = 0;
  taxisptr->vdate_ub = 0;
  taxisptr->vtime_ub = 0;
  datetime_init(&taxisptr->vdatetime_lb);
  datetime_init(&taxisptr->vdatetime_ub);
  taxisptr->fc_unit = DefaultTimeUnit;
  taxisptr->fc_period = 0;
  taxisptr->name = NULL;
  taxisptr->longname = NULL;
  taxisptr->units = NULL;
}

static taxis_t *
taxisNewEntry(cdiResH resH)
{
  taxis_t *taxisptr = (taxis_t *) Malloc(sizeof(taxis_t));

  taxisDefaultValue(taxisptr);
  if (resH == CDI_UNDEFID)
    taxisptr->self = reshPut(taxisptr, &taxisOps);
  else
    {
      taxisptr->self = resH;
      reshReplace(resH, taxisptr, &taxisOps);
    }

  return taxisptr;
}

static void
taxisInit(void)
{
  static bool taxisInitialized = false;

  if (taxisInitialized) return;

  taxisInitialized = true;

  char *env = getenv("TAXIS_DEBUG");
  if (env) TAXIS_Debug = atoi(env);
}

/*
@Function  taxisCreate
@Title     Create a Time axis

@Prototype int taxisCreate(int taxistype)
@Parameter
    @Item  taxistype  The type of the Time axis, one of the set of predefined CDI time axis types.
                      The valid CDI time axis types are @func{TAXIS_ABSOLUTE} and @func{TAXIS_RELATIVE}.

@Description
The function @func{taxisCreate} creates a Time axis.

@Result
@func{taxisCreate} returns an identifier to the Time axis.

@Example
Here is an example using @func{taxisCreate} to create a relative T-axis with a standard calendar.

@Source
#include "cdi.h"
   ...
int taxisID;
   ...
taxisID = taxisCreate(TAXIS_RELATIVE);
taxisDefCalendar(taxisID, CALENDAR_STANDARD);
taxisDefRdate(taxisID, 19850101);
taxisDefRtime(taxisID, 120000);
   ...
@EndSource
@EndFunction
*/
int
taxisCreate(int taxistype)
{
  if (CDI_Debug) Message("taxistype: %d", taxistype);

  taxisInit();

  taxis_t *taxisptr = taxisNewEntry(CDI_UNDEFID);
  taxisptr->type = taxistype;

  int taxisID = taxisptr->self;

  if (CDI_Debug) Message("taxisID: %d", taxisID);

  return taxisID;
}

void
taxisDestroyKernel(taxis_t *taxisptr)
{
  delete_refcount_string(taxisptr->name);
  delete_refcount_string(taxisptr->longname);
  delete_refcount_string(taxisptr->units);
}

/*
@Function  taxisDestroy
@Title     Destroy a Time axis

@Prototype void taxisDestroy(int taxisID)
@Parameter
    @Item  taxisID  Time axis ID, from a previous call to @func{taxisCreate}

@EndFunction
*/
void
taxisDestroy(int taxisID)
{
  taxis_t *taxisptr = (taxis_t *) reshGetVal(taxisID, &taxisOps);
  reshRemove(taxisID, &taxisOps);
  taxisDestroyKernel(taxisptr);
  Free(taxisptr);
}

void
taxisDestroyP(void *taxisptr)
{
  taxisDestroyKernel((taxis_t *) taxisptr);
  Free(taxisptr);
}

int
taxisDuplicate(int taxisID1)
{
  taxis_t *taxisptr1 = (taxis_t *) reshGetVal(taxisID1, &taxisOps);
  taxis_t *taxisptr2 = taxisNewEntry(CDI_UNDEFID);

  int taxisID2 = taxisptr2->self;

  if (CDI_Debug) Message("taxisID2: %d", taxisID2);

  ptaxisCopy(taxisptr2, taxisptr1);

  return taxisID2;
}

void
taxisDefType(int taxisID, int taxistype)
{
  taxis_t *taxisptr = (taxis_t *) reshGetVal(taxisID, &taxisOps);

  if (taxisptr->type != taxistype)
    {
      taxisptr->type = taxistype;
      taxisptr->datatype = CDI_DATATYPE_FLT64;
      if (taxisptr->units)
        {
          delete_refcount_string(taxisptr->units);
          taxisptr->units = NULL;
        }
      reshSetStatus(taxisID, &taxisOps, RESH_DESYNC_IN_USE);
    }
}

/*
@Function  taxisDefVdate
@Title     Define the verification date

@Prototype void taxisDefVdate(int taxisID, DateType vdate)
@Parameter
    @Item  taxisID  Time axis ID, from a previous call to @fref{taxisCreate}
    @Item  vdate    Verification date (YYYYMMDD)

@Description
The function @func{taxisDefVdate} defines the verification date of a Time axis.

@EndFunction
*/
void
taxisDefVdate(int taxisID, DateType vdate)
{
  taxis_t *taxisptr = (taxis_t *) reshGetVal(taxisID, &taxisOps);

  if (taxisptr->vdate != (int64_t) vdate)
    {
      taxisptr->vdate = (int64_t) vdate;
      reshSetStatus(taxisID, &taxisOps, RESH_DESYNC_IN_USE);
    }
}

/*
@Function  taxisDefVtime
@Title     Define the verification time

@Prototype void taxisDefVtime(int taxisID, int vtime)
@Parameter
    @Item  taxisID  Time axis ID, from a previous call to @fref{taxisCreate}
    @Item  vtime    Verification time (hhmmss)

@Description
The function @func{taxisDefVtime} defines the verification time of a Time axis.

@EndFunction
*/
void
taxisDefVtime(int taxisID, int vtime)
{
  taxis_t *taxisptr = (taxis_t *) reshGetVal(taxisID, &taxisOps);

  if (taxisptr->vtime != vtime)
    {
      taxisptr->vtime = vtime;
      reshSetStatus(taxisID, &taxisOps, RESH_DESYNC_IN_USE);
    }
}

void
taxisDefVdatetime(int taxisID, CdiDateTime vdatetime)
{
  taxis_t *taxisptr = (taxis_t *) reshGetVal(taxisID, &taxisOps);

  if (datetime_isNE(taxisptr->vdatetime, vdatetime))
    {
      taxisptr->vdatetime = vdatetime;
      reshSetStatus(taxisID, &taxisOps, RESH_DESYNC_IN_USE);
    }
}

/*
@Function  taxisDefRdate
@Title     Define the reference date

@Prototype void taxisDefRdate(int taxisID, DateType rdate)
@Parameter
    @Item  taxisID  Time axis ID, from a previous call to @fref{taxisCreate}
    @Item  rdate    Reference date (YYYYMMDD)

@Description
The function @func{taxisDefRdate} defines the reference date of a Time axis.

@EndFunction
*/
void
taxisDefRdate(int taxisID, DateType rdate)
{
  taxis_t *taxisptr = (taxis_t *) reshGetVal(taxisID, &taxisOps);

  if (taxisptr->rdate != (int64_t) rdate)
    {
      taxisptr->rdate = (int64_t) rdate;

      if (taxisptr->units)
        {
          delete_refcount_string(taxisptr->units);
          taxisptr->units = NULL;
        }
      reshSetStatus(taxisID, &taxisOps, RESH_DESYNC_IN_USE);
    }
}

/*
@Function  taxisDefRtime
@Title     Define the reference time

@Prototype void taxisDefRtime(int taxisID, int rtime)
@Parameter
    @Item  taxisID  Time axis ID, from a previous call to @fref{taxisCreate}
    @Item  rtime    Reference time (hhmmss)

@Description
The function @func{taxisDefRtime} defines the reference time of a Time axis.

@EndFunction
*/
void
taxisDefRtime(int taxisID, int rtime)
{
  taxis_t *taxisptr = (taxis_t *) reshGetVal(taxisID, &taxisOps);

  if (taxisptr->rtime != rtime)
    {
      taxisptr->rtime = rtime;
      if (taxisptr->units)
        {
          delete_refcount_string(taxisptr->units);
          taxisptr->units = NULL;
        }
      reshSetStatus(taxisID, &taxisOps, RESH_DESYNC_IN_USE);
    }
}

/*
@Function  taxisDefFdate
@Title     Define the forecast reference date

@Prototype void taxisDefFdate(int taxisID, DateType fdate)
@Parameter
    @Item  taxisID  Time axis ID, from a previous call to @fref{taxisCreate}
    @Item  fdate    Forecast reference date (YYYYMMDD)

@Description
The function @func{taxisDefFdate} defines the forecast reference date of a Time axis.

@EndFunction
*/
void
taxisDefFdate(int taxisID, DateType fdate)
{
  taxis_t *taxisptr = (taxis_t *) reshGetVal(taxisID, &taxisOps);

  if (taxisptr->fdate != (int64_t) fdate)
    {
      taxisptr->fdate = (int64_t) fdate;
      reshSetStatus(taxisID, &taxisOps, RESH_DESYNC_IN_USE);
    }
}

/*
@Function  taxisDefFtime
@Title     Define the forecast reference time

@Prototype void taxisDefFtime(int taxisID, int ftime)
@Parameter
    @Item  taxisID  Time axis ID, from a previous call to @fref{taxisCreate}
    @Item  ftime    Forecast reference time (hhmmss)

@Description
The function @func{taxisDefFtime} defines the forecast reference time of a Time axis.

@EndFunction
*/
void
taxisDefFtime(int taxisID, int ftime)
{
  taxis_t *taxisptr = (taxis_t *) reshGetVal(taxisID, &taxisOps);

  if (taxisptr->ftime != ftime)
    {
      taxisptr->ftime = ftime;
      reshSetStatus(taxisID, &taxisOps, RESH_DESYNC_IN_USE);
    }
}

/*
@Function  taxisDefCalendar
@Title     Define the calendar

@Prototype void taxisDefCalendar(int taxisID, int calendar)
@Parameter
    @Item  taxisID  Time axis ID, from a previous call to @fref{taxisCreate}
    @Item  calendar The type of the calendar, one of the set of predefined CDI calendar types.
                    The valid CDI calendar types are @func{CALENDAR_STANDARD}, @func{CALENDAR_PROLEPTIC},
                    @func{CALENDAR_360DAYS}, @func{CALENDAR_365DAYS} and @func{CALENDAR_366DAYS}.

@Description
The function @func{taxisDefCalendar} defines the calendar of a Time axis.

@EndFunction
*/
void
taxisDefCalendar(int taxisID, int calendar)
{
  taxis_t *taxisptr = (taxis_t *) reshGetVal(taxisID, &taxisOps);

  if (taxisptr->calendar != calendar)
    {
      taxisptr->calendar = calendar;
      reshSetStatus(taxisID, &taxisOps, RESH_DESYNC_IN_USE);
    }
}

void
taxisDefTunit(int taxisID, int unit)
{
  taxis_t *taxisptr = (taxis_t *) reshGetVal(taxisID, &taxisOps);

  if (taxisptr->unit != unit)
    {
      taxisptr->unit = unit;
      if (taxisptr->units)
        {
          delete_refcount_string(taxisptr->units);
          taxisptr->units = NULL;
        }
      reshSetStatus(taxisID, &taxisOps, RESH_DESYNC_IN_USE);
    }
}

void
taxisDefForecastTunit(int taxisID, int unit)
{
  taxis_t *taxisptr = (taxis_t *) reshGetVal(taxisID, &taxisOps);

  if (taxisptr->fc_unit != unit)
    {
      taxisptr->fc_unit = unit;
      reshSetStatus(taxisID, &taxisOps, RESH_DESYNC_IN_USE);
    }
}

void
taxisDefForecastPeriod(int taxisID, double fc_period)
{
  taxis_t *taxisptr = (taxis_t *) reshGetVal(taxisID, &taxisOps);

  if (IS_NOT_EQUAL(taxisptr->fc_period, fc_period))
    {
      taxisptr->fc_period = fc_period;
      reshSetStatus(taxisID, &taxisOps, RESH_DESYNC_IN_USE);
    }
}

void
taxisDefNumavg(int taxisID, int numavg)
{
  taxis_t *taxisptr = (taxis_t *) reshGetVal(taxisID, &taxisOps);

  if (taxisptr->numavg != numavg)
    {
      taxisptr->numavg = numavg;
      reshSetStatus(taxisID, &taxisOps, RESH_DESYNC_IN_USE);
    }
}

/*
  The type of the time axis, one of the set of predefined CDI time types.
  The valid CDI time types are TAXIS_ABSOLUTE and TAXIS_RELATIVE.
*/
int
taxisInqType(int taxisID)
{
  const taxis_t *taxisptr = (taxis_t *) reshGetVal(taxisID, &taxisOps);
  return taxisptr->type;
}

int
taxisHasBounds(int taxisID)
{
  const taxis_t *taxisptr = (taxis_t *) reshGetVal(taxisID, &taxisOps);
  return taxisptr->has_bounds;
}

void
taxisWithBounds(int taxisID)
{
  taxis_t *taxisptr = (taxis_t *) reshGetVal(taxisID, &taxisOps);

  if (taxisptr->has_bounds == false)
    {
      taxisptr->has_bounds = true;
      reshSetStatus(taxisID, &taxisOps, RESH_DESYNC_IN_USE);
    }
}

void
taxisDeleteBounds(int taxisID)
{
  taxis_t *taxisptr = (taxis_t *) reshGetVal(taxisID, &taxisOps);

  if (taxisptr->has_bounds)
    {
      taxisptr->has_bounds = false;
      reshSetStatus(taxisID, &taxisOps, RESH_DESYNC_IN_USE);
    }
}

void
taxisCopyTimestep(int taxisID2, int taxisID1)
{
  taxis_t *taxisptr1 = (taxis_t *) reshGetVal(taxisID1, &taxisOps), *taxisptr2 = (taxis_t *) reshGetVal(taxisID2, &taxisOps);

  reshLock();

  /* reference date/time and units can't be changed after streamDefVlist().
  if (taxisptr2->units && taxisptr2->rdate != CDI_UNDEFID)
    {
      if (taxisptr2->rdate != taxisptr1->rdate || taxisptr2->rtime != taxisptr1->rtime)
        {
          delete_refcount_string(taxisptr2->units);
          taxisptr2->units = NULL;
        }
    }

  taxisptr2->rdate = taxisptr1->rdate;
  taxisptr2->rtime = taxisptr1->rtime;
  */

  taxisptr2->sdate = taxisptr1->sdate;
  taxisptr2->stime = taxisptr1->stime;

  taxisptr2->vdate = taxisptr1->vdate;
  taxisptr2->vtime = taxisptr1->vtime;
  taxisptr2->vdatetime = taxisptr1->vdatetime;

  if (taxisptr2->has_bounds)
    {
      taxisptr2->vdate_lb = taxisptr1->vdate_lb;
      taxisptr2->vtime_lb = taxisptr1->vtime_lb;
      taxisptr2->vdate_ub = taxisptr1->vdate_ub;
      taxisptr2->vtime_ub = taxisptr1->vtime_ub;
      taxisptr2->vdatetime_lb = taxisptr1->vdatetime_lb;
      taxisptr2->vdatetime_ub = taxisptr1->vdatetime_ub;
   }

  taxisptr2->fdate = taxisptr1->fdate;
  taxisptr2->ftime = taxisptr1->ftime;

  taxisptr2->fc_unit = taxisptr1->fc_unit;
  taxisptr2->fc_period = taxisptr1->fc_period;

  reshSetStatus(taxisID2, &taxisOps, RESH_DESYNC_IN_USE);
  reshUnlock();
}

static CdiDate
cdiDateSet(int64_t date)
{
  int year, month, day;
  cdiDecodeDate(date, &year, &month, &day);

  CdiDate cdiDate;
  cdiDate.year = year;
  cdiDate.month = (short)month;
  cdiDate.day = (short)day;

  return cdiDate;
}

static CdiTime
cdiTimeSet(int time)
{
  int hour, minute, second;
  cdiDecodeTime(time, &hour, &minute, &second);

  CdiTime cdiTime;
  cdiTime.hour = (short)hour;
  cdiTime.minute = (short)minute;
  cdiTime.second = (short)second;
  cdiTime.ms = 0;

  return cdiTime;
}

CdiDateTime
taxisInqVdatetime(int taxisID)
{
  const taxis_t *taxisptr = (taxis_t *) reshGetVal(taxisID, &taxisOps);

  CdiDateTime datetime;
  datetime.date = cdiDateSet(taxisptr->vdate);
  datetime.time = cdiTimeSet(taxisptr->vtime);

  return datetime;
}


/*
@Function  taxisInqVdate
@Title     Get the verification date

@Prototype DateType taxisInqVdate(int taxisID)
@Parameter
    @Item  taxisID  Time axis ID, from a previous call to @fref{taxisCreate} or @fref{vlistInqTaxis}

@Description
The function @func{taxisInqVdate} returns the verification date of a Time axis.

@Result
@func{taxisInqVdate} returns the verification date.

@EndFunction
*/
DateType
taxisInqVdate(int taxisID)
{
  const taxis_t *taxisptr = (taxis_t *) reshGetVal(taxisID, &taxisOps);
  return (DateType) taxisptr->vdate;
}

int64_t
taxisInqSdate(int taxisID)
{
  const taxis_t *taxisptr = (taxis_t *) reshGetVal(taxisID, &taxisOps);
  return (DateType) taxisptr->sdate;
}

void
taxisInqVdateBounds(int taxisID, DateType *vdate_lb, DateType *vdate_ub)
{
  const taxis_t *taxisptr = (taxis_t *) reshGetVal(taxisID, &taxisOps);

  *vdate_lb = (DateType) taxisptr->vdate_lb;
  *vdate_ub = (DateType) taxisptr->vdate_ub;
}

void
taxisDefVdateBounds(int taxisID, DateType vdate_lb, DateType vdate_ub)
{
  taxis_t *taxisptr = (taxis_t *) reshGetVal(taxisID, &taxisOps);

  if (taxisptr->has_bounds == false || taxisptr->vdate_lb != (int64_t) vdate_lb || taxisptr->vdate_ub != (int64_t) vdate_ub)
    {
      taxisptr->vdate_lb = (int64_t) vdate_lb;
      taxisptr->vdate_ub = (int64_t) vdate_ub;
      taxisptr->has_bounds = true;
      reshSetStatus(taxisID, &taxisOps, RESH_DESYNC_IN_USE);
    }
}

/*
@Function  taxisInqVtime
@Title     Get the verification time

@Prototype int taxisInqVtime(int taxisID)
@Parameter
    @Item  taxisID  Time axis ID, from a previous call to @fref{taxisCreate} or @fref{vlistInqTaxis}

@Description
The function @func{taxisInqVtime} returns the verification time of a Time axis.

@Result
@func{taxisInqVtime} returns the verification time.

@EndFunction
*/
int
taxisInqVtime(int taxisID)
{
  const taxis_t *taxisptr = (taxis_t *) reshGetVal(taxisID, &taxisOps);
  return taxisptr->vtime;
}

int
taxisInqStime(int taxisID)
{
  const taxis_t *taxisptr = (taxis_t *) reshGetVal(taxisID, &taxisOps);
  return taxisptr->stime;
}

void
taxisInqVtimeBounds(int taxisID, int *vtime_lb, int *vtime_ub)
{
  const taxis_t *taxisptr = (taxis_t *) reshGetVal(taxisID, &taxisOps);

  *vtime_lb = taxisptr->vtime_lb;
  *vtime_ub = taxisptr->vtime_ub;
}

void
taxisDefVtimeBounds(int taxisID, int vtime_lb, int vtime_ub)
{
  taxis_t *taxisptr = (taxis_t *) reshGetVal(taxisID, &taxisOps);

  if (taxisptr->vtime_lb != vtime_lb || taxisptr->vtime_ub != vtime_ub || taxisptr->has_bounds == false)
    {
      taxisptr->vtime_lb = vtime_lb;
      taxisptr->vtime_ub = vtime_ub;
      taxisptr->has_bounds = true;
      reshSetStatus(taxisID, &taxisOps, RESH_DESYNC_IN_USE);
    }
}

/*
@Function  taxisInqRdate
@Title     Get the reference date

@Prototype DateType taxisInqRdate(int taxisID)
@Parameter
    @Item  taxisID  Time axis ID, from a previous call to @fref{taxisCreate} or @fref{vlistInqTaxis}

@Description
The function @func{taxisInqRdate} returns the reference date of a Time axis.

@Result
@func{taxisInqRdate} returns the reference date.

@EndFunction
*/
DateType
taxisInqRdate(int taxisID)
{
  taxis_t *taxisptr = (taxis_t *) reshGetVal(taxisID, &taxisOps);

  if (taxisptr->rdate == -1)
    {
      taxisptr->rdate = taxisptr->vdate;
      taxisptr->rtime = taxisptr->vtime;
      reshSetStatus(taxisID, &taxisOps, RESH_DESYNC_IN_USE);
    }

  return (DateType) taxisptr->rdate;
}

/*
@Function  taxisInqRtime
@Title     Get the reference time

@Prototype int taxisInqRtime(int taxisID)
@Parameter
    @Item  taxisID  Time axis ID, from a previous call to @fref{taxisCreate} or @fref{vlistInqTaxis}

@Description
The function @func{taxisInqRtime} returns the reference time of a Time axis.

@Result
@func{taxisInqRtime} returns the reference time.

@EndFunction
*/
int
taxisInqRtime(int taxisID)
{
  taxis_t *taxisptr = (taxis_t *) reshGetVal(taxisID, &taxisOps);

  if (taxisptr->rdate == -1)
    {
      taxisptr->rdate = taxisptr->vdate;
      taxisptr->rtime = taxisptr->vtime;
      reshSetStatus(taxisID, &taxisOps, RESH_DESYNC_IN_USE);
    }

  return taxisptr->rtime;
}

/*
@Function  taxisInqFdate
@Title     Get the forecast reference date

@Prototype DateType taxisInqFdate(int taxisID)
@Parameter
    @Item  taxisID  Time axis ID, from a previous call to @fref{taxisCreate} or @fref{vlistInqTaxis}

@Description
The function @func{taxisInqFdate} returns the forecast reference date of a Time axis.

@Result
@func{taxisInqFdate} returns the forecast reference date.

@EndFunction
*/
DateType
taxisInqFdate(int taxisID)
{
  taxis_t *taxisptr = (taxis_t *) reshGetVal(taxisID, &taxisOps);

  if (taxisptr->fdate == -1)
    {
      taxisptr->fdate = taxisInqRdate(taxisID);
      taxisptr->ftime = taxisInqRtime(taxisID);
    }

  return (DateType) taxisptr->fdate;
}

/*
@Function  taxisInqFtime
@Title     Get the forecast reference time

@Prototype int taxisInqFtime(int taxisID)
@Parameter
    @Item  taxisID  Time axis ID, from a previous call to @fref{taxisCreate} or @fref{vlistInqTaxis}

@Description
The function @func{taxisInqFtime} returns the forecast reference time of a Time axis.

@Result
@func{taxisInqFtime} returns the forecast reference time.

@EndFunction
*/
int
taxisInqFtime(int taxisID)
{
  taxis_t *taxisptr = (taxis_t *) reshGetVal(taxisID, &taxisOps);

  if (taxisptr->fdate == -1)
    {
      taxisptr->fdate = taxisptr->vdate;
      taxisptr->ftime = taxisptr->vtime;
    }

  return taxisptr->ftime;
}

/*
@Function  taxisInqCalendar
@Title     Get the calendar

@Prototype int taxisInqCalendar(int taxisID)
@Parameter
    @Item  taxisID  Time axis ID, from a previous call to @fref{taxisCreate} or @fref{vlistInqTaxis}

@Description
The function @func{taxisInqCalendar} returns the calendar of a Time axis.

@Result
@func{taxisInqCalendar} returns the type of the calendar,
one of the set of predefined CDI calendar types.
The valid CDI calendar types are @func{CALENDAR_STANDARD}, @func{CALENDAR_PROLEPTIC},
@func{CALENDAR_360DAYS}, @func{CALENDAR_365DAYS} and @func{CALENDAR_366DAYS}.

@EndFunction
*/
int
taxisInqCalendar(int taxisID)
{
  const taxis_t *taxisptr = (taxis_t *) reshGetVal(taxisID, &taxisOps);
  return taxisptr->calendar;
}

int
taxisInqTunit(int taxisID)
{
  const taxis_t *taxisptr = (taxis_t *) reshGetVal(taxisID, &taxisOps);
  return taxisptr->unit;
}

int
taxisInqForecastTunit(int taxisID)
{
  const taxis_t *taxisptr = (taxis_t *) reshGetVal(taxisID, &taxisOps);
  return taxisptr->fc_unit;
}

double
taxisInqForecastPeriod(int taxisID)
{
  const taxis_t *taxisptr = (taxis_t *) reshGetVal(taxisID, &taxisOps);
  return taxisptr->fc_period;
}

int
taxisInqNumavg(int taxisID)
{
  const taxis_t *taxisptr = (taxis_t *) reshGetVal(taxisID, &taxisOps);
  return taxisptr->numavg;
}

taxis_t *
taxisPtr(int taxisID)
{
  taxis_t *taxisptr = (taxis_t *) reshGetVal(taxisID, &taxisOps);
  return taxisptr;
}

void
ptaxisDefDatatype(taxis_t *taxisptr, int datatype)
{
  taxisptr->datatype = datatype;
}

void
ptaxisDefName(taxis_t *taxisptr, const char *name)
{
  if (name)
    {
      const size_t len = strlen(name);
      delete_refcount_string(taxisptr->name);
      char *taxisname = taxisptr->name = new_refcount_string(len);
      strcpy(taxisname, name);
    }
}

void
ptaxisDefLongname(taxis_t *taxisptr, const char *longname)
{
  if (longname)
    {
      const size_t len = strlen(longname);
      delete_refcount_string(taxisptr->longname);
      char *taxislongname = taxisptr->longname = new_refcount_string(len);
      strcpy(taxislongname, longname);
    }
}

void
ptaxisDefUnits(taxis_t *taxisptr, const char *units)
{
  if (units)
    {
      const size_t len = strlen(units);
      delete_refcount_string(taxisptr->units);
      char *taxisunits = taxisptr->units = new_refcount_string(len);
      strcpy(taxisunits, units);
    }
}

static void
cdi_decode_timevalue(int timeunit, double timevalue, int64_t *days, int *secs)
{
  *days = 0;
  *secs = 0;

  if (timeunit == TUNIT_MINUTE)
    {
      timevalue *= 60;
      timeunit = TUNIT_SECOND;
    }
  else if (timeunit == TUNIT_HOUR)
    {
      timevalue /= 24;
      timeunit = TUNIT_DAY;
    }

  if (timeunit == TUNIT_SECOND)
    {
      *days = (int) (timevalue / 86400);
      const double seconds = timevalue - *days * 86400.;
      *secs = (int) lround(seconds);
      if (*secs < 0)
        {
          *days -= 1;
          *secs += 86400;
        };
      /*
      {
        double cval = *days*86400. + *secs;
        if ( cval != timevalue )
          printf("TUNIT_SECOND error: %g %g %d %d\n", timevalue, cval, *days, *secs);
      }
      */
    }
  else if (timeunit == TUNIT_DAY)
    {
      *days = (int) timevalue;
      const double seconds = (timevalue - *days) * 86400;
      *secs = (int) lround(seconds);
      if (*secs < 0)
        {
          *days -= 1;
          *secs += 86400;
        };
      /*
      {
        double cval = *days + *secs/86400.;
        if ( cval != timevalue )
          printf("TUNIT_DAY error: %g %g %d %d\n", timevalue, cval, *days, *secs);
      }
      */
    }
  else
    {
      static bool lwarn = true;
      if (lwarn)
        {
          Warning("timeunit %s unsupported!", tunitNamePtr(timeunit));
          lwarn = false;
        }
    }
}

static void
cdiEncodeTimevalue(int days, int secs, int timeunit, double *timevalue)
{
  if (timeunit == TUNIT_SECOND)
    {
      *timevalue = days * 86400. + secs;
    }
  else if (timeunit == TUNIT_MINUTE || timeunit == TUNIT_QUARTER || timeunit == TUNIT_30MINUTES)
    {
      *timevalue = days * 1440. + secs / 60.;
    }
  else if (timeunit == TUNIT_HOUR || timeunit == TUNIT_3HOURS || timeunit == TUNIT_6HOURS || timeunit == TUNIT_12HOURS)
    {
      *timevalue = days * 24. + secs / 3600.;
    }
  else if (timeunit == TUNIT_DAY)
    {
      *timevalue = days + secs / 86400.;
    }
  else
    {
      static bool lwarn = true;
      if (lwarn)
        {
          Warning("timeunit %s unsupported!", tunitNamePtr(timeunit));
          lwarn = false;
        }
    }
}

static void
timeval2vtime(double timevalue, taxis_t *taxis, int64_t *vdate, int *vtime)
{
  int64_t rdate = taxis->rdate;
  int rtime = taxis->rtime;

  if (DBL_IS_EQUAL(timevalue, 0.))
    {
      *vdate = rdate;
      *vtime = rtime;
      return;
    }

  int year, month, day, hour, minute, second;
  cdiDecodeDate(rdate, &year, &month, &day);
  cdiDecodeTime(rtime, &hour, &minute, &second);

  int timeunit = taxis->unit;
  int calendar = taxis->calendar;

  if (timeunit == TUNIT_MONTH && calendar == CALENDAR_360DAYS)
    {
      timeunit = TUNIT_DAY;
      timevalue *= 30;
    }

  if (timeunit == TUNIT_MONTH || timeunit == TUNIT_YEAR)
    {
      if (timeunit == TUNIT_YEAR) timevalue *= 12;

      const int nmon = (int) timevalue;
      const double fmon = timevalue - nmon;

      month += nmon;

      // clang-format off
      while (month > 12) { month -= 12; year++; }
      while (month <  1) { month += 12; year--; }
      // clang-format on

      const int dpm = days_per_month(calendar, year, month);
      timeunit = TUNIT_DAY;
      timevalue = fmon * dpm;
    }

  int64_t julday, days;
  int secofday, secs;
  encode_caldaysec(calendar, year, month, day, hour, minute, second, &julday, &secofday);

  cdi_decode_timevalue(timeunit, timevalue, &days, &secs);

  julday_add(days, secs, &julday, &secofday);

  decode_caldaysec(calendar, julday, secofday, &year, &month, &day, &hour, &minute, &second);

  *vdate = cdiEncodeDate(year, month, day);
  *vtime = cdiEncodeTime(hour, minute, second);
}

static double
vtime2timeval(int64_t vdate, int vtime, taxis_t *taxis)
{
  double value = 0;
  int64_t julday1, julday2, days;
  int secofday1, secofday2, secs;

  int timeunit = (*taxis).unit;
  const int calendar = (*taxis).calendar;

  int64_t rdate = (*taxis).rdate;
  int rtime = (*taxis).rtime;
  if (rdate == -1)
    {
      rdate = (*taxis).vdate;
      rtime = (*taxis).vtime;
    }

  if (rdate == 0 && rtime == 0 && vdate == 0 && vtime == 0) return value;

  int ryear, rmonth;
  int year, month, day, hour, minute, second;
  cdiDecodeDate(rdate, &ryear, &rmonth, &day);
  cdiDecodeTime(rtime, &hour, &minute, &second);

  encode_caldaysec(calendar, ryear, rmonth, day, hour, minute, second, &julday1, &secofday1);

  cdiDecodeDate(vdate, &year, &month, &day);
  cdiDecodeTime(vtime, &hour, &minute, &second);

  int timeunit0 = timeunit;

  if (timeunit == TUNIT_MONTH && calendar == CALENDAR_360DAYS)
    {
      timeunit = TUNIT_DAY;
    }

  if (timeunit == TUNIT_MONTH || timeunit == TUNIT_YEAR)
    {
      value = (year - ryear) * 12 - rmonth + month;

      int nmonth = (int) value;
      month -= nmonth;

      // clang-format off
      while (month > 12) { month -= 12; year++; }
      while (month <  1) { month += 12; year--; }
      // clang-format on

      int dpm = days_per_month(calendar, year, month);

      encode_caldaysec(calendar, year, month, day, hour, minute, second, &julday2, &secofday2);

      julday_sub(julday1, secofday1, julday2, secofday2, &days, &secs);

      value += (days + secs / 86400.) / dpm;

      if (timeunit == TUNIT_YEAR) value = value / 12;
    }
  else
    {
      encode_caldaysec(calendar, year, month, day, hour, minute, second, &julday2, &secofday2);

      julday_sub(julday1, secofday1, julday2, secofday2, &days, &secs);

      cdiEncodeTimevalue(days, secs, timeunit, &value);
    }

  if (timeunit0 == TUNIT_MONTH && calendar == CALENDAR_360DAYS) value /= 30;

  return value;
}

static void
conv_timeval(double timevalue, int64_t *rvdate, int *rvtime)
{
  const int64_t vdate = (int) timevalue;
  const double tmpval = (timevalue - vdate) * 86400.0;
  const int daysec = (vdate < 0) ? (int) (-tmpval + 0.01) : (int) (tmpval + 0.01);

  const int hour = daysec / 3600;
  const int minute = (daysec - hour * 3600) / 60;
  const int second = daysec - hour * 3600 - minute * 60;
  const int vtime = cdiEncodeTime(hour, minute, second);

  *rvdate = vdate;
  *rvtime = vtime;
}

static void
split_timevalue(double timevalue, int timeunit, int64_t *date, int *time)
{
  int64_t vdate = 0;
  int vtime = 0;

  if (timeunit == TUNIT_SECOND)
    {
      timevalue /= 86400;
      conv_timeval(timevalue, &vdate, &vtime);
    }
  else if (timeunit == TUNIT_HOUR)
    {
      timevalue /= 24;
      conv_timeval(timevalue, &vdate, &vtime);
    }
  else if (timeunit == TUNIT_DAY)
    {
      conv_timeval(timevalue, &vdate, &vtime);
    }
  else if (timeunit == TUNIT_MONTH)
    {
      vdate = (int64_t) timevalue * 100 - ((timevalue < 0) * 2 - 1);
      vtime = 0;
    }
  else if (timeunit == TUNIT_YEAR)
    {
      {
        static bool lwarn = true;
        if (lwarn && (fabs(timevalue - (int64_t) timevalue) > 0))
          {
            Warning("Fraction of a year is not supported!!");
            lwarn = false;
          }
      }

      {
        static bool lwarn = true;
        if (timevalue < -214700)
          {
            if (lwarn)
              {
                Warning("Year %g out of range, set to -214700", timevalue);
                lwarn = false;
              }
            timevalue = -214700;
          }
        else if (timevalue > 214700)
          {
            if (lwarn)
              {
                Warning("Year %g out of range, set to 214700", timevalue);
                lwarn = false;
              }
            timevalue = 214700;
          }
      }

      vdate = (int64_t) timevalue * 10000;
      vdate += timevalue < 0 ? -101 : 101;
      vtime = 0;
    }
  else
    {
      static bool lwarn = true;
      if (lwarn)
        {
          Warning("timeunit %s unsupported!", tunitNamePtr(timeunit));
          lwarn = false;
        }
    }

  /* verify date and time */

  int year, month, day;
  cdiDecodeDate(vdate, &year, &month, &day);
  int hour, minute, second;
  cdiDecodeTime(vtime, &hour, &minute, &second);

  if (month > 17 || day > 31 || hour > 23 || minute > 59 || second > 59)
    {
      if ((month > 17 || day > 31) && (year < -9999 || year > 9999)) year = 1;
      if (month > 17) month = 1;
      if (day > 31) day = 1;
      if (hour > 23) hour = 0;
      if (minute > 59) minute = 0;
      if (second > 59) second = 0;

      vdate = cdiEncodeDate(year, month, day);
      vtime = cdiEncodeTime(hour, minute, second);

      static bool lwarn = true;
      if (lwarn)
        {
          lwarn = false;
          Warning("Reset wrong date/time to %4.4d-%2.2d-%2.2d %2.2d:%2.2d:%2.2d!", year, month, day, hour, minute, second);
        }
    }

  *date = vdate;
  *time = vtime;
}

void
cdiSetForecastPeriod(double timevalue, taxis_t *taxis)
{
  int64_t julday, days;
  int secofday, secs;

  (*taxis).fc_period = timevalue;

  int timeunit = (*taxis).fc_unit;
  int calendar = (*taxis).calendar;

  int64_t vdate = (*taxis).vdate;
  int vtime = (*taxis).vtime;

  if (vdate == 0 && vtime == 0 && DBL_IS_EQUAL(timevalue, 0.)) return;

  int year, month, day, hour, minute, second;
  cdiDecodeDate(vdate, &year, &month, &day);
  cdiDecodeTime(vtime, &hour, &minute, &second);

  if (timeunit == TUNIT_MONTH && calendar == CALENDAR_360DAYS)
    {
      timeunit = TUNIT_DAY;
      timevalue *= 30;
    }

  if (timeunit == TUNIT_MONTH || timeunit == TUNIT_YEAR)
    {
      if (timeunit == TUNIT_YEAR) timevalue *= 12;

      const int nmon = (int) timevalue;
      const double fmon = timevalue - nmon;

      month -= nmon;

      // clang-format off
      while (month > 12) { month -= 12; year++; }
      while (month <  1) { month += 12; year--; }
      // clang-format on

      const int dpm = days_per_month(calendar, year, month);
      timeunit = TUNIT_DAY;
      timevalue = fmon * dpm;
    }

  encode_caldaysec(calendar, year, month, day, hour, minute, second, &julday, &secofday);

  cdi_decode_timevalue(timeunit, timevalue, &days, &secs);

  julday_add(-days, -secs, &julday, &secofday);

  decode_caldaysec(calendar, julday, secofday, &year, &month, &day, &hour, &minute, &second);

  (*taxis).fdate = cdiEncodeDate(year, month, day);
  (*taxis).ftime = cdiEncodeTime(hour, minute, second);
}

void
cdiDecodeTimeval(double timevalue, taxis_t *taxis, int64_t *date, int *time)
{
  if (taxis->type == TAXIS_ABSOLUTE)
    split_timevalue(timevalue, taxis->unit, date, time);
  else
    timeval2vtime(timevalue, taxis, date, time);
}

double
cdiEncodeTimeval(int64_t date, int time, taxis_t *taxis)
{
  double timevalue;

  if (taxis->type == TAXIS_ABSOLUTE)
    {
      if (taxis->unit == TUNIT_YEAR)
        {
          int year, month, day;
          cdiDecodeDate(date, &year, &month, &day);

          timevalue = year;
        }
      else if (taxis->unit == TUNIT_MONTH)
        {
          int year, month, day;
          cdiDecodeDate(date, &year, &month, &day);
          timevalue = date / 100 + copysign((double) (day != 0) * 0.5, (double) date);
        }
      else
        {
          int hour, minute, second;
          cdiDecodeTime(time, &hour, &minute, &second);
          timevalue = copysign(1.0, (double) date) * (fabs((double) date) + (hour * 3600 + minute * 60 + second) / 86400.);
        }
    }
  else
    timevalue = vtime2timeval(date, time, taxis);

  return timevalue;
}

void
ptaxisInit(taxis_t *taxisptr)
{
  taxisDefaultValue(taxisptr);
}

void
ptaxisCopy(taxis_t *dest, taxis_t *source)
{
  reshLock();

  // memcpy(dest, source, sizeof(taxis_t));
  dest->used = source->used;
  dest->datatype = source->datatype;
  dest->type = source->type;
  dest->sdate = source->sdate;
  dest->stime = source->stime;
  dest->vdate = source->vdate;
  dest->vtime = source->vtime;
  dest->rdate = source->rdate;
  dest->rtime = source->rtime;
  dest->fdate = source->fdate;
  dest->ftime = source->ftime;
  dest->calendar = source->calendar;
  dest->unit = source->unit;
  dest->numavg = source->numavg;
  dest->climatology = source->climatology;
  dest->has_bounds = source->has_bounds;
  dest->vdate_lb = source->vdate_lb;
  dest->vtime_lb = source->vtime_lb;
  dest->vdate_ub = source->vdate_ub;
  dest->vtime_ub = source->vtime_ub;
  dest->fc_unit = source->fc_unit;
  dest->fc_period = source->fc_period;

  dest->climatology = source->climatology;
  delete_refcount_string(dest->name);
  delete_refcount_string(dest->longname);
  delete_refcount_string(dest->units);
  dest->name = dup_refcount_string(source->name);
  dest->longname = dup_refcount_string(source->longname);
  dest->units = dup_refcount_string(source->units);
  if (dest->self != CDI_UNDEFID) reshSetStatus(dest->self, &taxisOps, RESH_DESYNC_IN_USE);

  reshUnlock();
}

static void
taxisPrintKernel(taxis_t *taxisptr, FILE *fp)
{
  DateType vdate_lb, vdate_ub;
  int vtime_lb, vtime_ub;

  taxisInqVdateBounds(taxisptr->self, &vdate_lb, &vdate_ub);
  taxisInqVtimeBounds(taxisptr->self, &vtime_lb, &vtime_ub);

  fprintf(fp,
          "#\n"
          "# taxisID %d\n"
          "#\n"
          "self        = %d\n"
          "used        = %d\n"
          "type        = %d\n"
          "vdate       = %lld\n"
          "vtime       = %d\n"
          "rdate       = %lld\n"
          "rtime       = %d\n"
          "fdate       = %lld\n"
          "ftime       = %d\n"
          "calendar    = %d\n"
          "unit        = %d\n"
          "numavg      = %d\n"
          "climatology = %d\n"
          "has_bounds  = %d\n"
          "vdate_lb    = %ld\n"
          "vtime_lb    = %d\n"
          "vdate_ub    = %ld\n"
          "vtime_ub    = %d\n"
          "fc_unit     = %d\n"
          "fc_period   = %g\n"
          "\n",
          taxisptr->self, taxisptr->self, (int) taxisptr->used, taxisptr->type, taxisptr->vdate, taxisptr->vtime, taxisptr->rdate,
          taxisptr->rtime, taxisptr->fdate, taxisptr->ftime, taxisptr->calendar, taxisptr->unit, taxisptr->numavg,
          (int) taxisptr->climatology, (int) taxisptr->has_bounds, (long) vdate_lb, vtime_lb, (long) vdate_ub, vtime_ub,
          taxisptr->fc_unit, taxisptr->fc_period);
}

static int
taxisCompareP(void *taxisptr1, void *taxisptr2)
{
  const taxis_t *t1 = (const taxis_t *) taxisptr1, *t2 = (const taxis_t *) taxisptr2;

  xassert(t1 && t2);

  return !(t1->used == t2->used && t1->type == t2->type && t1->vdate == t2->vdate && t1->vtime == t2->vtime
           && t1->rdate == t2->rdate && t1->rtime == t2->rtime && t1->fdate == t2->fdate && t1->ftime == t2->ftime
           && t1->calendar == t2->calendar && t1->unit == t2->unit && t1->fc_unit == t2->fc_unit && t1->numavg == t2->numavg
           && t1->climatology == t2->climatology && t1->has_bounds == t2->has_bounds && t1->vdate_lb == t2->vdate_lb
           && t1->vtime_lb == t2->vtime_lb && t1->vdate_ub == t2->vdate_ub && t1->vtime_ub == t2->vtime_ub);
}

static int
taxisTxCode(void)
{
  return TAXIS;
}

enum
{
  taxisNint = 22
};

static int
taxisGetPackSize(void *p, void *context)
{
  taxis_t *taxisptr = (taxis_t *) p;
  int packBufferSize = serializeGetSize(taxisNint, CDI_DATATYPE_INT, context) + serializeGetSize(1, CDI_DATATYPE_UINT32, context)
                       + (taxisptr->name ? serializeGetSize((int) strlen(taxisptr->name), CDI_DATATYPE_TXT, context) : 0)
                       + (taxisptr->longname ? serializeGetSize((int) strlen(taxisptr->longname), CDI_DATATYPE_TXT, context) : 0)
                       + (taxisptr->units ? serializeGetSize((int) strlen(taxisptr->units), CDI_DATATYPE_TXT, context) : 0);
  return packBufferSize;
}

int
taxisUnpack(char *unpackBuffer, int unpackBufferSize, int *unpackBufferPos, int originNamespace, void *context, int force_id)
{
  taxis_t *taxisP;
  int intBuffer[taxisNint];
  uint32_t d;
  int idx = 0;

  serializeUnpack(unpackBuffer, unpackBufferSize, unpackBufferPos, intBuffer, taxisNint, CDI_DATATYPE_INT, context);
  serializeUnpack(unpackBuffer, unpackBufferSize, unpackBufferPos, &d, 1, CDI_DATATYPE_UINT32, context);

  xassert(cdiCheckSum(CDI_DATATYPE_INT, taxisNint, intBuffer) == d);

  taxisInit();

  cdiResH targetID = namespaceAdaptKey(intBuffer[idx++], originNamespace);
  taxisP = taxisNewEntry(force_id ? targetID : CDI_UNDEFID);

  xassert(!force_id || targetID == taxisP->self);

  taxisP->used = (short) intBuffer[idx++];
  taxisP->type = intBuffer[idx++];
  taxisP->vdate = intBuffer[idx++];
  taxisP->vtime = intBuffer[idx++];
  taxisP->rdate = intBuffer[idx++];
  taxisP->rtime = intBuffer[idx++];
  taxisP->fdate = intBuffer[idx++];
  taxisP->ftime = intBuffer[idx++];
  taxisP->calendar = intBuffer[idx++];
  taxisP->unit = intBuffer[idx++];
  taxisP->fc_unit = intBuffer[idx++];
  taxisP->numavg = intBuffer[idx++];
  taxisP->climatology = intBuffer[idx++];
  taxisP->has_bounds = (short) intBuffer[idx++];
  taxisP->vdate_lb = intBuffer[idx++];
  taxisP->vtime_lb = intBuffer[idx++];
  taxisP->vdate_ub = intBuffer[idx++];
  taxisP->vtime_ub = intBuffer[idx++];

  if (intBuffer[idx])
    {
      int len = intBuffer[idx];
      char *name = new_refcount_string((size_t) len);
      serializeUnpack(unpackBuffer, unpackBufferSize, unpackBufferPos, name, len, CDI_DATATYPE_TXT, context);
      name[len] = '\0';
      taxisP->name = name;
    }
  idx++;
  if (intBuffer[idx])
    {
      int len = intBuffer[idx];
      char *longname = new_refcount_string((size_t) len);
      serializeUnpack(unpackBuffer, unpackBufferSize, unpackBufferPos, longname, len, CDI_DATATYPE_TXT, context);
      longname[len] = '\0';
      taxisP->longname = longname;
    }
  if (intBuffer[idx])
    {
      int len = intBuffer[idx];
      char *units = new_refcount_string((size_t) len);
      serializeUnpack(unpackBuffer, unpackBufferSize, unpackBufferPos, units, len, CDI_DATATYPE_TXT, context);
      units[len] = '\0';
      taxisP->units = units;
    }

  reshSetStatus(taxisP->self, &taxisOps, reshGetStatus(taxisP->self, &taxisOps) & ~RESH_SYNC_BIT);

  return taxisP->self;
}

static void
taxisPack(void *voidP, void *packBuffer, int packBufferSize, int *packBufferPos, void *context)
{
  taxis_t *taxisP = (taxis_t *) voidP;
  int intBuffer[taxisNint];
  uint32_t d;
  int idx = 0;

  intBuffer[idx++] = taxisP->self;
  intBuffer[idx++] = taxisP->used;
  intBuffer[idx++] = taxisP->type;
  intBuffer[idx++] = taxisP->vdate;
  intBuffer[idx++] = taxisP->vtime;
  intBuffer[idx++] = taxisP->rdate;
  intBuffer[idx++] = taxisP->rtime;
  intBuffer[idx++] = taxisP->fdate;
  intBuffer[idx++] = taxisP->ftime;
  intBuffer[idx++] = taxisP->calendar;
  intBuffer[idx++] = taxisP->unit;
  intBuffer[idx++] = taxisP->fc_unit;
  intBuffer[idx++] = taxisP->numavg;
  intBuffer[idx++] = taxisP->climatology;
  intBuffer[idx++] = taxisP->has_bounds;
  intBuffer[idx++] = taxisP->vdate_lb;
  intBuffer[idx++] = taxisP->vtime_lb;
  intBuffer[idx++] = taxisP->vdate_ub;
  intBuffer[idx++] = taxisP->vtime_ub;
  intBuffer[idx++] = taxisP->name ? (int) strlen(taxisP->name) : 0;
  intBuffer[idx++] = taxisP->longname ? (int) strlen(taxisP->longname) : 0;
  intBuffer[idx++] = taxisP->units ? (int) strlen(taxisP->units) : 0;

  serializePack(intBuffer, taxisNint, CDI_DATATYPE_INT, packBuffer, packBufferSize, packBufferPos, context);
  d = cdiCheckSum(CDI_DATATYPE_INT, taxisNint, intBuffer);
  serializePack(&d, 1, CDI_DATATYPE_UINT32, packBuffer, packBufferSize, packBufferPos, context);
  if (taxisP->name)
    serializePack(taxisP->name, intBuffer[15], CDI_DATATYPE_TXT, packBuffer, packBufferSize, packBufferPos, context);
  if (taxisP->longname)
    serializePack(taxisP->longname, intBuffer[16], CDI_DATATYPE_TXT, packBuffer, packBufferSize, packBufferPos, context);
  if (taxisP->units)
    serializePack(taxisP->units, intBuffer[16], CDI_DATATYPE_TXT, packBuffer, packBufferSize, packBufferPos, context);
}

/*
 * Local Variables:
 * c-file-style: "Java"
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * show-trailing-whitespace: t
 * require-trailing-newline: t
 * End:
 */
