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

static int  TimeTableInit = 0;

TAXIS *taxisTable;
int    taxisTableSize = 0;

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


void taxisTableInit(void)
{
  static char func[] = "taxisTableInit";
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

  TimeTableInit = 1;
}


void ptaxisInit(TAXIS *taxis)
{
  if ( ! TimeTableInit ) taxisTableInit();

  taxis->used       = FALSE;
  taxis->type       = DefaultTimeType;
  taxis->vdate      = 0;
  taxis->vtime      = 0;
  taxis->rdate      = CDI_UNDEFID;
  taxis->rtime      = 0;
  taxis->calendar   = DefaultCalendar;
  taxis->unit       = DefaultTimeUnit;
  taxis->numavg     = 0;
  taxis->has_bounds = FALSE;
  taxis->vdate_lb   = 0;
  taxis->vtime_lb   = 0;
  taxis->vdate_ub   = 0;
  taxis->vtime_ub   = 0;
}


void ptaxisCopy(TAXIS *dest, TAXIS *source)
{
  memcpy(dest, source, sizeof(TAXIS));
}


int taxisTableNewEntry(int timetype)
{
  static char func[] = "taxisTableNewEntry";
  int taxisID = 0;

  if ( ! TimeTableInit ) taxisTableInit();

  /*
    Look for a free slot in taxisTable.
    (Create the table the first time through).
  */
  if ( ! taxisTableSize )
    {
      int i;

      taxisTableSize = 2;
      taxisTable = (TAXIS *) malloc(taxisTableSize*sizeof(TAXIS));
      if ( taxisTable == NULL )
	{
          Message(func, "taxisTableSize = %d", taxisTableSize);
	  SysError(func, "Allocation of TAXISTABLE failed");
	}

      for ( i = 0; i < taxisTableSize; i++ )
	{
	  taxisTable[i].used = FALSE;
	  ptaxisInit(&taxisTable[i]);
	}
    }
  else
    {
      while ( taxisID < taxisTableSize )
	{
	  if ( ! taxisTable[taxisID].used ) break;
	  taxisID++;
	}
    }
  /*
    If the table overflows, double its size.
  */
  if ( taxisID == taxisTableSize )
    {
      int i;

      taxisTableSize = 2*taxisTableSize;
      taxisTable = (TAXIS *) realloc(taxisTable, taxisTableSize*sizeof(TAXIS));
      if ( taxisTable == NULL )
	{
          Message(func, "taxisTableSize = %d", taxisTableSize);
	  SysError(func, "Reallocation of TAXISTABLE failed");
	}

      taxisID = taxisTableSize/2;

      for ( i = taxisID; i < taxisTableSize; i++ )
	{
	  taxisTable[i].used = FALSE;
	  ptaxisInit(&taxisTable[i]);
	}
    }

  taxisTable[taxisID].type = timetype;
  taxisTable[taxisID].used = TRUE;

  return (taxisID);
}


static void taxisCheckID(const char *func, int taxisID)
{
  if ( taxisID == CDI_UNDEFID )
    Error(func, "taxisID undefined!");

  if ( taxisID < 0 || taxisID >= taxisTableSize )
    Error(func, "taxisID %d undefined!", taxisID);

  if ( taxisTable[taxisID].type == CDI_UNDEFID )
    Error(func, "taxisID %d type undefined!", taxisID);
}


/*
@Function  taxisCreate
@Title     Create a Time axis

@Prototype int taxisCreate(int taxistype)
@Parameter
    @Item  taxistype  The type of the time axis, one of the set of predefined CDI time types.
                      The valid CDI time types are {\tt TAXIS_ABSOLUTE} and {\tt TAXIS_RELATIVE}.

@Description
The function {\tt taxisCreate} creates a Time axis.

@Result
{\tt taxisCreate} returns an identifier to the Time axis.

@EndFunction
*/
int taxisCreate(int taxistype)
{
  static char func[] = "taxisCreate";
  int taxisID;

  if ( CDI_Debug )
    Message(func, "taxistype: %d", taxistype);

  /* get next free time entry */
  taxisID = taxisTableNewEntry(taxistype);

  if ( CDI_Debug )
    Message(func, "taxisID: %d", taxisID);

  return (taxisID);
}


/*
@Function  taxisDestroy
@Title     Destroy a Time axis

@Prototype void taxisDestroy(int taxisID)
@Parameter
    @Item  taxisID Identifier, from a previous call to {\tt taxisCreate}

@EndFunction
*/
void taxisDestroy(int taxisID)
{
}


int taxisDuplicate(int taxisID1)
{
  static char func[] = "taxisDuplicate";
  int taxisID2;
  /* get next free time entry */
  taxisID2 = taxisTableNewEntry(taxisInqType(taxisID1));

  if ( CDI_Debug )
    Message(func, "taxisID: %d", taxisID2);

  ptaxisCopy(taxisPtr(taxisID2), taxisPtr(taxisID1));

  return (taxisID2);
}


void taxisDefType(int taxisID, int type)
{
  static char func[] = "taxisDefType";

  taxisCheckID(func, taxisID);

  taxisTable[taxisID].type = type;
}


/*
@Function  taxisDefVdate
@Title     Define the verification date

@Prototype void taxisDefVdate(int taxisID, int vdate)
@Parameter
    @Item  taxisID  T-axis identifier
    @Item  vdate    Verification date (YYYYMMDD)

@Description
The function {\tt taxisDefVdate} defines the verification date of a Time axis.

@EndFunction
*/
void taxisDefVdate(int taxisID, int vdate)
{
  static char func[] = "taxisDefVdate";

  taxisCheckID(func, taxisID);

  taxisTable[taxisID].vdate = vdate;
}


/*
@Function  taxisDefVtime
@Title     Define the verification time

@Prototype void taxisDefVtime(int taxisID, int vtime)
@Parameter
    @Item  taxisID  T-axis identifier
    @Item  vtime    Verification time (HHMM)

@Description
The function {\tt taxisDefVtime} defines the verification time of a Time axis.

@EndFunction
*/
void taxisDefVtime(int taxisID, int vtime)
{
  static char func[] = "taxisDefVtime";

  taxisCheckID(func, taxisID);

  taxisTable[taxisID].vtime = vtime;
}


/*
@Function  taxisDefRdate
@Title     Define the reference date

@Prototype void taxisDefRdate(int taxisID, int rdate)
@Parameter
    @Item  taxisID  T-axis identifier
    @Item  rdate    Reference date (YYYYMMDD)

@Description
The function {\tt taxisDefVdate} defines the reference date of a Time axis.

@EndFunction
*/
void taxisDefRdate(int taxisID, int rdate)
{
  static char func[] = "taxisDefRdate";

  taxisCheckID(func, taxisID);

  taxisTable[taxisID].rdate = rdate;
}


/*
@Function  taxisDefRtime
@Title     Define the reference time

@Prototype void taxisDefRtime(int taxisID, int rtime)
@Parameter
    @Item  taxisID  T-axis identifier
    @Item  rtime    Reference time (HHMM)

@Description
The function {\tt taxisDefVdate} defines the reference time of a Time axis.

@EndFunction
*/
void taxisDefRtime(int taxisID, int rtime)
{
  static char func[] = "taxisDefRtime";

  taxisCheckID(func, taxisID);

  taxisTable[taxisID].rtime = rtime;
}


/*
@Function  taxisDefCalendar
@Title     Define the calendar

@Prototype void taxisDefCalendar(int taxisID, int calendar)
@Parameter
    @Item  taxisID   T-axis identifier
    @Item  calendar  The type of the calendar, one of the set of predefined CDI calendar types.
                     The valid CDI calendar types are {\tt CALENDAR_STANDARD}, {\tt CALENDAR_360DAYS},
                     {\tt CALENDAR_365DAYS} and {\tt CALENDAR_366DAYS}.

@Description
The function {\tt taxisDefCalendar} defines the calendar of a Time axis.

@EndFunction
*/
void taxisDefCalendar(int taxisID, int calendar)
{
  static char func[] = "taxisDefCalendar";

  taxisCheckID(func, taxisID);

  taxisTable[taxisID].calendar = calendar;
}


void taxisDefTunit(int taxisID, int unit)
{
  static char func[] = "taxisDefTunit";

  taxisCheckID(func, taxisID);

  taxisTable[taxisID].unit = unit;
}

void taxisDefNumavg(int taxisID, int numavg)
{
  static char func[] = "taxisDefNumavg";

  taxisCheckID(func, taxisID);

  taxisTable[taxisID].numavg = numavg;
}


/*
The type of the time axis, one of the set of predefined CDI time types.
The valid CDI time types are TAXIS_ABSOLUTE and TAXIS_RELATIVE.
*/
int taxisInqType(int taxisID)
{
  static char func[] = "taxisInqType";

  taxisCheckID(func, taxisID);

  return (taxisTable[taxisID].type);
}


void taxisCopyTimestep(int taxisIDdes, int taxisIDsrc)
{
  static char func[] = "taxisCopyTimestep";

  taxisCheckID(func, taxisIDsrc);
  taxisCheckID(func, taxisIDdes);

  taxisTable[taxisIDdes].vdate = taxisTable[taxisIDsrc].vdate;
  taxisTable[taxisIDdes].vtime = taxisTable[taxisIDsrc].vtime;

  if ( taxisTable[taxisIDdes].has_bounds )
    {
      taxisTable[taxisIDdes].vdate_lb = taxisTable[taxisIDsrc].vdate_lb;
      taxisTable[taxisIDdes].vtime_lb = taxisTable[taxisIDsrc].vtime_lb;
      taxisTable[taxisIDdes].vdate_ub = taxisTable[taxisIDsrc].vdate_ub;
      taxisTable[taxisIDdes].vtime_ub = taxisTable[taxisIDsrc].vtime_ub;
    }
}


/*
@Function  taxisInqVdate
@Title     Get the verification date

@Prototype int taxisInqVdate(int taxisID)
@Parameter
    @Item  taxisID  T-axis identifier

@Description
The function {\tt taxisInqVdate} returns the verification date of a Time axis.

@Result
{\tt taxisInqVdate} returns the verification date.

@EndFunction
*/
int taxisInqVdate(int taxisID)
{
  static char func[] = "taxisInqVdate";

  taxisCheckID(func, taxisID);

  return (taxisTable[taxisID].vdate);
}


/*
@Function  taxisInqVtime
@Title     Get the verification time

@Prototype int taxisInqVtime(int taxisID)
@Parameter
    @Item  taxisID  T-axis identifier

@Description
The function {\tt taxisInqVtime} returns the verification time of a Time axis.

@Result
{\tt taxisInqVtime} returns the verification time.

@EndFunction
*/
int taxisInqVtime(int taxisID)
{
  static char func[] = "taxisInqVtime";

  taxisCheckID(func, taxisID);

  return (taxisTable[taxisID].vtime);
}


/*
@Function  taxisInqRdate
@Title     Get the reference date

@Prototype int taxisInqRdate(int taxisID)
@Parameter
    @Item  taxisID  T-axis identifier

@Description
The function {\tt taxisInqRdate} returns the reference date of a Time axis.

@Result
{\tt taxisInqVdate} returns the reference date.

@EndFunction
*/
int taxisInqRdate(int taxisID)
{
  static char func[] = "taxisInqRdate";

  taxisCheckID(func, taxisID);

  if ( taxisTable[taxisID].rdate == -1 )
    {
      taxisTable[taxisID].rdate = taxisTable[taxisID].vdate;
      taxisTable[taxisID].rtime = taxisTable[taxisID].vtime;
    }

  return (taxisTable[taxisID].rdate);
}


/*
@Function  taxisInqRtime
@Title     Get the reference time

@Prototype int taxisInqRtime(int taxisID)
@Parameter
    @Item  taxisID  T-axis identifier

@Description
The function {\tt taxisInqRtime} returns the reference time of a Time axis.

@Result
{\tt taxisInqVtime} returns the reference time.

@EndFunction
*/
int taxisInqRtime(int taxisID)
{
  static char func[] = "taxisInqRtime";

  taxisCheckID(func, taxisID);

  if ( taxisTable[taxisID].rdate == -1 )
    {
      taxisTable[taxisID].rdate = taxisTable[taxisID].vdate;
      taxisTable[taxisID].rtime = taxisTable[taxisID].vtime;
    }

  return (taxisTable[taxisID].rtime);
}


/*
@Function  taxisInqCalendar
@Title     Get the calendar

@Prototype int taxisInqCalendar(int taxisID)
@Parameter
    @Item  taxisID  T-axis identifier

@Description
The function {\tt taxisInqCalendar} returns the calendar of a Time axis.

@Result
{\tt taxisInqCalendar} returns the type of the calendar,
one of the set of predefined CDI calendar types.
The valid CDI calendar types are {\tt CALENDAR_STANDARD}, {\tt CALENDAR_360DAYS},
{\tt CALENDAR_365DAYS} and {\tt CALENDAR_366DAYS}.

@EndFunction
*/
int taxisInqCalendar(int taxisID)
{
  static char func[] = "taxisInqCalendar";

  taxisCheckID(func, taxisID);

  return (taxisTable[taxisID].calendar);
}


int taxisInqTunit(int taxisID)
{
  static char func[] = "taxisInqTunit";

  taxisCheckID(func, taxisID);

  return (taxisTable[taxisID].unit);
}


int taxisInqNumavg(int taxisID)
{
  static char func[] = "taxisInqNumavg";

  taxisCheckID(func, taxisID);

  return (taxisTable[taxisID].numavg);
}


TAXIS *taxisPtr(int taxisID)
{
  static char func[] = "taxisPtr";

  taxisCheckID(func, taxisID);

  return (&taxisTable[taxisID]);
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
  double second;
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
  *vtime = hour*100 + minute;
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
      Message(func, "Unsupported TIMEUNIT %s!", tunitNamePtr(timeunit));
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
