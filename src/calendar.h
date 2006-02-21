#ifndef _CALENDAR_H
#define _CALENDAR_H

typedef struct {
  double  origin;  /* origin datatype */
  double  factor;  /* scaling factor  */
}
sUnit;

void Calendar(double value, sUnit unit, int *year, int *month,
	      int *day, int *hour, int *minute, double *second);
void InvCalendar(int year, int month, int day, int hour,
		 int minute, double second, sUnit unit, double *value);

void cdiCalendar(int dpy, double value, sUnit unit, int *year, int *month,
		 int *day, int *hour, int *minute, double *second);
void cdiInvCalendar(int dpy, int year, int month, int day, int hour,
		    int minute, double second, sUnit unit, double *value);

#endif  /* _CALENDAR_H */
