#ifndef DATETIME_H
#define DATETIME_H

typedef struct
{
  int date;
  int time;
} CmpDateTime;

static inline int
datetime_differ(CmpDateTime dt1, CmpDateTime dt2)
{
  return ((2 * ((dt1.date > dt2.date) - (dt1.date < dt2.date)) + (dt1.time > dt2.time) - (dt1.time < dt2.time)) != 0);
}

#endif
/*
 * Local Variables:
 * c-file-style: "Java"
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * show-trailing-whitespace: t
 * require-trailing-newline: t
 * End:
 */
