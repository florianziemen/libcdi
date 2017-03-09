#include <stddef.h>
#include <stdio.h>

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

static const char Timeunits[][TAXIS_MAX_UNIT_STR_LEN + 1] = {
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
  enum
  {
    size = sizeof(Timeunits) / sizeof(*Timeunits)
  };
  return (unitID > 0 && unitID < size) ? Timeunits[unitID] : Timeunits[0];
}
static void
taxisDefaultValue(taxis_t *taxisptr)
{
  taxisptr->self = CDI_UNDEFID;
  taxisptr->used = false;
  taxisptr->datatype = CDI_DATATYPE_FLT64;
  taxisptr->type = DefaultTimeType;
  taxisptr->calendar = CDI_Default_Calendar;
  taxisptr->unit = DefaultTimeUnit;
  taxisptr->numavg = 0;
  taxisptr->climatology = false;
  taxisptr->has_bounds = false;
  cdiDateTime_init(&taxisptr->sDateTime);
  cdiDateTime_init(&taxisptr->vDateTime);
  cdiDateTime_init(&taxisptr->rDateTime);
  cdiDateTime_init(&taxisptr->fDateTime);
  cdiDateTime_init(&taxisptr->vDateTime_lb);
  cdiDateTime_init(&taxisptr->vDateTime_ub);
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
      delete_refcount_string(taxisptr->units);
      taxisptr->units = NULL;
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

  if ((int) cdiDate_get(taxisptr->vDateTime.date) != vdate)
    {
      taxisptr->vDateTime.date = cdiDate_set(vdate);
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

  if (cdiTime_get(taxisptr->vDateTime.time) != vtime)
    {
      taxisptr->vDateTime.time = cdiTime_set(vtime);
      reshSetStatus(taxisID, &taxisOps, RESH_DESYNC_IN_USE);
    }
}

void
taxisDefVdatetime(int taxisID, CdiDateTime vDateTime)
{
  taxis_t *taxisptr = (taxis_t *) reshGetVal(taxisID, &taxisOps);

  if (cdiDateTime_isNE(taxisptr->vDateTime, vDateTime))
    {
      taxisptr->vDateTime = vDateTime;
      reshSetStatus(taxisID, &taxisOps, RESH_DESYNC_IN_USE);
    }
}

void
taxisDefRdatetime(int taxisID, CdiDateTime rDateTime)
{
  taxis_t *taxisptr = (taxis_t *) reshGetVal(taxisID, &taxisOps);

  if (cdiDateTime_isNE(taxisptr->rDateTime, rDateTime))
    {
      taxisptr->rDateTime = rDateTime;
      delete_refcount_string(taxisptr->units);
      taxisptr->units = NULL;
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

  if ((int) cdiDate_get(taxisptr->rDateTime.date) != rdate)
    {
      taxisptr->rDateTime.date = cdiDate_set(rdate);
      delete_refcount_string(taxisptr->units);
      taxisptr->units = NULL;
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

  if (cdiTime_get(taxisptr->rDateTime.time) != rtime)
    {
      taxisptr->rDateTime.time = cdiTime_set(rtime);
      if (taxisptr->units)
        {
          delete_refcount_string(taxisptr->units);
          taxisptr->units = NULL;
        }
      reshSetStatus(taxisID, &taxisOps, RESH_DESYNC_IN_USE);
    }
}

/*
@Function  taxisDefFdatetime
@Title     Define the forecast reference date/time

@Prototype void taxisDefFdatetime(int taxisID, CdiDateTime fDateTime)
@Parameter
    @Item  taxisID    Time axis ID, from a previous call to @fref{taxisCreate}
    @Item  fDateTime  Forecast reference date/time

@Description
The function @func{taxisDefFdatetime} defines the forecast reference date/time of a Time axis.

@EndFunction
*/
void
taxisDefFdatetime(int taxisID, CdiDateTime fDateTime)
{
  taxis_t *taxisptr = (taxis_t *) reshGetVal(taxisID, &taxisOps);

  if (cdiDateTime_isNE(taxisptr->fDateTime, fDateTime))
    {
      taxisptr->fDateTime = fDateTime;
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
      delete_refcount_string(taxisptr->units);
      taxisptr->units = NULL;
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

  taxisptr2->sDateTime = taxisptr1->sDateTime;
  taxisptr2->vDateTime = taxisptr1->vDateTime;
  taxisptr2->fDateTime = taxisptr1->fDateTime;

  if (taxisptr2->has_bounds)
    {
      taxisptr2->vDateTime_lb = taxisptr1->vDateTime_lb;
      taxisptr2->vDateTime_ub = taxisptr1->vDateTime_ub;
    }

  taxisptr2->fc_unit = taxisptr1->fc_unit;
  taxisptr2->fc_period = taxisptr1->fc_period;

  reshSetStatus(taxisID2, &taxisOps, RESH_DESYNC_IN_USE);
  reshUnlock();
}

CdiDateTime
taxisInqVdatetime(int taxisID)
{
  const taxis_t *taxisptr = (taxis_t *) reshGetVal(taxisID, &taxisOps);
  return taxisptr->vDateTime;
}

CdiDateTime
taxisInqRdatetime(int taxisID)
{
  taxis_t *taxisptr = (taxis_t *) reshGetVal(taxisID, &taxisOps);

  if (cdiDateTime_isNull(taxisptr->rDateTime))
    {
      taxisptr->rDateTime = taxisptr->vDateTime;
      reshSetStatus(taxisID, &taxisOps, RESH_DESYNC_IN_USE);
    }

  return taxisptr->rDateTime;
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
  return (int) cdiDate_get(taxisptr->vDateTime.date);
}

CdiDateTime
taxisInqSdatetime(int taxisID)
{
  const taxis_t *taxisptr = (taxis_t *) reshGetVal(taxisID, &taxisOps);
  return taxisptr->sDateTime;
}

void
taxisInqVdateBounds(int taxisID, int *vdate_lb, int *vdate_ub)
{
  const taxis_t *taxisptr = (taxis_t *) reshGetVal(taxisID, &taxisOps);
  *vdate_lb = (int) cdiDate_get(taxisptr->vDateTime_lb.date);
  *vdate_ub = (int) cdiDate_get(taxisptr->vDateTime_ub.date);
}

void
taxisInqVdatetimeBounds(int taxisID, CdiDateTime *vDateTime_lb, CdiDateTime *vDateTime_ub)
{
  const taxis_t *taxisptr = (taxis_t *) reshGetVal(taxisID, &taxisOps);
  *vDateTime_lb = taxisptr->vDateTime_lb;
  *vDateTime_ub = taxisptr->vDateTime_ub;
}

void
taxisDefVdateBounds(int taxisID, int vdate_lb, int vdate_ub)
{
  taxis_t *taxisptr = (taxis_t *) reshGetVal(taxisID, &taxisOps);

  if (taxisptr->has_bounds == false || (int) cdiDate_get(taxisptr->vDateTime_lb.date) != vdate_lb
      || (int) cdiDate_get(taxisptr->vDateTime_ub.date) != vdate_ub)
    {
      taxisptr->vDateTime_lb.date = cdiDate_set(vdate_lb);
      taxisptr->vDateTime_ub.date = cdiDate_set(vdate_ub);
      taxisptr->has_bounds = true;
      reshSetStatus(taxisID, &taxisOps, RESH_DESYNC_IN_USE);
    }
}

void
taxisDefVdatetimeBounds(int taxisID, CdiDateTime vDateTime_lb, CdiDateTime vDateTime_ub)
{
  taxis_t *taxisptr = (taxis_t *) reshGetVal(taxisID, &taxisOps);

  if (taxisptr->has_bounds == false || cdiDateTime_isNE(taxisptr->vDateTime_lb, vDateTime_lb)
      || cdiDateTime_isNE(taxisptr->vDateTime_ub, vDateTime_ub))
    {
      taxisptr->vDateTime_lb = vDateTime_lb;
      taxisptr->vDateTime_ub = vDateTime_ub;
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
  return cdiTime_get(taxisptr->vDateTime.time);
}

void
taxisInqVtimeBounds(int taxisID, int *vtime_lb, int *vtime_ub)
{
  const taxis_t *taxisptr = (taxis_t *) reshGetVal(taxisID, &taxisOps);
  *vtime_lb = cdiTime_get(taxisptr->vDateTime_lb.time);
  *vtime_ub = cdiTime_get(taxisptr->vDateTime_ub.time);
}

void
taxisDefVtimeBounds(int taxisID, int vtime_lb, int vtime_ub)
{
  taxis_t *taxisptr = (taxis_t *) reshGetVal(taxisID, &taxisOps);

  if (taxisptr->has_bounds == false || cdiTime_get(taxisptr->vDateTime_lb.time) != vtime_lb
      || cdiTime_get(taxisptr->vDateTime_ub.time) != vtime_ub)
    {
      taxisptr->vDateTime_lb.time = cdiTime_set(vtime_lb);
      taxisptr->vDateTime_ub.time = cdiTime_set(vtime_ub);
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

  if (cdiDateTime_isNull(taxisptr->rDateTime))
    {
      taxisptr->rDateTime = taxisptr->vDateTime;
      reshSetStatus(taxisID, &taxisOps, RESH_DESYNC_IN_USE);
    }

  return (int) cdiDate_get(taxisptr->rDateTime.date);
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

  if (cdiDateTime_isNull(taxisptr->rDateTime))
    {
      taxisptr->rDateTime = taxisptr->vDateTime;
      reshSetStatus(taxisID, &taxisOps, RESH_DESYNC_IN_USE);
    }

  return cdiTime_get(taxisptr->rDateTime.time);
}

/*
@Function  taxisInqFdatetime
@Title     Get the forecast reference date/time

@Prototype int taxisInqFdatetime(int taxisID)
@Parameter
    @Item  taxisID  Time axis ID, from a previous call to @fref{taxisCreate} or @fref{vlistInqTaxis}

@Description
The function @func{taxisInqFdatetime} returns the forecast reference date/time of a Time axis.

@Result
@func{taxisInqFdate} returns the forecast reference date/time.

@EndFunction
*/
CdiDateTime
taxisInqFdatetime(int taxisID)
{
  taxis_t *taxisptr = (taxis_t *) reshGetVal(taxisID, &taxisOps);

  if (cdiDateTime_isNull(taxisptr->fDateTime))
    {
      // rDateTime is initialized from vDateTime if empty!
      taxisptr->fDateTime = taxisInqRdatetime(taxisID);
      reshSetStatus(taxisID, &taxisOps, RESH_DESYNC_IN_USE);
    }

  return taxisptr->fDateTime;
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

char *
ptaxisAllocUnits(taxis_t *taxisptr, size_t len)
{
  delete_refcount_string(taxisptr->units);
  return taxisptr->units = new_refcount_string(len);
}

void
ptaxisDefUnits(taxis_t *taxisptr, const char *units)
{
  if (units)
    {
      const size_t len = strlen(units);
      char *taxisunits = ptaxisAllocUnits(taxisptr, len);
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
        if (cval != timevalue) printf("TUNIT_SECOND error: %g %g %d %d\n", timevalue, cval, julianDate.julianDay,
      julianDate.secondOfDay);
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
        if (cval != timevalue) printf("TUNIT_DAY error: %g %g %d %d\n", timevalue, cval, julianDate.julianDay,
      julianDate.secondOfDay);
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
  if (IS_EQUAL(timevalue, 0.0)) return taxis->rDateTime;

  int timeunits = taxis->unit;
  const int calendar = taxis->calendar;

  if (timeunits == TUNIT_MONTH && calendar == CALENDAR_360DAYS)
    {
      timeunits = TUNIT_DAY;
      timevalue *= 30;
    }

  CdiDateTime rDateTime = taxis->rDateTime;

  if (timeunits == TUNIT_MONTH || timeunits == TUNIT_YEAR)
    {
      int year = rDateTime.date.year;
      int month = rDateTime.date.month;

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

      rDateTime.date.year = year;
      rDateTime.date.month = month;
    }

  const JulianDate julianDate = julianDate_encode(calendar, rDateTime);
  const JulianDate julianDate2 = timevalue_decode(timeunits, timevalue);

  return julianDate_decode(calendar, julianDate_add(julianDate2, julianDate));
}

// convert CdiDateTime to relative time value
static double
datetime2rtimeval(CdiDateTime vDateTime, taxis_t *taxis)
{
  double value = 0.0;

  const int calendar = (*taxis).calendar;
  int timeunits = (*taxis).unit;
  const int timeunits0 = timeunits;

  CdiDateTime rDateTime = (*taxis).rDateTime;

  if (cdiDateTime_isNull(rDateTime)) rDateTime = (*taxis).vDateTime;

  if (cdiDateTime_isNull(rDateTime) && cdiDateTime_isNull(vDateTime)) return value;

  const JulianDate julianDate1 = julianDate_encode(calendar, rDateTime);

  if (timeunits == TUNIT_MONTH && calendar == CALENDAR_360DAYS) timeunits = TUNIT_DAY;

  if (timeunits == TUNIT_MONTH || timeunits == TUNIT_YEAR)
    {
      const int ryear = rDateTime.date.year;
      const int rmonth = rDateTime.date.month;
      int year = vDateTime.date.year;
      int month = vDateTime.date.month;
      value = (year - ryear) * 12 - rmonth + month;

      int nmonth = (int) value;
      month -= nmonth;

      // clang-format off
      while (month > 12) { month -= 12; year++; }
      while (month <  1) { month += 12; year--; }
      // clang-format on

      const int dpm = days_per_month(calendar, year, month);

      vDateTime.date.year = year;
      vDateTime.date.month = month;
      const JulianDate julianDate2 = julianDate_encode(calendar, vDateTime);
      const JulianDate dateDifference = julianDate_sub(julianDate2, julianDate1);

      value += (dateDifference.julianDay + dateDifference.secondOfDay / 86400.0) / dpm;
      if (timeunits == TUNIT_YEAR) value = value / 12;
    }
  else
    {
      const JulianDate julianDate2 = julianDate_encode(calendar, vDateTime);
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

  if (cdiDateTime_isNull(taxis->vDateTime) && DBL_IS_EQUAL(timevalue, 0.0)) return;

  if (timeunits == TUNIT_MONTH && calendar == CALENDAR_360DAYS)
    {
      timeunits = TUNIT_DAY;
      timevalue *= 30;
    }

  CdiDateTime vDateTime = taxis->vDateTime;

  if (timeunits == TUNIT_MONTH || timeunits == TUNIT_YEAR)
    {
      int year = vDateTime.date.year;
      int month = vDateTime.date.month;

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

      vDateTime.date.year = year;
      vDateTime.date.month = month;
    }

  const JulianDate julianDate = julianDate_encode(calendar, vDateTime);
  const JulianDate julianDate2 = timevalue_decode(timeunits, timevalue);

  (*taxis).fDateTime = julianDate_decode(calendar, julianDate_sub(julianDate, julianDate2));
}

CdiDateTime
cdi_decode_timeval(double timevalue, taxis_t *taxis)
{
  return (taxis->type == TAXIS_ABSOLUTE) ? split_timevalue(timevalue, taxis->unit) : rtimeval2datetime(timevalue, taxis);
}

double
cdi_encode_timeval(CdiDateTime datetime, taxis_t *taxis)
{
  double timeValue = 0.0;

  if (taxis->type == TAXIS_ABSOLUTE)
    {
      if (taxis->unit == TUNIT_YEAR)
        {
          timeValue = datetime.date.year;
        }
      else if (taxis->unit == TUNIT_MONTH)
        {
          const int64_t xdate = cdiDate_get(datetime.date);
          timeValue = xdate / 100 + copysign((double) (datetime.date.day != 0) * 0.5, (double) xdate);
        }
      else if (taxis->unit == TUNIT_SECOND)
        {
          int hour, minute, second, ms;
          cdiTime_decode(datetime.time, &hour, &minute, &second, &ms);
          timeValue = hour * 3600 + minute * 60 + second;
        }
      else
        {
          int hour, minute, second, ms;
          cdiTime_decode(datetime.time, &hour, &minute, &second, &ms);
          const int64_t xdate = cdiDate_get(datetime.date);
          timeValue = copysign(1.0, (double) xdate) * (fabs((double) xdate) + (hour * 3600 + minute * 60 + second) / 86400.0);
        }
    }
  else
    timeValue = datetime2rtimeval(datetime, taxis);

  return timeValue;
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
  dest->calendar = source->calendar;
  dest->unit = source->unit;
  dest->numavg = source->numavg;
  dest->climatology = source->climatology;
  dest->has_bounds = source->has_bounds;
  dest->sDateTime = source->sDateTime;
  dest->vDateTime = source->vDateTime;
  dest->rDateTime = source->rDateTime;
  dest->fDateTime = source->fDateTime;
  dest->vDateTime_lb = source->vDateTime_lb;
  dest->vDateTime_ub = source->vDateTime_ub;
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
          taxisptr->self, taxisptr->self, (int) taxisptr->used, taxisptr->type, (int) cdiDate_get(taxisptr->vDateTime.date),
          cdiTime_get(taxisptr->vDateTime.time), (int) cdiDate_get(taxisptr->rDateTime.date), cdiTime_get(taxisptr->rDateTime.time),
          (int) cdiDate_get(taxisptr->fDateTime.date), cdiTime_get(taxisptr->fDateTime.time), taxisptr->calendar, taxisptr->unit,
          taxisptr->numavg, (int) taxisptr->climatology, (int) taxisptr->has_bounds, vdate_lb, vtime_lb, vdate_ub, vtime_ub,
          taxisptr->fc_unit, taxisptr->fc_period);
}

static int
taxisCompareP(void *taxisptr1, void *taxisptr2)
{
  const taxis_t *t1 = (const taxis_t *) taxisptr1, *t2 = (const taxis_t *) taxisptr2;

  xassert(t1 && t2);

  return !(t1->used == t2->used && t1->type == t2->type && cdiDateTime_isEQ(t1->vDateTime, t2->vDateTime)
           && cdiDateTime_isEQ(t1->rDateTime, t2->rDateTime) && cdiDateTime_isEQ(t1->fDateTime, t2->fDateTime)
           && t1->calendar == t2->calendar && t1->unit == t2->unit && t1->fc_unit == t2->fc_unit && t1->numavg == t2->numavg
           && t1->climatology == t2->climatology && t1->has_bounds == t2->has_bounds
           && cdiDateTime_isEQ(t1->vDateTime_lb, t2->vDateTime_lb) && cdiDateTime_isEQ(t1->vDateTime_ub, t2->vDateTime_ub));
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
  taxisP->vDateTime.date = cdiDate_set(intBuffer[idx++]);
  taxisP->vDateTime.time = cdiTime_set(intBuffer[idx++]);
  taxisP->rDateTime.date = cdiDate_set(intBuffer[idx++]);
  taxisP->rDateTime.time = cdiTime_set(intBuffer[idx++]);
  taxisP->fDateTime.date = cdiDate_set(intBuffer[idx++]);
  taxisP->fDateTime.time = cdiTime_set(intBuffer[idx++]);
  taxisP->calendar = intBuffer[idx++];
  taxisP->unit = intBuffer[idx++];
  taxisP->fc_unit = intBuffer[idx++];
  taxisP->numavg = intBuffer[idx++];
  taxisP->climatology = intBuffer[idx++];
  taxisP->has_bounds = (short) intBuffer[idx++];
  taxisP->vDateTime_lb.date = cdiDate_set(intBuffer[idx++]);
  taxisP->vDateTime_lb.time = cdiTime_set(intBuffer[idx++]);
  taxisP->vDateTime_ub.date = cdiDate_set(intBuffer[idx++]);
  taxisP->vDateTime_ub.time = cdiTime_set(intBuffer[idx++]);

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
  intBuffer[idx++] = (int) cdiDate_get(taxisP->vDateTime.date);
  intBuffer[idx++] = cdiTime_get(taxisP->vDateTime.time);
  intBuffer[idx++] = (int) cdiDate_get(taxisP->rDateTime.date);
  intBuffer[idx++] = cdiTime_get(taxisP->rDateTime.time);
  intBuffer[idx++] = (int) cdiDate_get(taxisP->fDateTime.date);
  intBuffer[idx++] = cdiTime_get(taxisP->fDateTime.time);
  intBuffer[idx++] = taxisP->calendar;
  intBuffer[idx++] = taxisP->unit;
  intBuffer[idx++] = taxisP->fc_unit;
  intBuffer[idx++] = taxisP->numavg;
  intBuffer[idx++] = taxisP->climatology;
  intBuffer[idx++] = taxisP->has_bounds;
  intBuffer[idx++] = (int) cdiDate_get(taxisP->vDateTime_lb.date);
  intBuffer[idx++] = cdiTime_get(taxisP->vDateTime_lb.time);
  intBuffer[idx++] = (int) cdiDate_get(taxisP->vDateTime_ub.date);
  intBuffer[idx++] = cdiTime_get(taxisP->vDateTime_ub.time);
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
