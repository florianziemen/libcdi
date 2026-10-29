#ifndef PRINTINFO_H
#define PRINTINFO_H

#include <cdi.h>

// clang-format off
#ifdef __cplusplus
extern "C"
{
#endif

void datetime2str(CdiDateTime dt, char *datetimestr, int maxlen);
void date2str(CdiDate date, char *datestr, int maxlen);
void time2str(CdiTime time, char *timestr, int maxlen);
const char *comptype_to_name(int comptype);
void printFiletype(int streamID, int vlistID);
void print_grid_info(int vlistID);
void print_zaxis_info(int vlistID);
void printSubtypeInfo(int vlistID);
void printTimesteps(int streamID, int taxisID, int verbose);

#ifdef __cplusplus
}
#endif
// clang-format on

#endif
