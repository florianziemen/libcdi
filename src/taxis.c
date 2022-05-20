#include <stddef.h>

#include "cdi.h"
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
}static void
taxisDefaultValue(taxis_t *taxisptr)
{
  taxisptr->self = CDI_UNDEFID;
  taxisptr->used = false;
  taxisptr->datatype = CDI_DATATYPE_FLT64;
  taxisptr->type = DefaultTimeType;
  taxisptr->sdate = 0;
  taxisptr->stime = 0;
  cdiDateTime_init(&taxisptr->vdatetime);
  cdiDateTime_init(&taxisptr->rdatetime);
  taxisptr->fdate = CDI_UNDEFID;
  taxisptr->ftime = 0;
  taxisptr->calendar = CDI_Default_Calendar;
  taxisptr->unit = DefaultTimeUnit;
  taxisptr->numavg = 0;
  taxisptr->climatology = false;
  taxisptr->has_bounds = false;
  cdiDateTime_init(&taxisptr->vdatetime_lb);
  cdiDateTime_init(&taxisptr->vdatetime_ub);
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

@Prototype void taxisDefVdate(int taxisID, int vdate)
@Parameter
    @Item  taxisID  Time axis ID, from a previous call to @fref{taxisCreate}
    @Item  vdate    Verification date (YYYYMMDD)

@Description
The function @func{taxisDefVdate} defines the verification date of a Time axis.

@EndFunction
*/
void
taxisDefVdate(int taxisID, int vdate)
{
  taxis_t *taxisptr = (taxis_t *) reshGetVal(taxisID, &taxisOps);

  if ((int)cdiDate_get(taxisptr->vdatetime.date) != vdate)
    {
      taxisptr->vdatetime.date = cdiDate_set(vdate);
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

  if (cdiTime_get(taxisptr->vdatetime.time) != vtime)
    {
      taxisptr->vdatetime.time = cdiTime_set(vtime);
      reshSetStatus(taxisID, &taxisOps, RESH_DESYNC_IN_USE);
    }
}

void
taxisDefVdatetime(int taxisID, CdiDateTime vdatetime)
{
  taxis_t *taxisptr = (taxis_t *) reshGetVal(taxisID, &taxisOps);

  if (cdiDateTime_isNE(taxisptr->vdatetime, vdatetime))
    {
      taxisptr->vdatetime = vdatetime;
      reshSetStatus(taxisID, &taxisOps, RESH_DESYNC_IN_USE);
    }
}

void
taxisDefRdatetime(int taxisID, CdiDateTime rdatetime)
{
  taxis_t *taxisptr = (taxis_t *) reshGetVal(taxisID, &taxisOps);

  if (cdiDateTime_isNE(taxisptr->rdatetime, rdatetime))
    {
      taxisptr->rdatetime = rdatetime;

      if (taxisptr->units)
        {
          delete_refcount_string(taxisptr->units);
          taxisptr->units = NULL;
        }
      reshSetStatus(taxisID, &taxisOps, RESH_DESYNC_IN_USE);
    }
}

/*
@Function  taxisDefRdate
@Title     Define the reference date

@Prototype void taxisDefRdate(int taxisID, int rdate)
@Parameter
    @Item  taxisID  Time axis ID, from a previous call to @fref{taxisCreate}
    @Item  rdate    Reference date (YYYYMMDD)

@Description
The function @func{taxisDefRdate} defines the reference date of a Time axis.

@EndFunction
*/
void
taxisDefRdate(int taxisID, int rdate)
{
  taxis_t *taxisptr = (taxis_t *) reshGetVal(taxisID, &taxisOps);

  if ((int)cdiDate_get(taxisptr->rdatetime.date) != rdate)
    {
      taxisptr->rdatetime.date = cdiDate_set(rdate);

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

  if (cdiTime_get(taxisptr->rdatetime.time) != rtime)
    {
      taxisptr->rdatetime.time = cdiTime_set(rtime);
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

@Prototype void taxisDefFdate(int taxisID, int fdate)
@Parameter
    @Item  taxisID  Time axis ID, from a previous call to @fref{taxisCreate}
    @Item  fdate    Forecast reference date (YYYYMMDD)

@Description
The function @func{taxisDefFdate} defines the forecast reference date of a Time axis.

@EndFunction
*/
void
taxisDefFdate(int taxisID, int fdate)
{
  taxis_t *taxisptr = (taxis_t *) reshGetVal(taxisID, &taxisOps);

  if (taxisptr->fdate != fdate)
    {
      taxisptr->fdate = fdate;
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

  // reference date/time and units can't be changed after streamDefVlist()!

  taxisptr2->sdate = taxisptr1->sdate;
  taxisptr2->stime = taxisptr1->stime;

  taxisptr2->vdatetime = taxisptr1->vdatetime;

  if (taxisptr2->has_bounds)
    {
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

CdiDateTime
taxisInqVdatetime(int taxisID)
{
  const taxis_t *taxisptr = (taxis_t *) reshGetVal(taxisID, &taxisOps);
  return taxisptr->vdatetime;
}

CdiDateTime
taxisInqRdatetime(int taxisID)
{
  taxis_t *taxisptr = (taxis_t *) reshGetVal(taxisID, &taxisOps);

  if (cdiDateTime_isNull(taxisptr->rdatetime))
    {
      taxisptr->rdatetime = taxisptr->vdatetime;
      reshSetStatus(taxisID, &taxisOps, RESH_DESYNC_IN_USE);
    }

  return taxisptr->rdatetime;
}


/*
@Function  taxisInqVdate
@Title     Get the verification date

@Prototype int taxisInqVdate(int taxisID)
@Parameter
    @Item  taxisID  Time axis ID, from a previous call to @fref{taxisCreate} or @fref{vlistInqTaxis}

@Description
The function @func{taxisInqVdate} returns the verification date of a Time axis.

@Result
@func{taxisInqVdate} returns the verification date.

@EndFunction
*/
int
taxisInqVdate(int taxisID)
{
  const taxis_t *taxisptr = (taxis_t *) reshGetVal(taxisID, &taxisOps);
  return (int)cdiDate_get(taxisptr->vdatetime.date);
}

int
taxisInqSdate(int taxisID)
{
  const taxis_t *taxisptr = (taxis_t *) reshGetVal(taxisID, &taxisOps);
  return taxisptr->sdate;
}

void
taxisInqVdateBounds(int taxisID, int *vdate_lb, int *vdate_ub)
{
  const taxis_t *taxisptr = (taxis_t *) reshGetVal(taxisID, &taxisOps);
  *vdate_lb = (int)cdiDate_get(taxisptr->vdatetime_lb.date);
  *vdate_ub = (int)cdiDate_get(taxisptr->vdatetime_ub.date);
}

void
taxisInqVdatetimeBounds(int taxisID, CdiDateTime *vdatetime_lb, CdiDateTime *vdatetime_ub)
{
  const taxis_t *taxisptr = (taxis_t *) reshGetVal(taxisID, &taxisOps);
  *vdatetime_lb = taxisptr->vdatetime_lb;
  *vdatetime_ub = taxisptr->vdatetime_ub;
}

void
taxisDefVdateBounds(int taxisID, int vdate_lb, int vdate_ub)
{
  taxis_t *taxisptr = (taxis_t *) reshGetVal(taxisID, &taxisOps);

  if (taxisptr->has_bounds == false || (int)cdiDate_get(taxisptr->vdatetime_lb.date) != vdate_lb || (int)cdiDate_get(taxisptr->vdatetime_ub.date) != vdate_ub)
    {
      taxisptr->vdatetime_lb.date = cdiDate_set(vdate_lb);
      taxisptr->vdatetime_ub.date = cdiDate_set(vdate_ub);
      taxisptr->has_bounds = true;
      reshSetStatus(taxisID, &taxisOps, RESH_DESYNC_IN_USE);
    }
}

void
taxisDefVdatetimeBounds(int taxisID, CdiDateTime vdatetime_lb, CdiDateTime vdatetime_ub)
{
  taxis_t *taxisptr = (taxis_t *) reshGetVal(taxisID, &taxisOps);

  if (taxisptr->has_bounds == false || cdiDateTime_isNE(taxisptr->vdatetime_lb, vdatetime_lb) || cdiDateTime_isNE(taxisptr->vdatetime_ub, vdatetime_ub))
    {
      taxisptr->vdatetime_lb = vdatetime_lb;
      taxisptr->vdatetime_ub = vdatetime_ub;
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
  //return taxisptr->vtime;
  return cdiTime_get(taxisptr->vdatetime.time);
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
  *vtime_lb = cdiTime_get(taxisptr->vdatetime_lb.time);
  *vtime_ub = cdiTime_get(taxisptr->vdatetime_ub.time);
}

void
taxisDefVtimeBounds(int taxisID, int vtime_lb, int vtime_ub)
{
  taxis_t *taxisptr = (taxis_t *) reshGetVal(taxisID, &taxisOps);

  if (taxisptr->has_bounds == false || cdiTime_get(taxisptr->vdatetime_lb.time) != vtime_lb || cdiTime_get(taxisptr->vdatetime_ub.time) != vtime_ub)
    {
      taxisptr->vdatetime_lb.time = cdiTime_set(vtime_lb);
      taxisptr->vdatetime_ub.time = cdiTime_set(vtime_ub);
      taxisptr->has_bounds = true;
      reshSetStatus(taxisID, &taxisOps, RESH_DESYNC_IN_USE);
    }
}

/*
@Function  taxisInqRdate
@Title     Get the reference date

@Prototype int taxisInqRdate(int taxisID)
@Parameter
    @Item  taxisID  Time axis ID, from a previous call to @fref{taxisCreate} or @fref{vlistInqTaxis}

@Description
The function @func{taxisInqRdate} returns the reference date of a Time axis.

@Result
@func{taxisInqRdate} returns the reference date.

@EndFunction
*/
int
taxisInqRdate(int taxisID)
{
  taxis_t *taxisptr = (taxis_t *) reshGetVal(taxisID, &taxisOps);

  if (cdiDateTime_isNull(taxisptr->rdatetime))
    {
      taxisptr->rdatetime = taxisptr->vdatetime;
      reshSetStatus(taxisID, &taxisOps, RESH_DESYNC_IN_USE);
    }

  return (int)cdiDate_get(taxisptr->rdatetime.date);
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

  if (cdiDateTime_isNull(taxisptr->rdatetime))
    {
      taxisptr->rdatetime = taxisptr->vdatetime;
      reshSetStatus(taxisID, &taxisOps, RESH_DESYNC_IN_USE);
    }

  return cdiTime_get(taxisptr->rdatetime.time);
}

/*
@Function  taxisInqFdate
@Title     Get the forecast reference date

@Prototype int taxisInqFdate(int taxisID)
@Parameter
    @Item  taxisID  Time axis ID, from a previous call to @fref{taxisCreate} or @fref{vlistInqTaxis}

@Description
The function @func{taxisInqFdate} returns the forecast reference date of a Time axis.

@Result
@func{taxisInqFdate} returns the forecast reference date.

@EndFunction
*/
int
taxisInqFdate(int taxisID)
{
  taxis_t *taxisptr = (taxis_t *) reshGetVal(taxisID, &taxisOps);

  if (taxisptr->fdate == -1)
    {
      // rdatetime is initialized from vdatetime if empty!
      taxisptr->fdate = taxisInqRdate(taxisID);
      taxisptr->ftime = taxisInqRtime(taxisID);
    }

  return taxisptr->fdate;
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
      // rdatetime is initialized from vdatetime if empty!
      taxisptr->fdate = taxisInqRdate(taxisID);
      taxisptr->ftime = taxisInqRtime(taxisID);
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

static JulianDate
timevalue_decode(int timeunits, double timevalue)
{
  JulianDate julianDate;
  julianDate.julianDay = 0;
  julianDate.secondOfDay = 0.0;

  if (timeunits == TUNIT_MINUTE)
    {
      timevalue *= 60;
      timeunits = TUNIT_SECOND;
    }
  else if (timeunits == TUNIT_HOUR)
    {
      timevalue /= 24;
      timeunits = TUNIT_DAY;
    }

  if (timeunits == TUNIT_SECOND)
    {
      julianDate.julianDay = (int64_t) (timevalue / 86400.0);
      const double seconds = timevalue - julianDate.julianDay * 86400.0;
      julianDate.secondOfDay = round(seconds * 1000.0) / 1000.0;
      if (julianDate.secondOfDay < 0)
        {
          julianDate.julianDay -= 1;
          julianDate.secondOfDay += 86400.0;
        };
      /*
      {
        double cval = julianDate.julianDay * 86400.0 + julianDate.secondOfDay;
        if (cval != timevalue) printf("TUNIT_SECOND error: %g %g %d %d\n", timevalue, cval, julianDate.julianDay, julianDate.secondOfDay);
      }
      */
    }
  else if (timeunits == TUNIT_DAY)
    {
      julianDate.julianDay = (int64_t) timevalue;
      const double seconds = (timevalue - julianDate.julianDay) * 86400.0;
      julianDate.secondOfDay = (int) lround(seconds);
      if (julianDate.secondOfDay < 0)
        {
          julianDate.julianDay -= 1;
          julianDate.secondOfDay += 86400.0;
        };
      /*
      {
        double cval = julianDate.julianDay + julianDate.secondOfDay / 86400.0;
        if (cval != timevalue) printf("TUNIT_DAY error: %g %g %d %d\n", timevalue, cval, julianDate.julianDay, julianDate.secondOfDay);
      }
      */
    }
  else
    {
      static bool lwarn = true;
      if (lwarn)
        {
          Warning("timeunit %s unsupported!", tunitNamePtr(timeunits));
          lwarn = false;
        }
    }

  return julianDate;
}

static double
cdi_encode_timevalue(int days, double secs, int timeunit)
{
  double timevalue = 0.0;

  if (timeunit == TUNIT_SECOND)
    {
      timevalue = days * 86400.0 + secs;
    }
  else if (timeunit == TUNIT_MINUTE || timeunit == TUNIT_QUARTER || timeunit == TUNIT_30MINUTES)
    {
      timevalue = days * 1440. + secs / 60.;
    }
  else if (timeunit == TUNIT_HOUR || timeunit == TUNIT_3HOURS || timeunit == TUNIT_6HOURS || timeunit == TUNIT_12HOURS)
    {
      timevalue = days * 24. + secs / 3600.;
    }
  else if (timeunit == TUNIT_DAY)
    {
      timevalue = days + secs / 86400.;
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

  return timevalue;
}

// convert relative time value to CdiDateTime
static CdiDateTime
rtimeval2datetime(double timevalue, taxis_t *taxis)
{
  if (IS_EQUAL(timevalue, 0.0)) return taxis->rdatetime;

  int timeunits = taxis->unit;
  const int calendar = taxis->calendar;

  if (timeunits == TUNIT_MONTH && calendar == CALENDAR_360DAYS)
    {
      timeunits = TUNIT_DAY;
      timevalue *= 30;
    }

  CdiDateTime rdatetime = taxis->rdatetime;

  if (timeunits == TUNIT_MONTH || timeunits == TUNIT_YEAR)
    {
      int year = rdatetime.date.year;
      int month = rdatetime.date.month;

      if (timeunits == TUNIT_YEAR) timevalue *= 12;

      const int nmon = (int) timevalue;
      const double fmon = timevalue - nmon;

      month += nmon;

      // clang-format off
      while (month > 12) { month -= 12; year++; }
      while (month <  1) { month += 12; year--; }
      // clang-format on

      timeunits = TUNIT_DAY;
      timevalue = fmon * days_per_month(calendar, year, month);

      rdatetime.date.year = year;
      rdatetime.date.month = month;
    }

  const JulianDate julianDate = julianDate_encode(calendar, rdatetime);
  const JulianDate julianDate2 = timevalue_decode(timeunits, timevalue);

  return julianDate_decode(calendar, julianDate_add(julianDate2, julianDate));
}

// convert CdiDateTime to relative time value
static double
datetime2rtimeval(CdiDateTime vdatetime, taxis_t *taxis)
{
  double value = 0.0;

  const int calendar = (*taxis).calendar;
  int timeunits = (*taxis).unit;
  const int timeunits0 = timeunits;

  CdiDateTime rdatetime = (*taxis).rdatetime;

  if (cdiDateTime_isNull(rdatetime)) rdatetime = (*taxis).vdatetime;

  if (cdiDateTime_isNull(rdatetime) && cdiDateTime_isNull(vdatetime)) return value;

  const JulianDate julianDate1 = julianDate_encode(calendar, rdatetime);

  if (timeunits == TUNIT_MONTH && calendar == CALENDAR_360DAYS) timeunits = TUNIT_DAY;

  if (timeunits == TUNIT_MONTH || timeunits == TUNIT_YEAR)
    {
      const int ryear = rdatetime.date.year;
      const int rmonth = rdatetime.date.month;
      int year = vdatetime.date.year;
      int month = vdatetime.date.month;
      value = (year - ryear) * 12 - rmonth + month;

      int nmonth = (int) value;
      month -= nmonth;

      // clang-format off
      while (month > 12) { month -= 12; year++; }
      while (month <  1) { month += 12; year--; }
      // clang-format on

      const int dpm = days_per_month(calendar, year, month);

      vdatetime.date.year = year;
      vdatetime.date.month = month;
      const JulianDate julianDate2 = julianDate_encode(calendar, vdatetime);
      const JulianDate dateDifference = julianDate_sub(julianDate2, julianDate1);

      value += (dateDifference.julianDay + dateDifference.secondOfDay / 86400.0) / dpm;
      if (timeunits == TUNIT_YEAR) value = value / 12;
    }
  else
    {
      const JulianDate julianDate2 = julianDate_encode(calendar, vdatetime);
      const JulianDate dateDifference = julianDate_sub(julianDate2, julianDate1);

      value = cdi_encode_timevalue(dateDifference.julianDay, dateDifference.secondOfDay, timeunits);
    }

  if (timeunits0 == TUNIT_MONTH && calendar == CALENDAR_360DAYS) value /= 30.0;

  return value;
}

// convert absolute time value to CdiDateTime
static CdiDateTime
atimeval2datetime(double timevalue)
{
  const int64_t vdate = (int64_t) timevalue;
  const double tmpval = (timevalue - vdate) * 86400.0;
  const int daysec = (vdate < 0) ? (int) (-tmpval + 0.01) : (int) (tmpval + 0.01);

  int year, month, day;
  cdiDecodeDate(vdate, &year, &month, &day);

  const int hour = daysec / 3600;
  const int minute = (daysec - hour * 3600) / 60;
  const int second = daysec - hour * 3600 - minute * 60;
  const int ms = 0;

  CdiDateTime datetime;
  datetime.date = cdiDate_encode(year, month, day);
  datetime.time = cdiTime_encode(hour, minute, second, ms);

  return datetime;
}

static CdiDateTime
split_timevalue(double timevalue, int timeunit)
{
  CdiDateTime datetime;
  cdiDateTime_init(&datetime);

  if (timeunit == TUNIT_SECOND)
    {
      timevalue /= 86400;
      datetime = atimeval2datetime(timevalue);
    }
  else if (timeunit == TUNIT_HOUR)
    {
      timevalue /= 24;
      datetime = atimeval2datetime(timevalue);
    }
  else if (timeunit == TUNIT_DAY)
    {
      datetime = atimeval2datetime(timevalue);
    }
  else if (timeunit == TUNIT_MONTH)
    {
      int64_t vdate = (int64_t) timevalue * 100 - ((timevalue < 0) * 2 - 1);
      datetime.date = cdiDate_set(vdate);
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

      int64_t vdate = (int64_t) timevalue * 10000;
      vdate += (timevalue < 0) ? -101 : 101;
      datetime.date = cdiDate_set(vdate);
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

  // verify date and time

  int year, month, day;
  cdiDate_decode(datetime.date, &year, &month, &day);
  int hour, minute, second, ms;
  cdiTime_decode(datetime.time, &hour, &minute, &second, &ms);

  if (month > 17 || day > 31 || hour > 23 || minute > 59 || second > 59)
    {
      if ((month > 17 || day > 31) && (year < -9999 || year > 9999)) year = 1;
      if (month > 17) month = 1;
      if (day > 31) day = 1;
      if (hour > 23) hour = 0;
      if (minute > 59) minute = 0;
      if (second > 59) second = 0;

      datetime.date = cdiDate_encode(year, month, day);
      datetime.time = cdiTime_encode(hour, minute, second, ms);

      static bool lwarn = true;
      if (lwarn)
        {
          lwarn = false;
          Warning("Reset wrong date/time to %4.4d-%2.2d-%2.2d %2.2d:%2.2d:%2.2d!", year, month, day, hour, minute, second);
        }
    }

  return datetime;
}

void
cdiSetForecastPeriod(double timevalue, taxis_t *taxis)
{
  taxis->fc_period = timevalue;

  int timeunits = taxis->fc_unit;
  const int calendar = taxis->calendar;

  if (cdiDateTime_isNull(taxis->vdatetime) && DBL_IS_EQUAL(timevalue, 0.0)) return;

  if (timeunits == TUNIT_MONTH && calendar == CALENDAR_360DAYS)
    {
      timeunits = TUNIT_DAY;
      timevalue *= 30;
    }

  CdiDateTime vdatetime = taxis->vdatetime;

  if (timeunits == TUNIT_MONTH || timeunits == TUNIT_YEAR)
    {
      int year = vdatetime.date.year;
      int month = vdatetime.date.month;

      if (timeunits == TUNIT_YEAR) timevalue *= 12;

      const int nmon = (int) timevalue;
      const double fmon = timevalue - nmon;

      month -= nmon;

      // clang-format off
      while (month > 12) { month -= 12; year++; }
      while (month <  1) { month += 12; year--; }
      // clang-format on

      timeunits = TUNIT_DAY;
      timevalue = fmon * days_per_month(calendar, year, month);

      vdatetime.date.year = year;
      vdatetime.date.month = month;
    }

  const JulianDate julianDate = julianDate_encode(calendar, vdatetime);
  const JulianDate julianDate2 = timevalue_decode(timeunits, timevalue);

  const CdiDateTime dt = julianDate_decode(calendar, julianDate_sub(julianDate, julianDate2));

  (*taxis).fdate = (int)cdiDate_get(dt.date);
  (*taxis).ftime = (int)cdiTime_get(dt.time);
}

CdiDateTime
cdi_decode_timeval(double timevalue, taxis_t *taxis)
{
  return (taxis->type == TAXIS_ABSOLUTE) ? split_timevalue(timevalue, taxis->unit) : rtimeval2datetime(timevalue, taxis);
}

double
cdi_encode_timeval(CdiDateTime datetime, taxis_t *taxis)
{
  double timevalue;

  if (taxis->type == TAXIS_ABSOLUTE)
    {
      if (taxis->unit == TUNIT_YEAR)
        {
          timevalue = datetime.date.year;
        }
      else if (taxis->unit == TUNIT_MONTH)
        {
          int64_t xdate = cdiDate_get(datetime.date);
          timevalue = xdate / 100 + copysign((double) (datetime.date.day != 0) * 0.5, (double) xdate);
        }
      else
        {
          int hour, minute, second, ms;
          cdiTime_decode(datetime.time, &hour, &minute, &second, &ms);
          int64_t xdate = cdiDate_get(datetime.date);
          timevalue = copysign(1.0, (double) xdate) * (fabs((double) xdate) + (hour * 3600 + minute * 60 + second) / 86400.0);
        }
    }
  else
    timevalue = datetime2rtimeval(datetime, taxis);

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
  dest->vdatetime = source->vdatetime;
  dest->rdatetime = source->rdatetime;
  dest->fdate = source->fdate;
  dest->ftime = source->ftime;
  dest->calendar = source->calendar;
  dest->unit = source->unit;
  dest->numavg = source->numavg;
  dest->climatology = source->climatology;
  dest->has_bounds = source->has_bounds;
  dest->vdatetime_lb = source->vdatetime_lb;
  dest->vdatetime_ub = source->vdatetime_ub;
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
  int vdate_lb, vdate_ub;
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
          "vdate       = %d\n"
          "vtime       = %d\n"
          "rdate       = %d\n"
          "rtime       = %d\n"
          "fdate       = %d\n"
          "ftime       = %d\n"
          "calendar    = %d\n"
          "unit        = %d\n"
          "numavg      = %d\n"
          "climatology = %d\n"
          "has_bounds  = %d\n"
          "vdate_lb    = %d\n"
          "vtime_lb    = %d\n"
          "vdate_ub    = %d\n"
          "vtime_ub    = %d\n"
          "fc_unit     = %d\n"
          "fc_period   = %g\n"
          "\n",
          taxisptr->self, taxisptr->self, (int) taxisptr->used, taxisptr->type,
          (int)cdiDate_get(taxisptr->vdatetime.date), cdiTime_get(taxisptr->vdatetime.time),
          (int)cdiDate_get(taxisptr->rdatetime.date), cdiTime_get(taxisptr->rdatetime.time),
          taxisptr->fdate, taxisptr->ftime, taxisptr->calendar, taxisptr->unit, taxisptr->numavg,
          (int) taxisptr->climatology, (int) taxisptr->has_bounds, vdate_lb, vtime_lb, vdate_ub, vtime_ub,
          taxisptr->fc_unit, taxisptr->fc_period);
}

static int
taxisCompareP(void *taxisptr1, void *taxisptr2)
{
  const taxis_t *t1 = (const taxis_t *) taxisptr1, *t2 = (const taxis_t *) taxisptr2;

  xassert(t1 && t2);

  return !(t1->used == t2->used && t1->type == t2->type && cdiDateTime_isEQ(t1->vdatetime,  t2->vdatetime)
           && cdiDateTime_isEQ(t1->rdatetime, t2->rdatetime) && t1->fdate == t2->fdate && t1->ftime == t2->ftime
           && t1->calendar == t2->calendar && t1->unit == t2->unit && t1->fc_unit == t2->fc_unit && t1->numavg == t2->numavg
           && t1->climatology == t2->climatology && t1->has_bounds == t2->has_bounds
           && cdiDateTime_isEQ(t1->vdatetime_lb, t2->vdatetime_lb) && cdiDateTime_isEQ(t1->vdatetime_ub, t2->vdatetime_ub));
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
  taxisP->vdatetime.date = cdiDate_set(intBuffer[idx++]);
  taxisP->vdatetime.time = cdiTime_set(intBuffer[idx++]);
  taxisP->rdatetime.date = cdiDate_set(intBuffer[idx++]);
  taxisP->rdatetime.time = cdiTime_set(intBuffer[idx++]);
  taxisP->fdate = intBuffer[idx++];
  taxisP->ftime = intBuffer[idx++];
  taxisP->calendar = intBuffer[idx++];
  taxisP->unit = intBuffer[idx++];
  taxisP->fc_unit = intBuffer[idx++];
  taxisP->numavg = intBuffer[idx++];
  taxisP->climatology = intBuffer[idx++];
  taxisP->has_bounds = (short) intBuffer[idx++];
  taxisP->vdatetime_lb.date = cdiDate_set(intBuffer[idx++]);
  taxisP->vdatetime_lb.time = cdiTime_set(intBuffer[idx++]);
  taxisP->vdatetime_ub.date = cdiDate_set(intBuffer[idx++]);
  taxisP->vdatetime_ub.time = cdiTime_set(intBuffer[idx++]);

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

  int idx = 0;
  intBuffer[idx++] = taxisP->self;
  intBuffer[idx++] = taxisP->used;
  intBuffer[idx++] = taxisP->type;
  intBuffer[idx++] = (int)cdiDate_get(taxisP->vdatetime.date);
  intBuffer[idx++] = cdiTime_get(taxisP->vdatetime.time);
  intBuffer[idx++] = (int)cdiDate_get(taxisP->rdatetime.date);
  intBuffer[idx++] = cdiTime_get(taxisP->rdatetime.time);
  intBuffer[idx++] = taxisP->fdate;
  intBuffer[idx++] = taxisP->ftime;
  intBuffer[idx++] = taxisP->calendar;
  intBuffer[idx++] = taxisP->unit;
  intBuffer[idx++] = taxisP->fc_unit;
  intBuffer[idx++] = taxisP->numavg;
  intBuffer[idx++] = taxisP->climatology;
  intBuffer[idx++] = taxisP->has_bounds;
  intBuffer[idx++] = (int)cdiDate_get(taxisP->vdatetime_lb.date);
  intBuffer[idx++] = cdiTime_get(taxisP->vdatetime_lb.time);
  intBuffer[idx++] = (int)cdiDate_get(taxisP->vdatetime_ub.date);
  intBuffer[idx++] = cdiTime_get(taxisP->vdatetime_ub.time);
  intBuffer[idx++] = taxisP->name ? (int) strlen(taxisP->name) : 0;
  intBuffer[idx++] = taxisP->longname ? (int) strlen(taxisP->longname) : 0;
  intBuffer[idx++] = taxisP->units ? (int) strlen(taxisP->units) : 0;

  serializePack(intBuffer, taxisNint, CDI_DATATYPE_INT, packBuffer, packBufferSize, packBufferPos, context);
  uint32_t d = cdiCheckSum(CDI_DATATYPE_INT, taxisNint, intBuffer);
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
