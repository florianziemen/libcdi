#ifndef _VLIST_H
#define _VLIST_H

#ifndef _CDI_LIMITS_H
#  include "cdi_limits.h"
#endif

#define  MAX_ATTRIBUTES  256


typedef struct
{
  int      flag;
  int      index;
  int      flevelID;
}
LEVINFO;


typedef struct
{
  int      flag;
  int      nlevs;
  int      isUsed;
  int      fvarID;
  int      code;
  int      gridID;
  int      zaxisID;
  int      timeID;    /* ( TIME_VARIABLE or TIME_CONSTANT ) */
  int      datatype;  /* DATATYPE_PACKX for GRIB data, else DATATYPE_FLT32 or DATATYPE_FLT64 */
  int      instID;
  int      modelID;
  int      tableID;
  int      average;
  int      func;
  int      missvalused;  /* TRUE if missval is defined */
  char    *name;
  char    *longname;
  char    *stdname;
  char    *units;
  double   missval;
  double   scalefactor;
  double   addoffset;
  LEVINFO *levinfo;
  int      szip;
}
VARINFO;


typedef struct
{
  int      self;
  int      used;
  int      nlock;
  int      nvars;        /* number of variables                */
  int      ngrids;
  int      nzaxis;
  int      ntsteps;
  int      taxisID;
  int      tableID;
  int      instID;
  int      modelID;
  int      varsAllocated;
  int      gridIDs[MAX_GRIDS_PS];
  int      zaxisIDs[MAX_ZAXIS_PS];
  VARINFO *vars;
  int      natts;
  char    *attnames[MAX_ATTRIBUTES];
  char    *attstrings[MAX_ATTRIBUTES];
  int      niatts;
  char    *iattnames[MAX_ATTRIBUTES];
  int      iatts[MAX_ATTRIBUTES];
}
VLIST;


VLIST  *vlist_to_pointer(int vlistID);
int     vlistNlock(int vlistID);
void    vlistLock(int vlistID);
void    vlistUnlock(int vlistID);
char   *vlistInqVarNamePtr(int vlistID, int varID);
char   *vlistInqVarLongnamePtr(int vlistID, int varID);
char   *vlistInqVarStdnamePtr(int vlistID, int varID);
char   *vlistInqVarUnitsPtr(int vlistID, int varID);
void    vlistDestroyVarName(int vlistID, int varID);
void    vlistDestroyVarLongname(int vlistID, int varID);
void    vlistDestroyVarUnits(int vlistID, int varID);
void    vlistDefVarTime(int vlistID, int varID, int timeID);
int     vlistInqVarMissvalUsed(int vlistID, int varID);
int     vlistHasTime(int vlistID);

void vlistDefIntAttribute(int vlistID, const char *attname, int attint);

#endif  /* _VLIST_H */
