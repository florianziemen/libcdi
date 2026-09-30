/* DO NOT REMOVE the config.h include file under any circumstances,
 * it's very much needed on some platforms */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
/* DO NOT REMOVE the above config.h include file under any
 * circumstances as long as it's the autoconf configuration header
 * used to build this package. When it's missing on some platforms,
 * some poor person has to do long, tedious debugging sessions, where
 * struct offsets almost imperceptibly change from one file to the
 * next to find out what happened */

#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "cdi.h"
#include "cdi_int.h"
#include "vlist.h"
#include "cdi_att.h"
#include "error.h"
#include "serialize.h"
#include "grid.h"
#include "zaxis.h"
#include "resource_unpack.h"
#include "dmemory.h"

static cdi_atts_t *
get_attsP(vlist_t *vlistP, int varID)
{
  if (varID == CDI_GLOBAL) return &vlistP->atts;
  if (varID >= 0 && varID < vlistP->nvars) return &(vlistP->vars[varID].atts);
  return NULL;
}

static inline short
att_name_nlen(const char *name)
{
  size_t len = strlen(name);
  if (len >= (size_t) CDI_MAX_NAME) return (short) (CDI_MAX_NAME - 1);
  return (short) len;
}

static int
att_index(cdi_atts_t *attsP, const char *name)
{
  xassert(attsP != NULL);
  if (attsP->nelems == 0) return -1;

  short slen = att_name_nlen(name);

  cdi_att_t *atts = attsP->value;
  for (int i = 0; i < attsP->nelems; i++)
  {
    cdi_att_t *attp = atts + i;
    if (attp->namesz != slen) continue;
    if (memcmp(attp->name, name, (size_t) slen + 1) == 0) return i;
  }

  return -1;
}

static cdi_att_t *
find_att(cdi_atts_t *attsP, const char *name)
{
  int idx = att_index(attsP, name);
  return (idx >= 0) ? attsP->value + idx : NULL;
}

static cdi_att_t *
new_att(cdi_atts_t *attsP, const char *name)
{
  xassert(attsP != NULL);
  xassert(name != NULL);

  if (attsP->nelems == attsP->nalloc) return NULL;

  cdi_att_t *attp = &(attsP->value[attsP->nelems]);
  attsP->nelems++;

  short slen = att_name_nlen(name);

  attp->name = (char *) Malloc((size_t) slen + 1);
  memcpy(attp->name, name, (size_t) slen + 1);
  attp->namesz = slen;
  attp->xvalue = NULL;
  attp->xsz = 0;
  attp->nelems = 0;
  attp->indtype = 0;
  attp->exdtype = 0;

  return attp;
}

static void
fill_att(cdi_att_t *attp, short indtype, short exdtype, short nelems, int xsz, const void *xvalue)
{
  xassert(attp != NULL);
  xassert(xsz == 0 || xvalue != NULL);

  int oldxsz = attp->xsz;
  attp->xsz = xsz;
  attp->indtype = indtype;
  attp->exdtype = exdtype;
  attp->nelems = nelems;

  if (xsz == 0)
  {
    Free(attp->xvalue);
    attp->xvalue = NULL;
  }
  else
  {
    if (attp->xvalue == NULL || xsz != oldxsz) { attp->xvalue = Realloc(attp->xvalue, (size_t) xsz); }
    if (attp->xvalue != xvalue) memcpy(attp->xvalue, xvalue, (size_t) xsz);
  }
}

static cdi_atts_t *
cdi_get_attsP(int objID, int varID)
{
  cdi_atts_t *attsP = NULL;

  if (varID == CDI_GLOBAL && reshGetTxCode(objID) == GRID)
  {
    grid_t *gridP = grid_to_pointer(objID);
    attsP = &gridP->atts;
  }
  else if (varID == CDI_GLOBAL && reshGetTxCode(objID) == ZAXIS)
  {
    zaxis_t *zaxisP = zaxis_to_pointer(objID);
    attsP = &zaxisP->atts;
  }
  else
  {
    vlist_t *vlistP = vlist_to_pointer(objID);
    attsP = get_attsP(vlistP, varID);
  }

  return attsP;
}

/*
@Function  cdiInqNatts
@Title     Get number of attributes

@Prototype int cdiInqNatts(int cdiID, int varID, int *nattsP)
@Parameter
    @Item  cdiID    CDI ID, from a previous call to @fref{vlistCreate}, @fref{gridCreate} or @fref{streamInqVlist}.
    @Item  varID    Variable identifier, or @func{CDI_GLOBAL} for a global attribute.
    @Item  nattsP   Pointer to location for returned number of attributes.

@Description
The function @func{cdiInqNatts} gets the number of attributes assigned to this variable.

@EndFunction
*/
int
cdiInqNatts(int cdiID, int varID, int *nattsP)
{
  cdi_atts_t *attsP = cdi_get_attsP(cdiID, varID);
  xassert(attsP != NULL);

  *nattsP = attsP->nelems;

  return CDI_NOERR;
}

/*
@Function  cdiInqAtt
@Title     Get information about an attribute

@Prototype int cdiInqAtt(int cdiID, int varID, int attnum, char *name, int *typep, int *lenp)
@Parameter
    @Item  cdiID    CDI ID, from a previous call to @fref{vlistCreate}, @fref{gridCreate} or @fref{streamInqVlist}.
    @Item  varID    Variable identifier, or @func{CDI_GLOBAL} for a global attribute.
    @Item  attnum   Attribute number (from 0 to natts-1).
    @Item  name     Pointer to the location for the returned attribute name. The caller must allocate space for the
                    returned string. The maximum possible length, in characters, of
                    the string is given by the predefined constant @func{CDI_MAX_NAME}.
    @Item  typep    Pointer to location for returned attribute type.
    @Item  lenp     Pointer to location for returned attribute number.

@Description
The function @func{cdiInqAtt} gets information about an attribute.

@EndFunction
*/
int
cdiInqAtt(int cdiID, int varID, int attnum, char *name, int *typep, int *lenp)
{
  int status = CDI_NOERR;

  xassert(name != NULL);

  cdi_atts_t *attsP = cdi_get_attsP(cdiID, varID);
  xassert(attsP != NULL);

  cdi_att_t *attp = NULL;
  if (attnum >= 0 && attnum < attsP->nelems) { attp = &(attsP->value[attnum]); }

  if (attp != NULL && attp->name)  // name in use
  {
    memcpy(name, attp->name, (size_t) attp->namesz + 1);
    *typep = attp->exdtype;
    *lenp = attp->nelems;
  }
  else
  {
    name[0] = 0;
    *typep = -1;
    *lenp = 0;
    status = -1;
  }

  return status;
}

int
cdiInqAttLen(int cdiID, int varID, const char *name)
{
  xassert(name != NULL);

  cdi_atts_t *attsP = cdi_get_attsP(cdiID, varID);
  xassert(attsP != NULL);

  const cdi_att_t *atts = find_att(attsP, name);
  if (atts != NULL) return atts->nelems;

  return -1;
}

int
cdiInqAttType(int cdiID, int varID, const char *name)
{
  xassert(name != NULL);

  cdi_atts_t *attsP = cdi_get_attsP(cdiID, varID);
  xassert(attsP != NULL);

  const cdi_att_t *atts = find_att(attsP, name);
  if (atts != NULL) return atts->exdtype;

  return -1;
}

static void
cdi_attribute_free(cdi_att_t *attp)
{
  if (attp->name) { Free(attp->name); }
  if (attp->xvalue) { Free(attp->xvalue); }
  memset(attp, 0, sizeof(*attp));
}

int
cdiDeleteAtts(int cdiID, int varID)
{
  cdi_atts_t *attsP = cdi_get_attsP(cdiID, varID);
  xassert(attsP != NULL);

  for (int i = 0; i < attsP->nelems; i++)
  {
    cdi_att_t *attp = &(attsP->value[i]);
    cdi_attribute_free(attp);
  }

  attsP->nelems = 0;

  return CDI_NOERR;
}

int
cdiDelAtt(int cdiID, int varID, const char *name)
{
  cdi_atts_t *attsP = cdi_get_attsP(cdiID, varID);
  xassert(attsP != NULL);

  int idx = att_index(attsP, name);
  if (idx < 0) return -1;

  cdi_attribute_free(&attsP->value[idx]);

  int last = attsP->nelems - 1;
  if (idx < last) memmove(&attsP->value[idx], &attsP->value[idx + 1], (size_t) (last - idx) * sizeof(cdi_att_t));
  memset(&attsP->value[last], 0, sizeof(cdi_att_t));
  attsP->nelems = last;

  return CDI_NOERR;
}

static void
define_attribute(cdi_atts_t *attsP, const char *name, short indtype, short exdtype, short len, int xsz, const void *xp)
{
  cdi_att_t *attp = find_att(attsP, name);
  if (attp == NULL) attp = new_att(attsP, name);
  if (attp != NULL) fill_att(attp, indtype, exdtype, len, xsz, xp);
}

static int
cdi_def_att(short indtype, short exdtype, int cdiID, int varID, const char *name, short len, int xsz, const void *xp)
{
  if (len != 0 && xp == NULL) return CDI_EINVAL;  // Null arg

  cdi_atts_t *attsP = cdi_get_attsP(cdiID, varID);
  xassert(attsP != NULL);

  define_attribute(attsP, name, indtype, exdtype, len, xsz, xp);

  return CDI_NOERR;
}

static int
cdi_inq_att(int indtype, int cdiID, int varID, const char *name, int mxsz, void *xp)
{
  int status = CDI_NOERR;

  if (mxsz != 0 && xp == NULL) return CDI_EINVAL;  // Null arg

  cdi_atts_t *attsP = cdi_get_attsP(cdiID, varID);
  xassert(attsP != NULL);

  cdi_att_t *attp = find_att(attsP, name);
  if (attp != NULL)  // name in use
  {
    if (attp->indtype == indtype)
    {
      int xsz = attp->xsz;
      if (mxsz < xsz) { xsz = mxsz; }
      if (xsz > 0) memcpy(xp, attp->xvalue, (size_t) xsz);
    }
    else
    {
      Warning("Attribute %s has wrong data type!", name);
      status = -2;
    }
  }
  else
  {
    // Warning("Internal problem, attribute %s not found!", name);
    status = -1;
  }

  return status;
}

int
cdiCopyAtts(int cdiID1, int varID1, int cdiID2, int varID2)
{
  cdi_atts_t *attsP1 = cdi_get_attsP(cdiID1, varID1);
  xassert(attsP1 != NULL);

  cdi_atts_t *attsP2 = cdi_get_attsP(cdiID2, varID2);
  xassert(attsP2 != NULL);

  for (int i = 0; i < attsP1->nelems; i++)
  {
    cdi_att_t *attp = &(attsP1->value[i]);
    define_attribute(attsP2, attp->name, attp->indtype, attp->exdtype, attp->nelems, attp->xsz, attp->xvalue);
  }

  return CDI_NOERR;
}

/*
@Function  cdiDefAttInt
@Title     Define an integer attribute

@Prototype int cdiDefAttInt(int cdiID, int varID, const char *name, int type, int len, const int *ip)

@Parameter
    @Item  cdiID    CDI ID, from a previous call to @fref{vlistCreate}, @fref{gridCreate} or @fref{zaxisCreate}.
    @Item  varID    Variable identifier, or @func{CDI_GLOBAL} for a global attribute.
    @Item  name     Attribute name.
    @Item  type     External data type (@func{CDI_DATATYPE_INT16} or @func{CDI_DATATYPE_INT32}).
    @Item  len      Number of values provided for the attribute.
    @Item  ip       Pointer to one or more integer values.

@Description
The function @func{cdiDefAttInt} defines an integer attribute.

@EndFunction
*/
int
cdiDefAttInt(int cdiID, int varID, const char *name, int type, int len, const int *ip)
{
  return cdi_def_att(CDI_DATATYPE_INT, type, cdiID, varID, name, (short) len, len * (int) sizeof(int), ip);
}

/*
@Function  cdiDefAttFlt
@Title     Define a floating point attribute

@Prototype int cdiDefAttFlt(int cdiID, int varID, const char *name, int type, int len, const double *dp)

@Parameter
    @Item  cdiID    CDI ID, from a previous call to @fref{vlistCreate}, @fref{gridCreate} or @fref{zaxisCreate}.
    @Item  varID    Variable identifier, or @func{CDI_GLOBAL} for a global attribute.
    @Item  name     Attribute name.
    @Item  type     External data type (@func{CDI_DATATYPE_FLT32} or @func{CDI_DATATYPE_FLT64}).
    @Item  len      Number of values provided for the attribute.
    @Item  dp       Pointer to one or more floating point values.

@Description
The function @func{cdiDefAttFlt} defines a floating point attribute.

@EndFunction
*/
int
cdiDefAttFlt(int cdiID, int varID, const char *name, int type, int len, const double *dp)
{
  return cdi_def_att(CDI_DATATYPE_FLT, type, cdiID, varID, name, (short) len, len * (int) sizeof(double), dp);
}

/*
@Function  cdiDefAttTxt
@Title     Define a text attribute

@Prototype int cdiDefAttTxt(int cdiID, int varID, const char *name, int len, const char *tp)

@Parameter
    @Item  cdiID    CDI ID, from a previous call to @fref{vlistCreate}, @fref{gridCreate} or @fref{zaxisCreate}.
    @Item  varID    Variable identifier, or @func{CDI_GLOBAL} for a global attribute.
    @Item  name     Attribute name.
    @Item  len      Number of values provided for the attribute.
    @Item  tp       Pointer to one or more character values.

@Description
The function @func{cdiDefAttTxt} defines a text attribute.

@Example
Here is an example using @func{cdiDefAttTxt} to define the attribute "description":

@Source
#include "cdi.h"
   ...
int vlistID, varID, status;
char text[] = "description of the variable";
   ...
vlistID = vlistCreate();
varID = vlistDefVar(vlistID, gridID, zaxisID, TIME_VARYING);
   ...
status = cdiDefAttTxt(vlistID, varID, "description", LEN(text), text);
   ...
@EndSource
@EndFunction
*/
int
cdiDefAttTxt(int cdiID, int varID, const char *name, int len, const char *tp)
{
  return cdi_def_att(CDI_DATATYPE_TXT, CDI_DATATYPE_TXT, cdiID, varID, name, (short) len, len, tp);
}

/*
@Function  cdiInqAttInt
@Title     Get the value(s) of an integer attribute

@Prototype int cdiInqAttInt(int cdiID, int varID, const char *name, int mlen, int *ip)
@Parameter
    @Item  cdiID    CDI ID, from a previous call to @fref{vlistCreate}, @fref{gridCreate} or @fref{zaxisCreate}.
    @Item  varID    Variable identifier, or @func{CDI_GLOBAL} for a global attribute.
    @Item  name     Attribute name.
    @Item  mlen     Number of allocated values provided for the attribute.
    @Item  ip       Pointer location for returned integer attribute value(s).

@Description
The function @func{cdiInqAttInt} gets the values(s) of an integer attribute.

@EndFunction
*/
int
cdiInqAttInt(int cdiID, int varID, const char *name, int mlen, int *ip)
{
  return cdi_inq_att(CDI_DATATYPE_INT, cdiID, varID, name, mlen * (int) sizeof(int), ip);
}

/*
@Function  cdiInqAttFlt
@Title     Get the value(s) of a floating point attribute

@Prototype int cdiInqAttFlt(int cdiID, int varID, const char *name, int mlen, double *dp)
@Parameter
    @Item  cdiID    CDI ID, from a previous call to @fref{vlistCreate}, @fref{gridCreate} or @fref{zaxisCreate}.
    @Item  varID    Variable identifier, or @func{CDI_GLOBAL} for a global attribute.
    @Item  name     Attribute name.
    @Item  mlen     Number of allocated values provided for the attribute.
    @Item  dp       Pointer location for returned floating point attribute value(s).

@Description
The function @func{cdiInqAttFlt} gets the values(s) of a floating point attribute.

@EndFunction
*/
int
cdiInqAttFlt(int cdiID, int varID, const char *name, int mlen, double *dp)
{
  return cdi_inq_att(CDI_DATATYPE_FLT, cdiID, varID, name, mlen * (int) sizeof(double), dp);
}

/*
@Function  cdiInqAttTxt
@Title     Get the value(s) of a text attribute

@Prototype int cdiInqAttTxt(int cdiID, int varID, const char *name, int mlen, char *tp)
@Parameter
    @Item  cdiID    CDI ID, from a previous call to @fref{vlistCreate}, @fref{gridCreate} or @fref{zaxisCreate}.
    @Item  varID    Variable identifier, or @func{CDI_GLOBAL} for a global attribute.
    @Item  name     Attribute name.
    @Item  mlen     Number of allocated values provided for the attribute.
    @Item  tp       Pointer location for returned text attribute value(s).

@Description
The function @func{cdiInqAttTxt} gets the values(s) of a text attribute.

@EndFunction
*/
int
cdiInqAttTxt(int cdiID, int varID, const char *name, int mlen, char *tp)
{
  return cdi_inq_att(CDI_DATATYPE_TXT, cdiID, varID, name, mlen, tp);
}

enum
{
  cdi_att_nints = 4, /* namesz, exdtype, indtype, nelems */
};

static inline int
cdiAttSerialType(int indtype)
{
  switch (indtype)
  {
    case CDI_DATATYPE_FLT: return CDI_DATATYPE_FLT64;
    case CDI_DATATYPE_INT:
    case CDI_DATATYPE_TXT: return indtype;
    default: xabort("Unknown datatype encountered: %d\n", indtype);
  }
}

static inline int
cdiAttElemSize(int indtype)
{
  switch (indtype)
  {
    case CDI_DATATYPE_FLT: return (int) sizeof(double);
    case CDI_DATATYPE_INT: return (int) sizeof(int);
    case CDI_DATATYPE_TXT: return 1;
    default: xabort("Unknown datatype encountered: %d\n", indtype);
  }
}

int
cdi_att_compare(cdi_atts_t *attsPa, cdi_atts_t *attsPb, int attnum)
{
  xassert(attnum >= 0 && attnum < attsPa->nelems && attnum < attsPb->nelems);
  cdi_att_t *attpa = attsPa->value + attnum;
  cdi_att_t *attpb = attsPb->value + attnum;

  if (attpa->xsz != attpb->xsz) return 1;
  if (attpa->namesz != attpb->namesz) return 1;
  if (attpa->name && attpb->name && memcmp(attpa->name, attpb->name, attpa->namesz) != 0) return 1;
  if (attpa->indtype != attpb->indtype || attpa->exdtype != attpb->exdtype || attpa->nelems != attpb->nelems) return 1;
  if (attpa->xsz > 0 && memcmp(attpa->xvalue, attpb->xvalue, (size_t) attpa->xsz) != 0) return 1;

  return 0;
}

static int
cdiAttGetSize(cdi_atts_t *attsP, int attnum, void *context)
{
  xassert(attnum >= 0 && attnum < attsP->nelems);
  cdi_att_t *attp = &(attsP->value[attnum]);
  int txsize
      = serializeGetSize(cdi_att_nints, CDI_DATATYPE_INT, context) + serializeGetSize(attp->namesz, CDI_DATATYPE_TXT, context);
  txsize += serializeGetSize(attp->nelems, cdiAttSerialType(attp->indtype), context);
  return txsize;
}

int
cdiAttsGetSize(void *vp, int varID, void *context)
{
  cdi_atts_t *attsP;
  xassert(attsP = get_attsP((vlist_t *) vp, varID));
  int txsize = serializeGetSize(1, CDI_DATATYPE_INT, context);
  int numAtts = attsP->nelems;
  for (int i = 0; i < numAtts; ++i) { txsize += cdiAttGetSize(attsP, i, context); }
  return txsize;
}

static void
cdiAttPack(cdi_atts_t *attsP, int attnum, void *buf, int size, int *position, void *context)
{
  int tempbuf[cdi_att_nints];

  xassert(attnum >= 0 && attnum < attsP->nelems);
  cdi_att_t *attp = &(attsP->value[attnum]);
  tempbuf[0] = attp->namesz;
  tempbuf[1] = attp->exdtype;
  tempbuf[2] = attp->indtype;
  tempbuf[3] = attp->nelems;
  serializePack(tempbuf, cdi_att_nints, CDI_DATATYPE_INT, buf, size, position, context);
  serializePack(attp->name, attp->namesz, CDI_DATATYPE_TXT, buf, size, position, context);
  serializePack(attp->xvalue, attp->nelems, cdiAttSerialType(attp->indtype), buf, size, position, context);
}

void
cdiAttsPack(void *vp, int varID, void *buf, int size, int *position, void *context)
{
  cdi_atts_t *attsP;
  xassert(attsP = get_attsP((vlist_t *) vp, varID));
  int numAtts = attsP->nelems;
  int numAttsI = numAtts;
  xassert(numAtts <= INT_MAX);
  serializePack(&numAttsI, 1, CDI_DATATYPE_INT, buf, size, position, context);
  for (int i = 0; i < numAtts; ++i) cdiAttPack(attsP, i, buf, size, position, context);
}

static void
cdiAttUnpack(int cdiID, int varID, void *buf, int size, int *position, void *context)
{
  int tempbuf[cdi_att_nints];

  serializeUnpack(buf, size, position, tempbuf, cdi_att_nints, CDI_DATATYPE_INT, context);
  int namesz = tempbuf[0];
  xassert(namesz >= 0);
  char attNameStack[CDI_MAX_NAME];
  char *attName = ((size_t) namesz < CDI_MAX_NAME) ? attNameStack : (char *) Malloc((size_t) namesz + 1);
  serializeUnpack(buf, size, position, attName, namesz, CDI_DATATYPE_TXT, context);
  attName[namesz] = '\0';
  int attVDt = cdiAttSerialType(tempbuf[2]);
  int elemSize = cdiAttElemSize(tempbuf[2]);
  size_t dataSize = (size_t) elemSize * (size_t) tempbuf[3];
  char attDataStack[512];
  void *attData = (dataSize <= sizeof(attDataStack)) ? attDataStack : Malloc(dataSize);
  serializeUnpack(buf, size, position, attData, tempbuf[3], attVDt, context);
  cdi_def_att(tempbuf[2], tempbuf[1], cdiID, varID, attName, (short) tempbuf[3], (int) dataSize, attData);
  if (attData != attDataStack) Free(attData);
  if (attName != attNameStack) Free(attName);
}

void
cdiAttsUnpack(int cdiID, int varID, void *buf, int size, int *position, void *context)
{
  int numAtts;
  serializeUnpack(buf, size, position, &numAtts, 1, CDI_DATATYPE_INT, context);
  for (int i = 0; i < numAtts; ++i) cdiAttUnpack(cdiID, varID, buf, size, position, context);
}

/*
 * Local Variables:
 * c-file-style: "Java"
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * show-trailing-whitespace: t
 * require-trailing-newline: t
 * End:
 */
