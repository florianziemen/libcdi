#ifndef  ZAXIS_H
#define  ZAXIS_H

#ifdef  HAVE_CONFIG_H
#include "config.h"
#endif

#include "cdi_key.h"
#include "cdi_att.h"

typedef struct {
  double value;
  bool defined;
}
zkey_double_t;

typedef struct {
  char     dimname[CDI_MAX_NAME];
  char     vdimname[CDI_MAX_NAME];
  char     name[CDI_MAX_NAME];
  char     longname[CDI_MAX_NAME];
  char     stdname[CDI_MAX_NAME];
  char     units[CDI_MAX_NAME];
  char     psname[CDI_MAX_NAME];
  char     p0name[CDI_MAX_NAME];
  zkey_double_t p0value;
  double  *vals;
#ifndef USE_MPI
  char   **cvals;
  int      clength;
#endif
  double  *lbounds;
  double  *ubounds;
  double  *weights;
  int      self;
  int      datatype;
  int      scalar;
  int      type;
  int      ltype;    /* GRIB level type */
  int      ltype2;
  int      size;
  int      direction;
  int      vctsize;
  unsigned positive;
  double  *vct;
  int      number;   /* Reference number to a generalized Z-axis */
  int      nhlev;
  unsigned char uuid[CDI_UUID_SIZE];
  cdi_keys_t keys;
  cdi_atts_t atts;
}
zaxis_t;


void zaxisGetTypeDescription(int zaxisType, int* outPositive, const char** outName, const char** outLongName, const char** outStdName, const char** outUnit);  //The returned const char* point to static storage. Don't free or modify them.

unsigned cdiZaxisCount(void);

zaxis_t *zaxis_to_pointer(int zaxisID);

void cdiZaxisGetIndexList(unsigned numIDs, int *IDs);

void
zaxisUnpack(char * unpackBuffer, int unpackBufferSize,
            int * unpackBufferPos, int originNamespace, void *context,
            int force_id);

void zaxisDefLtype2(int zaxisID, int ltype2);

const resOps *getZaxisOps(void);

const char *zaxisInqNamePtr(int zaxisID);

const double *zaxisInqLevelsPtr(int zaxisID);
#ifndef USE_MPI
char **zaxisInqCValsPtr(int zaxisID);
#endif
void zaxisResize(int zaxisID, int size);

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
