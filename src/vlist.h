#ifndef _VLIST_H
#define _VLIST_H

#ifndef _CDI_LIMITS_H
#  include "cdi_limits.h"
#endif

#define  MAX_ATTRIBUTES  256


/*
 * CDI attribute
 */
typedef struct {
  size_t xsz;		/* amount of space at xvalue             */
  size_t namesz;        /* size of name                          */
  char  *name;          /* attribute name                        */
  int    type;		/* data type of xvalue (INT, FLT or TXT) */
  int    nctype;
  size_t nelems;     	/* number of elements                    */
  void  *xvalue;      	/* the actual data                       */
} CDI_attr;


typedef struct {
  size_t nalloc;		/* number allocated >= nelems */
  size_t nelems;		/* length of the array */
  CDI_attr value[MAX_ATTRIBUTES];
} CDI_atts;


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
  CDI_atts atts;
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
  CDI_atts atts;
}
VLIST;


VLIST  *vlist_to_pointer(int vlistID);
void    vlistCheckVarID(char *func, int vlistID, int varID);
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

int     vlistDelAtts(int vlistID, int varID);
int     vlistCopyVarAtts(int vlistID1, int varID_1, int vlistID2, int varID_2);

#endif  /* _VLIST_H */
