#ifndef _VLIST_H
#define _VLIST_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stddef.h>  /* size_t */

#ifdef USE_MPI
#include <mpi.h>
#endif

#ifndef _CDI_LIMITS_H
#  include "cdi_limits.h"
#endif

#define VALIDMISS 1.e+303

/*
 * CDI attribute
 */
typedef struct {
  size_t    xsz;	  /* amount of space at xvalue                      */
  size_t    namesz;       /* size of name                                   */
  char     *name;         /* attribute name                                 */
  int       indtype;	  /* internal data type of xvalue (INT, FLT or TXT) */
  int       exdtype;      /* external data type                             */
                          /* indtype    exdtype                             */
                          /* TXT        TXT                                 */
                          /* INT        INT16, INT32                        */
                          /* FLT        FLT32, FLT64                        */
  size_t    nelems;    	  /* number of elements                             */
  void     *xvalue;       /* the actual data                                */
} cdi_att_t;


typedef struct {
  size_t     nalloc;		/* number allocated >= nelems */
  size_t     nelems;		/* length of the array */
  cdi_att_t  value[MAX_ATTRIBUTES];
} cdi_atts_t;


typedef struct
{
  int      flag;
  int      index;
  int      mlevelID;
  int      flevelID;
}
levinfo_t;


typedef struct
{
  int      rank;
  int      offset;
  int      chunk;
}
deco_t;

typedef struct
{
  int ens_index;
  int ens_count;
  int forecast_init_type;
}
ensinfo_t;


typedef struct
{
  int         flag;
  int         nlevs;
  int         isUsed;
  int         mvarID;
  int         fvarID;
  int         param;
  int         gridID;
  int         zaxisID;
  int         tsteptype; /* TSTEP_* */
  int         datatype;  /* DATATYPE_PACKX for GRIB data, else DATATYPE_FLT32 or DATATYPE_FLT64 */
  int         instID;
  int         modelID;
  int         tableID;
  int         timave;
  int         timaccu;
  int         chunktype;
  int         xyz;
  int         missvalused; /* TRUE if missval is defined */
  int         lvalidrange;
  char       *name;
  char       *longname;
  char       *stdname;
  char       *units;
  double      missval;
  double      scalefactor;
  double      addoffset;
  double      validrange[2];
  levinfo_t  *levinfo;
  int         comptype;     // compression type
  int         complevel;    // compression level
  ensinfo_t  *ensdata;      /* Ensemble information */
  cdi_atts_t  atts;
  int         iorank;
  int         decoSize;
  deco_t     *deco;

}
var_t;


typedef struct
{
  int         self;
  int         nvars;        /* number of variables                */
  int         ngrids;
  int         nzaxis;
  int         ntsteps;
  int         taxisID;
  int         tableID;
  int         instID;
  int         modelID;
  int         varsAllocated;
  int         gridIDs[MAX_GRIDS_PS];
  int         zaxisIDs[MAX_ZAXES_PS];
  var_t      *vars;
  cdi_atts_t  atts;
}
vlist_t;


vlist_t *vlist_to_pointer(int vlistID);
const char *vlistInqVarNamePtr(int vlistID, int varID);
const char *vlistInqVarLongnamePtr(int vlistID, int varID);
const char *vlistInqVarStdnamePtr(int vlistID, int varID);
const char *vlistInqVarUnitsPtr(int vlistID, int varID);
void     vlistDestroyVarName(int vlistID, int varID);
void     vlistDestroyVarLongname(int vlistID, int varID);
void     vlistDestroyVarUnits(int vlistID, int varID);
void     vlistDefVarTsteptype(int vlistID, int varID, int tsteptype);
int      vlistInqVarMissvalUsed(int vlistID, int varID);
int      vlistHasTime(int vlistID);

int      vlistDelAtts(int vlistID, int varID);
int      vlistCopyVarAtts(int vlistID1, int varID_1, int vlistID2, int varID_2);

#ifdef USE_MPI
void     vlistUnpack(char * buffer, int bufferSize, int * pos, int, MPI_Comm comm);
void     vlistDefVarDeco ( int vlistID, int varID, int decoSize, 
                              deco_t * deco );
#endif

/*      vlistDefVarValidrange: Define the valid range of a Variable */
void    vlistDefVarValidrange(int vlistID, int varID, const double *validrange);

/*      vlistInqVarValidrange: Get the valid range of a Variable */
int     vlistInqVarValidrange(int vlistID, int varID, double *validrange);

#endif  /* _VLIST_H */
/*
 * Local Variables:
 * c-file-style: "Java"
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * show-trailing-whitespace: t
 * require-trailing-newline: t
 * End:
 */
