#ifndef _TAXIS_H
#define _TAXIS_H


typedef struct {
  int     self;
  int     used;
  int     type;           /* time type             */
  int     vdate;          /* verification date     */
  int     vtime;          /* verification time     */
  int     rdate;          /* reference date        */
  int     rtime;          /* reference time        */
  int     calendar;
  int     unit;           /* time unit             */
  int     numavg;
  int     has_bounds;
  int     vdate_lb;       /* lower bounds of vdate */
  int     vtime_lb;       /* lower bounds of vtime */
  int     vdate_ub;       /* upper bounds of vdate */
  int     vtime_ub;       /* upper bounds of vtime */
}
TAXIS;

void    ptaxisInit(TAXIS *taxis);
void    ptaxisCopy(TAXIS *dest, TAXIS *source);
TAXIS  *taxisPtr(int timeID);
void    decode_timeval(double timevalue, TAXIS *taxis, int *date, int *time);
double  encode_timeval(int date, int time, TAXIS *taxis);
void    timeval2vtime(double timevalue, TAXIS *taxis, int *vdate, int *vtime);
double  vtime2timeval(int vdate, int vtime, TAXIS *taxis);


#endif  /* _TAXIS_H */
