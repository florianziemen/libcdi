#ifndef VLIST_H
#define VLIST_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdbool.h>
#include <stddef.h> /* size_t */

#include "cdi_int.h"
#include "cdi_key.h"
#include "cdi_att.h"

#define VALIDMISS 1.e+303

typedef struct
{
  bool flag;
  int index;
  int mlevelID;
  int flevelID;
} levinfo_t;

#define DEFAULT_LEVINFO(levID) \
  (levinfo_t) { 0, -1, levID, levID }
/*
#define DEFAULT_LEVINFO(levID) \
  (levinfo_t){ .flag = 0, .index = -1, .flevelID = levID, .mlevelID = levID}
*/
typedef struct
{
  int ens_index;
  int ens_count;
  int forecast_init_type;
} ensinfo_t;

typedef struct
{
  bool isUsed;
  bool flag;
  bool lvalidrange;
  signed char xyz;   /* order of spatial dimensions,
                      * a permutation of 123 */
  bool missvalused;  // true if missval is defined
  int mvarID;
  int fvarID;
  int param;
  int gridID;
  int zaxisID;
  int timetype;   // TIME_*
  int tsteptype;  // TSTEP_*
  int datatype;   // CDI_DATATYPE_PACKX for GRIB data, else CDI_DATATYPE_FLT32 or CDI_DATATYPE_FLT64
  int instID;
  int modelID;
  int tableID;
  int timave;
  int nsb;  // Number of significant bits
  double missval;
  double validrange[2];
  levinfo_t *levinfo;
  int comptype;   // compression type
  int complevel;  // compression level
  cdi_keys_t keys;
  cdi_atts_t atts;
  int subtypeID;  // subtype ID for tile-related meta-data, currently for GRIB-API only.

  int opt_grib_nentries;                // current no. key-value pairs
  int opt_grib_kvpair_size;             // current allocated size
  opt_key_val_pair_t *opt_grib_kvpair;  // (optional) list of keyword/value pairs
} var_t;

typedef struct
{
  // set when a vlist is passed to streamDefVlist() to safeguard against modifications of the wrong vlist object
  bool immutable;
  // set if this vlist has been created by CDI itself, and must not be destroyed by the user, consequently
  bool internal;
  int self;
  int nvars;  // number of variables
  int ngrids;
  int nzaxis;
  int nsubtypes;  // no. of variable subtypes (e.g. sets of tiles)
  long ntsteps;
  int taxisID;
  int tableID;
  int instID;
  int modelID;
  int varsAllocated;
  int gridIDs[MAX_GRIDS_PS];
  int zaxisIDs[MAX_ZAXES_PS];
  int subtypeIDs[MAX_SUBTYPES_PS];
  var_t *vars;
  cdi_keys_t keys;
  cdi_atts_t atts;
} vlist_t;

vlist_t *vlist_to_pointer(int vlistID);
vlist_t *vlist_new_entry(cdiResH resH);

void cdiVlistMakeInternal(int vlistID);
void cdiVlistMakeImmutable(int vlistID);
void cdiVlistDestroy_(int vlistID, bool assertInternal);
int vlistInqVarMissvalUsed(int vlistID, int varID);
bool vlistHasTime(int vlistID);

/*      vlistDefVarValidrange: Define the valid range of a Variable */
void vlistDefVarValidrange(int vlistID, int varID, const double *validrange);

/*      vlistInqVarValidrange: Get the valid range of a Variable */
int vlistInqVarValidrange(int vlistID, int varID, double *validrange);

void vlistInqVarDimorder(int vlistID, int varID, int outDimorder[3]);

void resize_opt_grib_entries(var_t *var, int nentries);

static inline void
vlistAdd2GridIDs(vlist_t *vlistP, int gridID)
{
  int index, ngrids = vlistP->ngrids;
  for (index = 0; index < ngrids; index++)
  {
    if (vlistP->gridIDs[index] == gridID) break;
    //      if ( gridIsEqual(vlistP->gridIDs[index], gridID) ) break;
  }

  if (index == ngrids)
  {
    if (ngrids >= MAX_GRIDS_PS) Error("Internal limit exceeded: more than %d grids.", MAX_GRIDS_PS);
    vlistP->gridIDs[ngrids] = gridID;
    ++(vlistP->ngrids);
  }
}

static inline void
vlistAdd2ZaxisIDs(vlist_t *vlistP, int zaxisID)
{
  int index, nzaxis = vlistP->nzaxis;
  for (index = 0; index < nzaxis; index++)
    if (zaxisID == vlistP->zaxisIDs[index]) break;

  if (index == nzaxis)
  {
    if (nzaxis >= MAX_ZAXES_PS) Error("Internal limit exceeded: more than %d zaxis.", MAX_ZAXES_PS);
    vlistP->zaxisIDs[nzaxis] = zaxisID;
    ++(vlistP->nzaxis);
  }
}

static inline void
vlistAdd2SubtypeIDs(vlist_t *vlistP, int subtypeID)
{
  if (subtypeID == CDI_UNDEFID) return;

  int index, nsubs = vlistP->nsubtypes;
  for (index = 0; index < nsubs; index++)
    if (vlistP->subtypeIDs[index] == subtypeID) break;

  if (index == nsubs)
  {
    if (nsubs >= MAX_SUBTYPES_PS) Error("Internal limit exceeded: more than %d subs.", MAX_SUBTYPES_PS);
    vlistP->subtypeIDs[nsubs] = subtypeID;
    ++(vlistP->nsubtypes);
  }
}

#ifdef HAVE_LIBGRIB_API
extern int cdiNAdditionalGRIBKeys;
extern char *cdiAdditionalGRIBKeys[];
#endif

extern const resOps vlistOps;

#endif /* VLIST_H */
/*
 * Local Variables:
 * c-file-style: "Java"
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * show-trailing-whitespace: t
 * require-trailing-newline: t
 * End:
 */
