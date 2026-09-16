/* DO NOT REMOVE the config.h include file under any circumstances,
 * it's very much needed on some platforms */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
/* DO NOT REMOVE the above config.h include file under any
 * circumstances as long as it's the autoconf configuration header
 * used to build this package. When it's missing on some platforms,
 * some poor person has to do long, tedious debugging sessions, where
 * struct offsets almost imperceptibly change from one file to the
 * next to find out what happened */

#include <stddef.h>
#include <stdio.h>

#include "cdi.h"
#include "dmemory.h"
#include "taxis.h"
#include "taxis_pack.h"
#include "cdi_int.h"
#include "normalize_month.h"

static int DefaultTimeType = TAXIS_ABSOLUTE;
static int DefaultTimeUnit = TUNIT_HOUR;

static int taxis_compare(void *taxisP1, void *taxisP2);
static void taxis_destroy(void *taxisP);
static void taxis_print(taxis_t *taxisP, FILE *fp);

const resOps taxisOps
    = { taxis_compare, taxis_destroy, (void (*)(void *, FILE *)) taxis_print, taxisGetPackSize, taxisPack, taxisTxCode };

#define container_of(ptr, type, member) ((type *) (void *) ((unsigned char *) ptr - offsetof(type, member)))

struct refcount_string
{
  int ref_count;
  char string[];
};

char *
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

const char *
taxisNamePtr(int taxisID)
{
  taxis_t *taxisP = (taxis_t *) reshGetVal(taxisID, &taxisOps);
  return taxisP->name;
}

const char *
tunitNamePtr(int unitID)
{
  static const char Timeunits[][TAXIS_MAX_UNIT_STR_LEN + 1] = {
    "undefined", "seconds", "minutes", "quarters", "30minutes", "hours", "3hours", "6hours", "12hours", "days", "months", "years",
  };
  enum
  {
    size = ARRAY_SIZE(Timeunits)
  };

  return (unitID > 0 && unitID < size) ? Timeunits[unitID] : Timeunits[0];
}

void
ptaxisInit(taxis_t *taxisP)
{
  taxisP->self = CDI_UNDEFID;
  taxisP->type = DefaultTimeType;
  taxisP->calendar = CDI_Default_Calendar;
  taxisP->unit = DefaultTimeUnit;
  taxisP->numavg = 0;
  taxisP->climatology = false;
  taxisP->hasBounds = false;
  cdiDateTime_init(&taxisP->sDateTime);
  cdiDateTime_init(&taxisP->vDateTime);
  cdiDateTime_init(&taxisP->rDateTime);
  cdiDateTime_init(&taxisP->fDateTime);
  cdiDateTime_init(&taxisP->vDateTime_lb);
  cdiDateTime_init(&taxisP->vDateTime_ub);
  taxisP->fc_unit = DefaultTimeUnit;
  taxisP->fc_period = 0;
  taxisP->name = NULL;
  taxisP->longname = NULL;
  taxisP->units = NULL;

  cdiInitKeys(&taxisP->keys);
  cdiDefVarKeyInt(&taxisP->keys, CDI_KEY_DATATYPE, CDI_DATATYPE_FLT64);
}

taxis_t *
taxisNewEntry(cdiResH resH)
{
  taxis_t *taxisP = (taxis_t *) Malloc(sizeof(taxis_t));

  ptaxisInit(taxisP);
  if (resH == CDI_UNDEFID) { taxisP->self = reshPut(taxisP, &taxisOps); }
  else
  {
    taxisP->self = resH;
    reshReplace(resH, taxisP, &taxisOps);
  }

  return taxisP;
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
  taxis_t *taxisP = taxisNewEntry(CDI_UNDEFID);
  taxisP->type = taxistype;

  int taxisID = taxisP->self;
  return taxisID;
}

void
taxisDestroyKernel(taxis_t *taxisP)
{
  delete_refcount_string(taxisP->name);
  delete_refcount_string(taxisP->longname);
  delete_refcount_string(taxisP->units);
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
  taxis_t *taxisP = (taxis_t *) reshGetVal(taxisID, &taxisOps);
  reshRemove(taxisID, &taxisOps);
  taxisDestroyKernel(taxisP);
  Free(taxisP);
}

void
taxis_destroy(void *taxisP)
{
  taxisDestroyKernel((taxis_t *) taxisP);
  Free(taxisP);
}

int
taxisDuplicate(int taxisID1)
{
  taxis_t *taxisP1 = (taxis_t *) reshGetVal(taxisID1, &taxisOps);
  taxis_t *taxisP2 = taxisNewEntry(CDI_UNDEFID);

  int taxisID2 = taxisP2->self;

  ptaxisCopy(taxisP2, taxisP1);

  return taxisID2;
}

void
taxisDefType(int taxisID, int taxistype)
{
  taxis_t *taxisP = (taxis_t *) reshGetVal(taxisID, &taxisOps);

  if (taxisP->type != taxistype)
  {
    taxisP->type = taxistype;
    cdiDefVarKeyInt(&taxisP->keys, CDI_KEY_DATATYPE, CDI_DATATYPE_FLT64);
    delete_refcount_string(taxisP->units);
    taxisP->units = NULL;
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
  taxis_t *taxisP = (taxis_t *) reshGetVal(taxisID, &taxisOps);

  if ((int) cdiDate_get(taxisP->vDateTime.date) != vdate)
  {
    taxisP->vDateTime.date = cdiDate_set(vdate);
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
  taxis_t *taxisP = (taxis_t *) reshGetVal(taxisID, &taxisOps);

  if (cdiTime_get(taxisP->vDateTime.time) != vtime)
  {
    taxisP->vDateTime.time = cdiTime_set(vtime);
    reshSetStatus(taxisID, &taxisOps, RESH_DESYNC_IN_USE);
  }
}

void
taxisDefVdatetime(int taxisID, CdiDateTime vDateTime)
{
  taxis_t *taxisP = (taxis_t *) reshGetVal(taxisID, &taxisOps);

  if (cdiDateTime_isNE(taxisP->vDateTime, vDateTime))
  {
    taxisP->vDateTime = vDateTime;
    reshSetStatus(taxisID, &taxisOps, RESH_DESYNC_IN_USE);
  }
}

void
taxisDefRdatetime(int taxisID, CdiDateTime rDateTime)
{
  taxis_t *taxisP = (taxis_t *) reshGetVal(taxisID, &taxisOps);

  if (cdiDateTime_isNE(taxisP->rDateTime, rDateTime))
  {
    taxisP->rDateTime = rDateTime;
    delete_refcount_string(taxisP->units);
    taxisP->units = NULL;
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
  taxis_t *taxisP = (taxis_t *) reshGetVal(taxisID, &taxisOps);

  if ((int) cdiDate_get(taxisP->rDateTime.date) != rdate)
  {
    taxisP->rDateTime.date = cdiDate_set(rdate);
    delete_refcount_string(taxisP->units);
    taxisP->units = NULL;
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
  taxis_t *taxisP = (taxis_t *) reshGetVal(taxisID, &taxisOps);

  if (cdiTime_get(taxisP->rDateTime.time) != rtime)
  {
    taxisP->rDateTime.time = cdiTime_set(rtime);
    if (taxisP->units)
    {
      delete_refcount_string(taxisP->units);
      taxisP->units = NULL;
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
  taxis_t *taxisP = (taxis_t *) reshGetVal(taxisID, &taxisOps);

  if (cdiDateTime_isNE(taxisP->fDateTime, fDateTime))
  {
    taxisP->fDateTime = fDateTime;
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
  taxis_t *taxisP = (taxis_t *) reshGetVal(taxisID, &taxisOps);

  if (taxisP->calendar != calendar)
  {
    taxisP->calendar = calendar;
    reshSetStatus(taxisID, &taxisOps, RESH_DESYNC_IN_USE);
  }
}

void
taxisDefTunit(int taxisID, int unit)
{
  taxis_t *taxisP = (taxis_t *) reshGetVal(taxisID, &taxisOps);

  if (taxisP->unit != unit)
  {
    taxisP->unit = unit;
    delete_refcount_string(taxisP->units);
    taxisP->units = NULL;
    reshSetStatus(taxisID, &taxisOps, RESH_DESYNC_IN_USE);
  }
}

void
taxisDefForecastTunit(int taxisID, int unit)
{
  taxis_t *taxisP = (taxis_t *) reshGetVal(taxisID, &taxisOps);

  if (taxisP->fc_unit != unit)
  {
    taxisP->fc_unit = unit;
    reshSetStatus(taxisID, &taxisOps, RESH_DESYNC_IN_USE);
  }
}

void
taxisDefForecastPeriod(int taxisID, double fc_period)
{
  taxis_t *taxisP = (taxis_t *) reshGetVal(taxisID, &taxisOps);

  if (is_not_equal(taxisP->fc_period, fc_period))
  {
    taxisP->fc_period = fc_period;
    reshSetStatus(taxisID, &taxisOps, RESH_DESYNC_IN_USE);
  }
}

void
taxisDefNumavg(int taxisID, int numavg)
{
  taxis_t *taxisP = (taxis_t *) reshGetVal(taxisID, &taxisOps);

  if (taxisP->numavg != numavg)
  {
    taxisP->numavg = numavg;
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
  taxis_t *taxisP = (taxis_t *) reshGetVal(taxisID, &taxisOps);
  return taxisP->type;
}

int
taxisHasBounds(int taxisID)
{
  taxis_t *taxisP = (taxis_t *) reshGetVal(taxisID, &taxisOps);
  return (int) taxisP->hasBounds;
}

void
taxisWithBounds(int taxisID)
{
  taxis_t *taxisP = (taxis_t *) reshGetVal(taxisID, &taxisOps);

  if (taxisP->hasBounds == false)
  {
    taxisP->hasBounds = true;
    reshSetStatus(taxisID, &taxisOps, RESH_DESYNC_IN_USE);
  }
}

void
taxisDeleteBounds(int taxisID)
{
  taxis_t *taxisP = (taxis_t *) reshGetVal(taxisID, &taxisOps);

  if (taxisP->hasBounds)
  {
    taxisP->hasBounds = false;
    reshSetStatus(taxisID, &taxisOps, RESH_DESYNC_IN_USE);
  }
}

void
taxisCopyTimestep(int taxisID2, int taxisID1)
{
  taxis_t *taxisP1 = (taxis_t *) reshGetVal(taxisID1, &taxisOps), *taxisP2 = (taxis_t *) reshGetVal(taxisID2, &taxisOps);

  reshLock();

  // reference date/time and units can't be changed after streamDefVlist()!

  taxisP2->sDateTime = taxisP1->sDateTime;
  taxisP2->vDateTime = taxisP1->vDateTime;
  taxisP2->fDateTime = taxisP1->fDateTime;

  if (taxisP2->hasBounds)
  {
    taxisP2->vDateTime_lb = taxisP1->vDateTime_lb;
    taxisP2->vDateTime_ub = taxisP1->vDateTime_ub;
  }

  taxisP2->fc_unit = taxisP1->fc_unit;
  taxisP2->fc_period = taxisP1->fc_period;

  reshSetStatus(taxisID2, &taxisOps, RESH_DESYNC_IN_USE);
  reshUnlock();
}

CdiDateTime
taxisInqVdatetime(int taxisID)
{
  taxis_t *taxisP = (taxis_t *) reshGetVal(taxisID, &taxisOps);
  return taxisP->vDateTime;
}

CdiDateTime
taxisInqRdatetime(int taxisID)
{
  taxis_t *taxisP = (taxis_t *) reshGetVal(taxisID, &taxisOps);

  if (cdiDateTime_isNull(taxisP->rDateTime))
  {
    taxisP->rDateTime = taxisP->vDateTime;
    reshSetStatus(taxisID, &taxisOps, RESH_DESYNC_IN_USE);
  }

  return taxisP->rDateTime;
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
  taxis_t *taxisP = (taxis_t *) reshGetVal(taxisID, &taxisOps);
  return (int) cdiDate_get(taxisP->vDateTime.date);
}

CdiDateTime
taxisInqSdatetime(int taxisID)
{
  taxis_t *taxisP = (taxis_t *) reshGetVal(taxisID, &taxisOps);
  return taxisP->sDateTime;
}

void
taxisInqVdateBounds(int taxisID, int *vdate_lb, int *vdate_ub)
{
  taxis_t *taxisP = (taxis_t *) reshGetVal(taxisID, &taxisOps);
  *vdate_lb = (int) cdiDate_get(taxisP->vDateTime_lb.date);
  *vdate_ub = (int) cdiDate_get(taxisP->vDateTime_ub.date);
}

void
taxisInqVdatetimeBounds(int taxisID, CdiDateTime *vDateTime_lb, CdiDateTime *vDateTime_ub)
{
  taxis_t *taxisP = (taxis_t *) reshGetVal(taxisID, &taxisOps);
  *vDateTime_lb = taxisP->vDateTime_lb;
  *vDateTime_ub = taxisP->vDateTime_ub;
}

void
taxisDefVdateBounds(int taxisID, int vdate_lb, int vdate_ub)
{
  taxis_t *taxisP = (taxis_t *) reshGetVal(taxisID, &taxisOps);

  if (taxisP->hasBounds == false || (int) cdiDate_get(taxisP->vDateTime_lb.date) != vdate_lb
      || (int) cdiDate_get(taxisP->vDateTime_ub.date) != vdate_ub)
  {
    taxisP->vDateTime_lb.date = cdiDate_set(vdate_lb);
    taxisP->vDateTime_ub.date = cdiDate_set(vdate_ub);
    taxisP->hasBounds = true;
    reshSetStatus(taxisID, &taxisOps, RESH_DESYNC_IN_USE);
  }
}

void
taxisDefVdatetimeBounds(int taxisID, CdiDateTime vDateTime_lb, CdiDateTime vDateTime_ub)
{
  taxis_t *taxisP = (taxis_t *) reshGetVal(taxisID, &taxisOps);

  if (taxisP->hasBounds == false || cdiDateTime_isNE(taxisP->vDateTime_lb, vDateTime_lb)
      || cdiDateTime_isNE(taxisP->vDateTime_ub, vDateTime_ub))
  {
    taxisP->vDateTime_lb = vDateTime_lb;
    taxisP->vDateTime_ub = vDateTime_ub;
    taxisP->hasBounds = true;
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
  taxis_t *taxisP = (taxis_t *) reshGetVal(taxisID, &taxisOps);
  return cdiTime_get(taxisP->vDateTime.time);
}

void
taxisInqVtimeBounds(int taxisID, int *vtime_lb, int *vtime_ub)
{
  taxis_t *taxisP = (taxis_t *) reshGetVal(taxisID, &taxisOps);
  *vtime_lb = cdiTime_get(taxisP->vDateTime_lb.time);
  *vtime_ub = cdiTime_get(taxisP->vDateTime_ub.time);
}

void
taxisDefVtimeBounds(int taxisID, int vtime_lb, int vtime_ub)
{
  taxis_t *taxisP = (taxis_t *) reshGetVal(taxisID, &taxisOps);

  if (taxisP->hasBounds == false || cdiTime_get(taxisP->vDateTime_lb.time) != vtime_lb
      || cdiTime_get(taxisP->vDateTime_ub.time) != vtime_ub)
  {
    taxisP->vDateTime_lb.time = cdiTime_set(vtime_lb);
    taxisP->vDateTime_ub.time = cdiTime_set(vtime_ub);
    taxisP->hasBounds = true;
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
  taxis_t *taxisP = (taxis_t *) reshGetVal(taxisID, &taxisOps);

  if (cdiDateTime_isNull(taxisP->rDateTime))
  {
    taxisP->rDateTime = taxisP->vDateTime;
    reshSetStatus(taxisID, &taxisOps, RESH_DESYNC_IN_USE);
  }

  return (int) cdiDate_get(taxisP->rDateTime.date);
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
  taxis_t *taxisP = (taxis_t *) reshGetVal(taxisID, &taxisOps);

  if (cdiDateTime_isNull(taxisP->rDateTime))
  {
    taxisP->rDateTime = taxisP->vDateTime;
    reshSetStatus(taxisID, &taxisOps, RESH_DESYNC_IN_USE);
  }

  return cdiTime_get(taxisP->rDateTime.time);
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
  taxis_t *taxisP = (taxis_t *) reshGetVal(taxisID, &taxisOps);

  if (cdiDateTime_isNull(taxisP->fDateTime))
  {
    // rDateTime is initialized from vDateTime if empty!
    taxisP->fDateTime = taxisInqRdatetime(taxisID);
    reshSetStatus(taxisID, &taxisOps, RESH_DESYNC_IN_USE);
  }

  return taxisP->fDateTime;
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
  taxis_t *taxisP = (taxis_t *) reshGetVal(taxisID, &taxisOps);
  return taxisP->calendar;
}

int
taxisInqTunit(int taxisID)
{
  taxis_t *taxisP = (taxis_t *) reshGetVal(taxisID, &taxisOps);
  return taxisP->unit;
}

int
taxisInqForecastTunit(int taxisID)
{
  taxis_t *taxisP = (taxis_t *) reshGetVal(taxisID, &taxisOps);
  return taxisP->fc_unit;
}

double
taxisInqForecastPeriod(int taxisID)
{
  taxis_t *taxisP = (taxis_t *) reshGetVal(taxisID, &taxisOps);
  return taxisP->fc_period;
}

int
taxisInqNumavg(int taxisID)
{
  taxis_t *taxisP = (taxis_t *) reshGetVal(taxisID, &taxisOps);
  return taxisP->numavg;
}

taxis_t *
taxis_to_pointer(int taxisID)
{
  taxis_t *taxisP = (taxis_t *) reshGetVal(taxisID, &taxisOps);
  return taxisP;
}

void
ptaxisDefDatatype(taxis_t *taxisP, int datatype)
{
  cdiDefVarKeyInt(&taxisP->keys, CDI_KEY_DATATYPE, datatype);
}

void
ptaxisDefName(taxis_t *taxisP, const char *name)
{
  if (name)
  {
    size_t len = strlen(name);
    delete_refcount_string(taxisP->name);
    char *taxisname = taxisP->name = new_refcount_string(len);
    strncpy(taxisname, name, len + 1);
  }
}

void
ptaxisDefLongname(taxis_t *taxisP, const char *longname)
{
  if (longname)
  {
    size_t len = strlen(longname);
    delete_refcount_string(taxisP->longname);
    char *taxislongname = taxisP->longname = new_refcount_string(len);
    strncpy(taxislongname, longname, len + 1);
  }
}

char *
ptaxisAllocUnits(taxis_t *taxisP, size_t len)
{
  delete_refcount_string(taxisP->units);
  return taxisP->units = new_refcount_string(len);
}

void
ptaxisDefUnits(taxis_t *taxisP, const char *units)
{
  if (units)
  {
    size_t len = strlen(units);
    char *taxisunits = ptaxisAllocUnits(taxisP, len);
    strncpy(taxisunits, units, len + 1);
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
    double seconds = fmod(timevalue, 86400.0);
    /* FIXME: does the following line serve a definable purpose? */
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
    double days, seconds = modf(timevalue, &days) * 86400.0;
    /* FIXME: why does the seconds value have to be integral? If that's
     * needed why not use modf? */
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
    CDI_WARN_ONCE("timeunit %s unsupported!", tunitNamePtr(timeunits));
  }

  return julianDate;
}

static double
cdi_encode_timevalue(int64_t days, double secs, int timeunit)
{
  double timevalue = 0.0;

  if (timeunit == TUNIT_SECOND) { timevalue = (double) days * 86400.0 + secs; }
  else if (timeunit == TUNIT_MINUTE || timeunit == TUNIT_QUARTER || timeunit == TUNIT_30MINUTES)
  {
    timevalue = (double) days * 1440. + secs / 60.;
  }
  else if (timeunit == TUNIT_HOUR || timeunit == TUNIT_3HOURS || timeunit == TUNIT_6HOURS || timeunit == TUNIT_12HOURS)
  {
    timevalue = (double) days * 24. + secs / 3600.;
  }
  else if (timeunit == TUNIT_DAY) { timevalue = (double) days + secs / 86400.; }
  else
  {
    CDI_WARN_ONCE("timeunit %s unsupported!", tunitNamePtr(timeunit));
  }

  return timevalue;
}

// convert relative time value to CdiDateTime
static CdiDateTime
rtimeval2datetime(double timevalue, const taxis_t *taxis)
{
  if (is_equal(timevalue, 0.0)) return taxis->rDateTime;

  int timeunits = taxis->unit;
  int calendar = taxis->calendar;

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

    double fmon = fmod(timevalue, 1.0);

    month += (int) timevalue;

    struct YearMonth ym = normalize_month(year, month);
    year = ym.year;
    month = ym.month;

    timeunits = TUNIT_DAY;
    timevalue = fmon * days_per_month(calendar, year, month);

    rDateTime.date.year = year;
    rDateTime.date.month = (short) month;
  }

  JulianDate julianDate = julianDate_encode(calendar, rDateTime);
  JulianDate julianDate2 = timevalue_decode(timeunits, timevalue);

  return julianDate_decode(calendar, julianDate_add(julianDate2, julianDate));
}

// convert CdiDateTime to relative time value
static double
datetime2rtimeval(CdiDateTime vDateTime, const taxis_t *taxis)
{
  double value = 0.0;

  int calendar = taxis->calendar;
  int timeunits = taxis->unit;
  int timeunits0 = timeunits;

  CdiDateTime rDateTime = taxis->rDateTime;

  if (cdiDateTime_isNull(rDateTime)) rDateTime = (*taxis).vDateTime;

  if (cdiDateTime_isNull(rDateTime) && cdiDateTime_isNull(vDateTime)) return value;

  JulianDate julianDate1 = julianDate_encode(calendar, rDateTime);

  if (timeunits == TUNIT_MONTH && calendar == CALENDAR_360DAYS) timeunits = TUNIT_DAY;

  if (timeunits == TUNIT_MONTH || timeunits == TUNIT_YEAR)
  {
    int ryear = rDateTime.date.year;
    int rmonth = rDateTime.date.month;
    int year = vDateTime.date.year;
    int month = vDateTime.date.month;
    value = (year - ryear) * 12 - rmonth + month;

    int nmonth = (int) value;
    month -= nmonth;

    struct YearMonth ym = normalize_month(year, month);
    year = ym.year;
    month = ym.month;

    int dpm = days_per_month(calendar, year, month);

    vDateTime.date.year = year;
    vDateTime.date.month = (short) month;
    JulianDate julianDate2 = julianDate_encode(calendar, vDateTime);
    JulianDate dateDifference = julianDate_sub(julianDate2, julianDate1);

    value += ((double) dateDifference.julianDay + dateDifference.secondOfDay / 86400.0) / dpm;
    if (timeunits == TUNIT_YEAR) value = value / 12;
  }
  else
  {
    JulianDate julianDate2 = julianDate_encode(calendar, vDateTime);
    JulianDate dateDifference = julianDate_sub(julianDate2, julianDate1);

    value = cdi_encode_timevalue(dateDifference.julianDay, dateDifference.secondOfDay, timeunits);
  }

  if (timeunits0 == TUNIT_MONTH && calendar == CALENDAR_360DAYS) value /= 30.0;

  return value;
}

// convert absolute seconds to CdiDateTime
static CdiDateTime
seconds2datetime(double timevalue)
{
  int calendar = CALENDAR_STANDARD;
  int64_t seconds = (int64_t) timevalue;

  CdiDateTime datetime0;
  datetime0.date = cdiDate_encode(1, 1, 1);
  datetime0.time = cdiTime_encode(0, 0, 0, 0);
  JulianDate julianDate = julianDate_encode(calendar, datetime0);

  return julianDate_decode(calendar, julianDate_add_seconds(julianDate, seconds));
}

// convert absolute time value to CdiDateTime
static CdiDateTime
absTimeval2datetime(double timevalue)
{
  int64_t vdate = (int64_t) timevalue;
  double days, tmpval = modf(timevalue, &days) * 86400.0;
  /* FIXME: why +0.01? */
  int daysec = (vdate < 0) ? (int) (-tmpval + 0.01) : (int) (tmpval + 0.01);

  CdiDateTime datetime;
  datetime.date = cdiDate_set(vdate);
  int hour = daysec / 3600;
  int minute = (daysec - hour * 3600) / 60;
  int second = daysec - hour * 3600 - minute * 60;
  int ms = 0;
  datetime.time = cdiTime_encode(hour, minute, second, ms);

  return datetime;
}

static CdiDateTime
split_timevalue(double timevalue, int timeunit)
{
  CdiDateTime datetime;
  cdiDateTime_init(&datetime);

  if (timeunit == TUNIT_SECOND) { datetime = seconds2datetime(timevalue); }
  else if (timeunit == TUNIT_HOUR)
  {
    timevalue /= 24;
    datetime = absTimeval2datetime(timevalue);
  }
  else if (timeunit == TUNIT_DAY) { datetime = absTimeval2datetime(timevalue); }
  else if (timeunit == TUNIT_MONTH)
  {
    int64_t vdate = (int64_t) timevalue * 100 - ((timevalue < 0) * 2 - 1);
    datetime.date = cdiDate_set(vdate);
  }
  else if (timeunit == TUNIT_YEAR)
  {
    CDI_WARN_ONCE("Fraction of a year is not supported!!");

    if (timevalue < -214700)
    {
      CDI_WARN_ONCE("Year %g out of range, set to -214700", timevalue);
      timevalue = -214700;
    }
    else if (timevalue > 214700)
    {
      CDI_WARN_ONCE("Year %g out of range, set to 214700", timevalue);
      timevalue = 214700;
    }

    int64_t vdate = (int64_t) timevalue * 10000;
    vdate += (timevalue < 0) ? -101 : 101;
    datetime.date = cdiDate_set(vdate);
  }
  else
  {
    CDI_WARN_ONCE("timeunit %s unsupported!", tunitNamePtr(timeunit));
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

    CDI_WARN_ONCE("Reset wrong date/time to %4.4d-%2.2d-%2.2d %2.2d:%2.2d:%2.2d!", year, month, day, hour, minute, second);
  }

  return datetime;
}

void
cdi_set_forecast_period(double timevalue, taxis_t *taxis)
{
  taxis->fc_period = timevalue;

  int timeunits = taxis->fc_unit;
  int calendar = taxis->calendar;

  if (cdiDateTime_isNull(taxis->vDateTime) && dbl_is_equal(timevalue, 0.0)) return;

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

    double fmon = fmod(timevalue, 1.0);

    month -= (int) timevalue;

    struct YearMonth ym = normalize_month(year, month);
    year = ym.year;
    month = ym.month;

    timeunits = TUNIT_DAY;
    timevalue = fmon * days_per_month(calendar, year, month);

    vDateTime.date.year = year;
    vDateTime.date.month = (short) month;
  }

  JulianDate julianDate = julianDate_encode(calendar, vDateTime);
  JulianDate julianDate2 = timevalue_decode(timeunits, timevalue);

  taxis->fDateTime = julianDate_decode(calendar, julianDate_sub(julianDate, julianDate2));
}

CdiDateTime
cdi_decode_timeval(double timevalue, const taxis_t *taxis)
{
  return (taxis->type == TAXIS_ABSOLUTE) ? split_timevalue(timevalue, taxis->unit) : rtimeval2datetime(timevalue, taxis);
}

/*
static int64_t
datetime2seconds(CdiDateTime datetime)
{
  int calendar = CALENDAR_STANDARD;

  CdiDateTime datetime0;
  datetime0.date = cdiDate_encode(1, 1, 1);
  datetime0.time = cdiTime_encode(0, 0, 0, 0);
  JulianDate julianDate0 = julianDate_encode(calendar, datetime0);
  JulianDate julianDate = julianDate_encode(calendar, datetime);

  int64_t days = julianDate.julianDay - julianDate0.julianDay;
  int64_t seconds = days * 86400 + (int64_t) julianDate.secondOfDay;

  return seconds;
}
*/
double
cdi_encode_timeval(CdiDateTime datetime, taxis_t *taxis)
{
  double timeValue = 0.0;

  if (taxis->type == TAXIS_ABSOLUTE)
  {
    if (taxis->unit == TUNIT_YEAR) { timeValue = datetime.date.year; }
    else if (taxis->unit == TUNIT_MONTH)
    {
      int64_t xdate = cdiDate_get(datetime.date);
      timeValue = (double) xdate / 100 + copysign((double) (datetime.date.day != 0) * 0.5, (double) xdate);
    }
    /*
    else if (taxis->unit == TUNIT_SECOND)
      {
        timeValue = (double) datetime2seconds(datetime);
      }
    */
    else
    {
      int hour, minute, second, ms;
      cdiTime_decode(datetime.time, &hour, &minute, &second, &ms);
      int64_t xdate = cdiDate_get(datetime.date);
      timeValue = copysign(1.0, (double) xdate) * (fabs((double) xdate) + (hour * 3600 + minute * 60 + second) / 86400.0);
    }
  }
  else
    timeValue = datetime2rtimeval(datetime, taxis);

  return timeValue;
}

void
ptaxisCopy(taxis_t *dest, taxis_t *source)
{
  reshLock();

  // memcpy(dest, source, sizeof(taxis_t));
  dest->type = source->type;
  dest->calendar = source->calendar;
  dest->unit = source->unit;
  dest->numavg = source->numavg;
  dest->climatology = source->climatology;
  dest->hasBounds = source->hasBounds;
  dest->sDateTime = source->sDateTime;
  dest->vDateTime = source->vDateTime;
  dest->rDateTime = source->rDateTime;
  dest->fDateTime = source->fDateTime;
  dest->vDateTime_lb = source->vDateTime_lb;
  dest->vDateTime_ub = source->vDateTime_ub;
  dest->fc_unit = source->fc_unit;
  dest->fc_period = source->fc_period;

  delete_refcount_string(dest->name);
  delete_refcount_string(dest->longname);
  delete_refcount_string(dest->units);
  dest->name = dup_refcount_string(source->name);
  dest->longname = dup_refcount_string(source->longname);
  dest->units = dup_refcount_string(source->units);
  if (dest->self != CDI_UNDEFID) reshSetStatus(dest->self, &taxisOps, RESH_DESYNC_IN_USE);

  cdiInitKeys(&dest->keys);
  cdiCopyVarKeys(&source->keys, &dest->keys);

  reshUnlock();
}

static void
taxis_print(taxis_t *taxisP, FILE *fp)
{
  int vdate_lb, vdate_ub;
  int vtime_lb, vtime_ub;
  taxisInqVdateBounds(taxisP->self, &vdate_lb, &vdate_ub);
  taxisInqVtimeBounds(taxisP->self, &vtime_lb, &vtime_ub);

  fprintf(fp,
          "#\n"
          "# taxisID %d\n"
          "#\n"
          "self        = %d\n"
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
          "hasBounds   = %d\n"
          "vdate_lb    = %d\n"
          "vtime_lb    = %d\n"
          "vdate_ub    = %d\n"
          "vtime_ub    = %d\n"
          "fc_unit     = %d\n"
          "fc_period   = %g\n"
          "\n",
          taxisP->self, taxisP->self, taxisP->type, (int) cdiDate_get(taxisP->vDateTime.date), cdiTime_get(taxisP->vDateTime.time),
          (int) cdiDate_get(taxisP->rDateTime.date), cdiTime_get(taxisP->rDateTime.time), (int) cdiDate_get(taxisP->fDateTime.date),
          cdiTime_get(taxisP->fDateTime.time), taxisP->calendar, taxisP->unit, taxisP->numavg, (int) taxisP->climatology,
          (int) taxisP->hasBounds, vdate_lb, vtime_lb, vdate_ub, vtime_ub, taxisP->fc_unit, taxisP->fc_period);
}

static int
taxis_compare(void *taxisP1, void *taxisP2)
{
  const taxis_t *t1 = (const taxis_t *) taxisP1, *t2 = (const taxis_t *) taxisP2;

  xassert(t1 && t2);

  return !(t1->type == t2->type && cdiDateTime_isEQ(t1->vDateTime, t2->vDateTime) && cdiDateTime_isEQ(t1->rDateTime, t2->rDateTime)
           && cdiDateTime_isEQ(t1->fDateTime, t2->fDateTime) && t1->calendar == t2->calendar && t1->unit == t2->unit
           && t1->fc_unit == t2->fc_unit && is_equal(t1->fc_period, t2->fc_period) && t1->numavg == t2->numavg
           && t1->climatology == t2->climatology && t1->hasBounds == t2->hasBounds
           && cdiDateTime_isEQ(t1->vDateTime_lb, t2->vDateTime_lb) && cdiDateTime_isEQ(t1->vDateTime_ub, t2->vDateTime_ub));
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
