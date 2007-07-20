#ifndef _BASETIME_H
#define _BASETIME_H


typedef struct {
  int   ncvarid;
  int   ncdimid;
  int   ncvarboundsid;
}
BaseTime;

void basetimeInit(BaseTime *basetime);

#endif  /* _BASETIME_H */
