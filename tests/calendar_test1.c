#include <stdio.h>
#include <stdlib.h>

#include "cdi.h"
#include "calendar.h"
#include "timebase.h"

static int
date_to_calday(int calendar, int date)
{
  const int dpy = calendar_dpy(calendar);

  int year, month, day;
  cdiDecodeDate(date, &year, &month, &day);

  int calday;
  if (dpy == 360 || dpy == 365 || dpy == 366)
    calday = cdi_encode_day(dpy, year, month, day);
  else
    calday = encode_julday(calendar, year, month, day);

  return calday;
}

static int
calday_to_date(int calendar, int calday)
{
  int year, month, day;
  const int dpy = calendar_dpy(calendar);

  if (dpy == 360 || dpy == 365 || dpy == 366)
    cdi_decode_day(dpy, calday, &year, &month, &day);
  else
    decode_julday(calendar, calday, &year, &month, &day);

  const int date = cdiEncodeDate(year, month, day);

  return date;
}

int
main(void)
{
  int calendar = CALENDAR_STANDARD;
  int nmin;
  int64_t vdate0, vdate;
  int vtime0, vtime;
  int ijulinc;
  int j = 0;
  int year, mon, day, hour, minute, second;
  int64_t calday;
  int secofday;

  /* 1 - Check valid range of years */

  nmin = 11000;
  vdate0 = -80001201;
  vtime0 = 120500;

  /* printf("start time: %8ld %4d\n", vdate0, vtime0); */

  for (int i = 0; i < nmin; i++)
    {
      cdiDecodeDate(vdate0, &year, &mon, &day);
      cdiDecodeTime(vtime0, &hour, &minute, &second);

      calday = date_to_calday(calendar, vdate0);
      secofday = time_to_sec(vtime0);

      vdate = calday_to_date(calendar, calday);
      vtime = sec_to_time(secofday);

      if (vdate0 != vdate || vtime0 != vtime)
        fprintf(stderr, "%4d %8ld %4d %8ld %4d %9ld %9d\n", ++j, vdate0, vtime0, vdate, vtime, calday, secofday);

      year++;
      vdate0 = cdiEncodeDate(year, mon, day);
      vtime0 = cdiEncodeTime(hour, minute, second);
    }

  /* printf("stop time: %8ld %4d\n", vdate0, vtime0); */

  /* 2 - Check time increment of one minute */

  nmin = 120000;
  ijulinc = 60;
  vdate0 = 20001201;
  vtime0 = 0;

  /* printf("start time: %8ld %4d\n", vdate0, vtime0); */

  calday = date_to_calday(calendar, vdate0);
  secofday = time_to_sec(vtime0);
  for (int i = 0; i < nmin; i++)
    {
      cdiDecodeDate(vdate0, &year, &mon, &day);
      cdiDecodeTime(vtime0, &hour, &minute, &second);

      if (++minute >= 60)
        {
          minute = 0;
          if (++hour >= 24)
            {
              hour = 0;
              if (++day >= 32)
                {
                  day = 1;
                  if (++mon >= 13)
                    {
                      mon = 1;
                      year++;
                    }
                }
            }
        }

      vdate0 = cdiEncodeDate(year, mon, day);
      vtime0 = cdiEncodeTime(hour, minute, second);

      julday_add_seconds(ijulinc, &calday, &secofday);

      vdate = calday_to_date(calendar, calday);
      vtime = sec_to_time(secofday);
      if (vdate0 != vdate || vtime0 != vtime)
        fprintf(stderr, "%4d %8ld %4d %8ld %4d %9ld %9d\n", ++j, vdate0, vtime0, vdate, vtime, calday, secofday);
    }

  /* printf("stop time: %8ld %4d\n", vdate0, vtime0); */

  return j == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
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
