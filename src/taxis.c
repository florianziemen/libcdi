#if defined (HAVE_CONFIG_H)
#  include "config.h"
#endif

#include <string.h>

#include "dmemory.h"

#include "cdi.h"
#include "stream_int.h"

int DefaultTimeType = TAXIS_ABSOLUTE;
int DefaultTimeUnit = TUNIT_HOUR;
int DefaultCalendar = CALENDAR_STANDARD;


char *Timeunits[] = {
  "undefined",
  "seconds",
  "minutes",
  "hours",
  "days",
  "months",
  "years",
};


char *tunitNamePtr(int unitID)
{
  char *name;
  int size = sizeof(Timeunits)/sizeof(char *);

  if ( unitID > 0 && unitID < size )
    name = Timeunits[unitID];
  else
    name = Timeunits[0];

  return (name);
}


void taxis_defaults(void)
{
  static char func[] = "taxis_defaults";
  char *timeunit;

  timeunit = getenv("TIMEUNIT");
  if ( timeunit )
    {
      if ( strcmp(timeunit, "minute") == 0 )
	DefaultTimeUnit = TUNIT_MINUTE;
      else if ( strcmp(timeunit, "hour") == 0 )
	DefaultTimeUnit = TUNIT_HOUR;
      else if ( strcmp(timeunit, "day") == 0 )
	DefaultTimeUnit = TUNIT_DAY;
      else if ( strcmp(timeunit, "month") == 0 )
	DefaultTimeUnit = TUNIT_MONTH;
      else if ( strcmp(timeunit, "year") == 0 )
	DefaultTimeUnit = TUNIT_YEAR;
      else
	Warning(func, "Unsupported TIMEUNIT %s!", timeunit);
    }
}


static int  TAXIS_Debug = 0;   /* If set to 1, debugging */

static int _taxis_max = MAX_TAXIS;

static void taxis_initialize(void);

static int _taxis_init = FALSE;

#if  defined  (HAVE_LIBPTHREAD)
#  include <pthread.h>

static pthread_once_t _taxis_init_thread = PTHREAD_ONCE_INIT;
static pthread_mutex_t _taxis_mutex;

#  define TAXIS_LOCK           pthread_mutex_lock(&_taxis_mutex);
#  define TAXIS_UNLOCK         pthread_mutex_unlock(&_taxis_mutex);
#  define TAXIS_INIT                               \
   if ( _taxis_init == FALSE ) pthread_once(&_taxis_init_thread, taxis_initialize);

#else

#  define TAXIS_LOCK
#  define TAXIS_UNLOCK
#  define TAXIS_INIT                               \
   if ( _taxis_init == FALSE ) taxis_initialize();

#endif


typedef struct _taxisPtrToIdx {
  int idx;
  TAXIS *ptr;
  struct _taxisPtrToIdx *next;
} taxisPtrToIdx;


static taxisPtrToIdx *_taxisList  = NULL;
static taxisPtrToIdx *_taxisAvail = NULL;


static void taxis_list_new(void)
{
  static char func[] = "taxis_list_new";

  assert(_taxisList == NULL);

  _taxisList = (taxisPtrToIdx *) malloc(_taxis_max*sizeof(taxisPtrToIdx));
}


static void taxis_list_delete(void)
{
  static char func[] = "taxis_list_delete";

  if ( _taxisList ) free(_taxisList);
}


static void taxis_init_pointer(void)
{
  int  i;
  
  for ( i = 0; i < _taxis_max; i++ )
    {
      _taxisList[i].next = _taxisList + i + 1;
      _taxisList[i].idx  = i;
      _taxisList[i].ptr  = 0;
    }

  _taxisList[_taxis_max-1].next = 0;

  _taxisAvail = _taxisList;
}


TAXIS *taxis_to_pointer(int idx)
{
  static char func[] = "taxis_to_pointer";
  TAXIS *taxisptr = NULL;

  TAXIS_INIT

  if ( idx >= 0 && idx < _taxis_max )
    {
      TAXIS_LOCK

      taxisptr = _taxisList[idx].ptr;

      TAXIS_UNLOCK
    }
  else
    Error(func, "taxis index %d undefined!", idx);

  return (taxisptr);
}


/* Create an index from a pointer */
static int taxis_from_pointer(TAXIS *ptr)
{
  static char func[] = "taxis_from_pointer";
  int      idx = -1;
  taxisPtrToIdx *newptr;

  if ( ptr )
    {
      TAXIS_LOCK

      if ( _taxisAvail )
	{
	  newptr       = _taxisAvail;
	  _taxisAvail   = _taxisAvail->next;
	  newptr->next = 0;
	  idx	       = newptr->idx;
	  newptr->ptr  = ptr;
      
	  if ( TAXIS_Debug )
	    Message(func, "Pointer %p has idx %d from taxis list", ptr, idx);
	}
      else
	Warning(func, "Too many open taxis (limit is %d)!", _taxis_max);

      TAXIS_UNLOCK
    }
  else
    Error(func, "Internal problem (pointer %p undefined)", ptr);

  return (idx);
}


void taxis_init_ptr(TAXIS *taxisptr)
{
  taxisptr->self       = -1;

  taxisptr->used       = FALSE;
  taxisptr->type       = DefaultTimeType;
  taxisptr->vdate      = 0;
  taxisptr->vtime      = 0;
  taxisptr->rdate      = CDI_UNDEFID;
  taxisptr->rtime      = 0;
  taxisptr->calendar   = DefaultCalendar;
  taxisptr->unit       = DefaultTimeUnit;
  taxisptr->numavg     = 0;
  taxisptr->has_bounds = FALSE;
  taxisptr->vdate_lb   = 0;
  taxisptr->vtime_lb   = 0;
  taxisptr->vdate_ub   = 0;
  taxisptr->vtime_ub   = 0;
}

void taxis_init_entry(TAXIS *taxisptr)
{
  taxis_init_ptr(taxisptr);

  taxisptr->self       = taxis_from_pointer(taxisptr);
}


static TAXIS *taxis_new_entry(void)
{
  static char func[] = "taxis_new_entry";
  TAXIS *taxisptr;

  taxisptr = (TAXIS *) malloc(sizeof(TAXIS));

  if ( taxisptr ) taxis_init_entry(taxisptr);

  return (taxisptr);
}


static void taxis_delete_entry(TAXIS *taxisptr)
{
  static char func[] = "taxis_delete_entry";
  int idx;

  idx = taxisptr->self;

  TAXIS_LOCK

  free(taxisptr);

  _taxisList[idx].next = _taxisAvail;
  _taxisList[idx].ptr  = 0;
  _taxisAvail          = &_taxisList[idx];

  TAXIS_UNLOCK

  if ( TAXIS_Debug )
    Message(func, "Removed idx %d from taxis list", idx);
}


static void taxis_initialize(void)
{
  char *env;

#if  defined  (HAVE_LIBPTHREAD)
  /* initialize global API mutex lock */
  pthread_mutex_init(&_taxis_mutex, NULL);
#endif

  env = getenv("TAXIS_DEBUG");
  if ( env ) TAXIS_Debug = atoi(env);

  taxis_list_new();
  atexit(taxis_list_delete);

  TAXIS_LOCK

  taxis_init_pointer();
  
  TAXIS_UNLOCK

  _taxis_init = TRUE;

  taxis_defaults();
}


static void taxis_copy(TAXIS *taxisptr2, TAXIS *taxisptr1)
{
  int taxisID2;

  taxisID2 = taxisptr2->self;
  memcpy(taxisptr2, taxisptr1, sizeof(TAXIS));
  taxisptr2->self = taxisID2;
}


static void taxis_check_ptr(const char *func, TAXIS *taxisptr)
{
  if ( taxisptr == NULL )
    Error(func, "taxis undefined!");
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
Here is an example using @func{taxisCreate} to create a relative T-axis
with a standard calendar.

@Source
#include "cdi.h"
   ...
int taxisID;
   ...
taxisID = taxisCreate(TAXIS_RELATIVE);
taxisDefCalendar(taxisID, CALENDAR_STANDARD);
taxisDefRdate(taxisID, 19870101);
taxisDefRtime(taxisID, 1200);
   ...
@EndSource
@EndFunction
*/
int taxisCreate(int taxistype)
{
  static char func[] = "taxisCreate";
  int taxisID;
  TAXIS *taxisptr;

  if ( CDI_Debug )
    Message(func, "taxistype: %d", taxistype);

  TAXIS_INIT

  taxisptr = taxis_new_entry();
  if ( ! taxisptr ) Error(func, "No memory");

  taxisID = taxisptr->self;
  taxisptr->type = taxistype;

  if ( CDI_Debug )
    Message(func, "taxisID: %d", taxisID);

  return (taxisID);
}


/*
@Function  taxisDestroy
@Title     Destroy a Time axis

@Prototype void taxisDestroy(int taxisID)
@Parameter
    @Item  taxisID  Time axis ID, from a previous call to @func{taxisCreate}

@EndFunction
*/
void taxisDestroy(int taxisID)
{
}


int taxisDuplicate(int taxisID1)
{
  static char func[] = "taxisDuplicate";
  int taxisID2;
  TAXIS *taxisptr1;
  TAXIS *taxisptr2;

  taxisptr1 = taxis_to_pointer(taxisID1);

  taxisptr2 = taxis_new_entry();
  if ( ! taxisptr2 ) Error(func, "No memory");

  taxisID2 = taxisptr2->self;

  if ( CDI_Debug )
    Message(func, "taxisID2: %d", taxisID2);

  ptaxisCopy(taxisptr2, taxisptr1);

  return (taxisID2);
}


void taxisDefType(int taxisID, int type)
{
  static char func[] = "taxisDefType";
  TAXIS *taxisptr;

  taxisptr = taxis_to_pointer(taxisID);

  taxis_check_ptr(func, taxisptr);

  taxisptr->type = type;
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
void taxisDefVdate(int taxisID, int vdate)
{
  static char func[] = "taxisDefVdate";
  TAXIS *taxisptr;

  taxisptr = taxis_to_pointer(taxisID);

  taxis_check_ptr(func, taxisptr);

  taxisptr->vdate = vdate;
}


/*
@Function  taxisDefVtime
@Title     Define the verification time

@Prototype void taxisDefVtime(int taxisID, int vtime)
@Parameter
    @Item  taxisID  Time axis ID, from a previous call to @fref{taxisCreate}
    @Item  vtime    Verification time (hhmm)

@Description
The function @func{taxisDefVtime} defines the verification time of a Time axis.

@EndFunction
*/
void taxisDefVtime(int taxisID, int vtime)
{
  static char func[] = "taxisDefVtime";
  TAXIS *taxisptr;

  taxisptr = taxis_to_pointer(taxisID);

  taxis_check_ptr(func, taxisptr);

  taxisptr->vtime = vtime;
}


/*
@Function  taxisDefRdate
@Title     Define the reference date

@Prototype void taxisDefRdate(int taxisID, int rdate)
@Parameter
    @Item  taxisID  Time axis ID, from a previous call to @fref{taxisCreate}
    @Item  rdate    Reference date (YYYYMMDD)

@Description
The function @func{taxisDefVdate} defines the reference date of a Time axis.

@EndFunction
*/
void taxisDefRdate(int taxisID, int rdate)
{
  static char func[] = "taxisDefRdate";
  TAXIS *taxisptr;

  taxisptr = taxis_to_pointer(taxisID);

  taxis_check_ptr(func, taxisptr);

  taxisptr->rdate = rdate;
}


/*
@Function  taxisDefRtime
@Title     Define the reference time

@Prototype void taxisDefRtime(int taxisID, int rtime)
@Parameter
    @Item  taxisID  Time axis ID, from a previous call to @fref{taxisCreate}
    @Item  rtime    Reference time (hhmm)

@Description
The function @func{taxisDefVdate} defines the reference time of a Time axis.

@EndFunction
*/
void taxisDefRtime(int taxisID, int rtime)
{
  static char func[] = "taxisDefRtime";
  TAXIS *taxisptr;

  taxisptr = taxis_to_pointer(taxisID);

  taxis_check_ptr(func, taxisptr);

  taxisptr->rtime = rtime;
}


/*
@Function  taxisDefCalendar
@Title     Define the calendar

@Prototype void taxisDefCalendar(int taxisID, int calendar)
@Parameter
    @Item  taxisID  Time axis ID, from a previous call to @fref{taxisCreate}
    @Item  calendar The type of the calendar, one of the set of predefined CDI calendar types.
                    The valid CDI calendar types are @func{CALENDAR_STANDARD}, @func{CALENDAR_360DAYS},
                    @func{CALENDAR_365DAYS} and @func{CALENDAR_366DAYS}.

@Description
The function @func{taxisDefCalendar} defines the calendar of a Time axis.

@EndFunction
*/
void taxisDefCalendar(int taxisID, int calendar)
{
  static char func[] = "taxisDefCalendar";
  TAXIS *taxisptr;

  taxisptr = taxis_to_pointer(taxisID);

  taxis_check_ptr(func, taxisptr);

  taxisptr->calendar = calendar;
}


void taxisDefTunit(int taxisID, int unit)
{
  static char func[] = "taxisDefTunit";
  TAXIS *taxisptr;

  taxisptr = taxis_to_pointer(taxisID);

  taxis_check_ptr(func, taxisptr);

  taxisptr->unit = unit;
}

void taxisDefNumavg(int taxisID, int numavg)
{
  static char func[] = "taxisDefNumavg";
  TAXIS *taxisptr;

  taxisptr = taxis_to_pointer(taxisID);

  taxis_check_ptr(func, taxisptr);

  taxisptr->numavg = numavg;
}


/*
The type of the time axis, one of the set of predefined CDI time types.
The valid CDI time types are TAXIS_ABSOLUTE and TAXIS_RELATIVE.
*/
int taxisInqType(int taxisID)
{
  static char func[] = "taxisInqType";
  TAXIS *taxisptr;

  taxisptr = taxis_to_pointer(taxisID);

  taxis_check_ptr(func, taxisptr);

  return (taxisptr->type);
}


void taxisCopyTimestep(int taxisID2, int taxisID1)
{
  static char func[] = "taxisCopyTimestep";
  TAXIS *taxisptr1;
  TAXIS *taxisptr2;

  taxisptr1 = taxis_to_pointer(taxisID1);
  taxisptr2 = taxis_to_pointer(taxisID2);

  taxis_check_ptr(func, taxisptr1);
  taxis_check_ptr(func, taxisptr2);

  taxisptr2->vdate = taxisptr1->vdate;
  taxisptr2->vtime = taxisptr1->vtime;

  if ( taxisptr2->has_bounds )
    {
      taxisptr2->vdate_lb = taxisptr1->vdate_lb;
      taxisptr2->vtime_lb = taxisptr1->vtime_lb;
      taxisptr2->vdate_ub = taxisptr1->vdate_ub;
      taxisptr2->vtime_ub = taxisptr1->vtime_ub;
    }
}


/*
@Function  taxisInqVdate
@Title     Get the verification date

@Prototype int taxisInqVdate(int taxisID)
@Parameter
    @Item  taxisID  Time axis ID, from a previous call to @fref{taxisCreate}

@Description
The function @func{taxisInqVdate} returns the verification date of a Time axis.

@Result
@func{taxisInqVdate} returns the verification date.

@EndFunction
*/
int taxisInqVdate(int taxisID)
{
  static char func[] = "taxisInqVdate";
  TAXIS *taxisptr;

  taxisptr = taxis_to_pointer(taxisID);

  taxis_check_ptr(func, taxisptr);

  return (taxisptr->vdate);
}


/*
@Function  taxisInqVtime
@Title     Get the verification time

@Prototype int taxisInqVtime(int taxisID)
@Parameter
    @Item  taxisID  Time axis ID, from a previous call to @fref{taxisCreate}

@Description
The function @func{taxisInqVtime} returns the verification time of a Time axis.

@Result
@func{taxisInqVtime} returns the verification time.

@EndFunction
*/
int taxisInqVtime(int taxisID)
{
  static char func[] = "taxisInqVtime";
  TAXIS *taxisptr;

  taxisptr = taxis_to_pointer(taxisID);

  taxis_check_ptr(func, taxisptr);

  return (taxisptr->vtime);
}


/*
@Function  taxisInqRdate
@Title     Get the reference date

@Prototype int taxisInqRdate(int taxisID)
@Parameter
    @Item  taxisID  Time axis ID, from a previous call to @fref{taxisCreate}

@Description
The function @func{taxisInqRdate} returns the reference date of a Time axis.

@Result
@func{taxisInqVdate} returns the reference date.

@EndFunction
*/
int taxisInqRdate(int taxisID)
{
  static char func[] = "taxisInqRdate";
  TAXIS *taxisptr;

  taxisptr = taxis_to_pointer(taxisID);

  taxis_check_ptr(func, taxisptr);

  if ( taxisptr->rdate == -1 )
    {
      taxisptr->rdate = taxisptr->vdate;
      taxisptr->rtime = taxisptr->vtime;
    }

  return (taxisptr->rdate);
}


/*
@Function  taxisInqRtime
@Title     Get the reference time

@Prototype int taxisInqRtime(int taxisID)
@Parameter
    @Item  taxisID  Time axis ID, from a previous call to @fref{taxisCreate}

@Description
The function @func{taxisInqRtime} returns the reference time of a Time axis.

@Result
@func{taxisInqVtime} returns the reference time.

@EndFunction
*/
int taxisInqRtime(int taxisID)
{
  static char func[] = "taxisInqRtime";
  TAXIS *taxisptr;

  taxisptr = taxis_to_pointer(taxisID);

  taxis_check_ptr(func, taxisptr);

  if ( taxisptr->rdate == -1 )
    {
      taxisptr->rdate = taxisptr->vdate;
      taxisptr->rtime = taxisptr->vtime;
    }

  return (taxisptr->rtime);
}


/*
@Function  taxisInqCalendar
@Title     Get the calendar

@Prototype int taxisInqCalendar(int taxisID)
@Parameter
    @Item  taxisID  Time axis ID, from a previous call to @fref{taxisCreate}

@Description
The function @func{taxisInqCalendar} returns the calendar of a Time axis.

@Result
@func{taxisInqCalendar} returns the type of the calendar,
one of the set of predefined CDI calendar types.
The valid CDI calendar types are @func{CALENDAR_STANDARD}, @func{CALENDAR_360DAYS},
@func{CALENDAR_365DAYS} and @func{CALENDAR_366DAYS}.

@EndFunction
*/
int taxisInqCalendar(int taxisID)
{
  static char func[] = "taxisInqCalendar";
  TAXIS *taxisptr;

  taxisptr = taxis_to_pointer(taxisID);

  taxis_check_ptr(func, taxisptr);

  return (taxisptr->calendar);
}


int taxisInqTunit(int taxisID)
{
  static char func[] = "taxisInqTunit";
  TAXIS *taxisptr;

  taxisptr = taxis_to_pointer(taxisID);

  taxis_check_ptr(func, taxisptr);

  return (taxisptr->unit);
}


int taxisInqNumavg(int taxisID)
{
  static char func[] = "taxisInqNumavg";
  TAXIS *taxisptr;

  taxisptr = taxis_to_pointer(taxisID);

  taxis_check_ptr(func, taxisptr);

  return (taxisptr->numavg);
}


TAXIS *taxisPtr(int taxisID)
{
  static char func[] = "taxisPtr";
  TAXIS *taxisptr;

  taxisptr = taxis_to_pointer(taxisID);

  taxis_check_ptr(func, taxisptr);

  return (taxisptr);
}


void timeval2vtime(double timevalue, TAXIS *taxis, int *vdate, int *vtime)
{
  static char func[] = "timeval2vtime";
  int year, month, day, hour, minute;
  int rdate, rtime;
  sUnit tunit = {0, 1};
  double value, factor = 1;
  int timeunit;
  int calendar, dpy = 0;
  double second = 0;
  static int lwarn = TRUE;

  *vdate = 0;
  *vtime = 0;

  timeunit = (*taxis).unit;
  calendar = (*taxis).calendar;

  if      ( calendar == CALENDAR_360DAYS )   dpy = 360;
  else if ( calendar == CALENDAR_365DAYS )   dpy = 365;
  else if ( calendar == CALENDAR_366DAYS )   dpy = 366;
  else if ( calendar == CALENDAR_PROLEPTIC ) dpy = -1;

  rdate  = (*taxis).rdate;
  rtime  = (*taxis).rtime;
  year   =  rdate / 10000;
  month  = (rdate - year*10000) / 100;
  day    =  rdate - year*10000 - month*100;
  hour   =  rtime / 100;
  minute =  rtime - hour*100;

  if ( timeunit == TUNIT_MONTH || timeunit == TUNIT_YEAR )
    {
      if ( timeunit == TUNIT_YEAR ) timevalue *= 12;

      month += NINT(timevalue);

      while ( month > 12 ) { month -= 12; year++; }
      while ( month <  1 ) { month += 12; year--; }
    }
  else
    {
      switch ( timeunit )
	{
	case TUNIT_SECOND: factor =     1.0; break;
	case TUNIT_MINUTE: factor =    60.0; break;
	case TUNIT_HOUR:   factor =  3600.0; break;
	case TUNIT_DAY:    factor = 86400.0; break;
	default:
	  if ( lwarn )
	    {
	      Warning(func, "timeunit %s unsupported!", tunitNamePtr(timeunit));
	      lwarn = FALSE;
	    }
	}

      /* attention: used offset of 1 second to prevent rounding errors */
      cdiInvCalendar(dpy, year, month, day, hour, minute, 1.0, tunit, &value);

      if ( timeunit == TUNIT_MONTH || timeunit == TUNIT_YEAR )
	timevalue = value;
      else
	tunit.origin = value;

      tunit.factor = factor;

      cdiCalendar(dpy, timevalue, tunit, &year, &month, &day, &hour, &minute, &second);
    }

  *vdate = year*10000 + month*100 + day;
  *vtime = hour*100 + minute /* + (int) (second/60+0.5)*/;
}


double vtime2timeval(int vdate, int vtime, TAXIS *taxis)
{
  static char func[] = "vtime2timeval";
  int ryear, rmonth;
  int year, month, day, hour, minute;
  int rdate, rtime;
  sUnit tunit = {0, 1};
  double value;
  int timeunit;
  int calendar, dpy = 0;
  static int lwarn = TRUE;

  timeunit = (*taxis).unit;
  calendar = (*taxis).calendar;

  if      ( calendar == CALENDAR_360DAYS ) dpy = 360;
  else if ( calendar == CALENDAR_365DAYS ) dpy = 365;
  else if ( calendar == CALENDAR_366DAYS ) dpy = 366;
  else if ( calendar == CALENDAR_PROLEPTIC ) dpy = -1;

  rdate  = (*taxis).rdate;
  rtime  = (*taxis).rtime;
  if ( rdate == -1 )
    {
      rdate  = (*taxis).vdate;
      rtime  = (*taxis).vtime;
    }

  ryear  =  rdate / 10000;
  rmonth = (rdate - ryear*10000) / 100;
  day    =  rdate - ryear*10000 - rmonth*100;
  hour   =  rtime / 100;
  minute =  rtime - hour*100;

  cdiInvCalendar(dpy, ryear, rmonth, day, hour, minute, 0.0, tunit, &value);
  tunit.origin = value;
  tunit.factor = 60.0;
      
  year   =  vdate / 10000;
  month  = (vdate - year*10000) / 100;
  day    =  vdate - year*10000 - month*100;
  hour   =  vtime / 100;
  minute =  vtime - hour*100;

  if ( timeunit == TUNIT_MONTH || timeunit == TUNIT_YEAR )
    {
      value = (year-ryear)*12 - rmonth + month;
      if ( timeunit == TUNIT_YEAR ) value = NINT(value/12);
    }
  else
    {
      switch ( timeunit )
	{
	case TUNIT_SECOND: tunit.factor =      1.0; break;
	case TUNIT_MINUTE: tunit.factor =     60.0; break;
	case TUNIT_HOUR:   tunit.factor =   3600.0; break;
	case TUNIT_DAY:    tunit.factor =  86400.0; break;
	default:
	  if ( lwarn )
	    {
	      Warning(func, "timeunit %s unsupported!", tunitNamePtr(timeunit));
	      lwarn = FALSE;
	    }
	}

      cdiInvCalendar(dpy, year, month, day, hour, minute, 0.0, tunit, &value);
    }

  return (value);
}


void splitTimevalue(double timevalue, int timeunit, int *date, int *time)
{
  static char func[] = "splitTimevalue";
  int vdate = 0, vtime = 0;
  int hour, minute;

  if ( timeunit == TUNIT_HOUR )
    {
      timevalue /= 24;
      vdate = (int) timevalue;
      vtime = (int) ((timevalue - vdate)*1440 + 0.01);

      hour   = vtime / 60;
      minute = vtime - hour*60;
      vtime  = hour*100 + minute;
    }
  else if ( timeunit == TUNIT_DAY )
    {
      vdate = (int) timevalue;
      vtime = (int) ((timevalue - vdate)*1440 + 0.01);

      hour   = vtime / 60;
      minute = vtime - hour*60;
      vtime  = hour*100 + minute;
    }
  else if ( timeunit == TUNIT_MONTH )
    {
      vdate = (int) timevalue*100;
      vtime = 0;
    }
  else
    {
      Message(func, "Unsupported TIMEUNIT: %s!", tunitNamePtr(timeunit));
    }

  *date = vdate;
  *time = vtime;
}


void decode_timeval(double timevalue, TAXIS *taxis, int *date, int *time)
{
  if ( taxis->type == TAXIS_ABSOLUTE )
    splitTimevalue(timevalue, taxis->unit, date, time);
  else
    timeval2vtime(timevalue, taxis, date, time);
}


double encode_timeval(int date, int time, TAXIS *taxis)
{
  double timevalue;

  if ( taxis->type == TAXIS_ABSOLUTE )
    {
      if ( taxis->unit == TUNIT_MONTH )
	{
	  timevalue = date/100 + 0.5;
	}
      else
	{
	  int hour   = time / 100;
	  int minute = time - hour*100;
	  timevalue = date + (hour*60 + minute)/1440.;
	}
    }
  else
    timevalue = vtime2timeval(date, time, taxis);

  return (timevalue);
}


void ptaxisInit(TAXIS *taxisptr)
{
  taxis_init_ptr(taxisptr);
}


void ptaxisCopy(TAXIS *dest, TAXIS *source)
{
  memcpy(dest, source, sizeof(TAXIS));
}
