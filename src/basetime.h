#ifndef _BASETIME_H
#define _BASETIME_H

#ifndef _CALENDAR_H
#include "calendar.h"
#endif


typedef struct {
  int   ncvarid;
  int   ncdimid;
  int   ncvarboundsid;
}
BaseTime;

void basetimeInit(BaseTime *basetime);

#endif  /* _BASETIME_H */
