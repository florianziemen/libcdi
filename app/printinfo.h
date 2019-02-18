#ifndef PRINTINFO_H
#define PRINTINFO_H

#include <stdio.h>

#define DATE_FORMAT "%5.4d-%2.2d-%2.2d"
#define TIME_FORMAT "%2.2d:%2.2d:%2.2d"

void datetime2str(int64_t date, int time, char *datetimestr, int maxlen);

void date2str(int64_t date, char *datestr, int maxlen);

void time2str(int time, char *timestr, int maxlen);

const char *comp_name(int comptype);

void printFiletype(int streamID, int vlistID);

void printGridInfo(int vlistID);

void printZaxisInfo(int vlistID);

void printSubtypeInfo(int vlistID);

void printTimesteps(int streamID, int taxisID, int verbose);

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
