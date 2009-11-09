#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "dmemory.h"

#include "cdi.h"
#include "stream_int.h"
#include "vlist.h"


static
CDI_atts *get_attsp(VLIST *vlistptr, int varID)
{
  static char func[] = "get_attsp";
  CDI_atts *attsp = NULL;

  if ( varID == CDI_GLOBAL )
    {
      attsp = &vlistptr->atts;
    }
  else
    {
      if ( varID >= 0 && varID < vlistptr->nvars )
	attsp = &(vlistptr->vars[varID].atts);
    }

  return (attsp);
}


static
CDI_att *find_att(CDI_atts *attsp, const char *name)
{
  CDI_att *attp;
  size_t attid;
  size_t slen;

  assert(attsp != NULL);

  if ( attsp->nelems == 0 ) return NULL;

  slen = strlen(name);

  for ( attid = 0; attid < attsp->nelems; attid++ )
    {
      attp = &(attsp->value[attid]);
      if ( attp->namesz == slen )
	if ( memcmp(attp->name, name, slen) == 0)
	  {
	    return (attp); /* Normal return */
	  }
    }

  return (NULL);
}


static
CDI_att *new_att(CDI_atts *attsp, const char *name)
{
  static char func[] = "new_att";
  CDI_att *attp;
  size_t slen;

  assert(attsp != NULL);
  assert(name  != NULL);

  if ( attsp->nelems == attsp->nalloc ) return (NULL);

  attp = &(attsp->value[attsp->nelems]);
  attsp->nelems++;

  slen = strlen(name);

  attp->name = (char *) malloc(slen+1);
  memcpy(attp->name, name, slen+1);
  attp->namesz = slen;
  attp->xvalue = NULL;

  return (attp);
}


static
void fill_att(CDI_att *attp, int type, size_t nelems, size_t xsz, const void *xvalue)
{
  static char func[] = "fill_att";

  assert(attp != NULL);

  attp->xsz = xsz;
  attp->type = type;
  attp->nelems = nelems;

  if ( xsz > 0 )
    {
      attp->xvalue = (void *) realloc(attp->xvalue, xsz);
      memcpy(attp->xvalue, xvalue, xsz);
    }
}


int vlistInqNatts(int vlistID, int varID, int *nattsp)
{
  static char func[] = "vlistInqNatts";
  int status = CDI_NOERR;
  VLIST *vlistptr;
  CDI_atts *attsp;

  vlistptr = vlist_to_pointer(vlistID);
  
  attsp = get_attsp(vlistptr, varID);
  assert(attsp != NULL);

  *nattsp = attsp->nelems;

  return (status);
}


int vlistInqAtt(int vlistID, int varID, int attnum, char *name, int *typep, int *lenp)
{
  static char func[] = "vlistInqAtt";
  int status = CDI_NOERR;
  VLIST *vlistptr;
  CDI_att *attp = NULL;
  CDI_atts *attsp;

  assert(name != NULL);

  vlistptr = vlist_to_pointer(vlistID);

  attsp = get_attsp(vlistptr, varID);
  assert(attsp != NULL);

  if ( attnum >= 0 && attnum < (int)attsp->nelems )
    attp = &(attsp->value[attnum]);

  if ( attp != NULL ) /* name in use */
    {
      memcpy(name, attp->name, attp->namesz+1);
      *typep  = attp->type;
      *lenp   = attp->nelems;
    }
  else
    {
      name[0] =  0;
      *typep  = -1;
      *lenp   =  0;
    }

  return (status);
}


int vlistDelAtts(int vlistID, int varID)
{
  static char func[] = "vlistDelAtts";
  int status = CDI_NOERR;
  VLIST *vlistptr;
  CDI_att *attp = NULL;
  CDI_atts *attsp;
  int attid;

  vlistptr = vlist_to_pointer(vlistID);

  attsp = get_attsp(vlistptr, varID);
  assert(attsp != NULL);

  for ( attid = 0; attid < (int)attsp->nelems; attid++ )
    {
      attp = &(attsp->value[attid]);
      if ( attp->name   ) free(attp->name);
      if ( attp->xvalue ) free(attp->xvalue);
    }

  attsp->nelems = 0;

  return (status);
}


int vlistDelAtt(int vlistID, int varID, const char *name)
{
  int status = CDI_NOERR;

  fprintf(stderr, "vlistDelAtt not implemented!\n");

  return (status);
}


static
int vlist_def_att(int type, int vlistID, int varID, const char *name, size_t len, size_t xsz, const void *xp)
{
  static char func[] = "vlist_def_att";
  int status = CDI_NOERR;
  VLIST *vlistptr;
  CDI_att *attp;
  CDI_atts *attsp;

  if ( len != 0 && xp == NULL ) /* Null arg */
    {
      return (CDI_EINVAL);
    }

  vlistptr = vlist_to_pointer(vlistID);

  attsp = get_attsp(vlistptr, varID);
  assert(attsp != NULL);

  attp = find_att(attsp, name);
  if ( attp == NULL )
    attp = new_att(attsp, name);

  if ( attp != NULL )
    fill_att(attp, type, len, xsz, xp);
  
  return (status);
}


static
int vlist_inq_att(int type, int vlistID, int varID, const char *name, size_t mxsz, void *xp)
{
  static char func[] = "vlist_inq_att";
  int status = CDI_NOERR;
  VLIST *vlistptr;
  CDI_att *attp;
  CDI_atts *attsp;
  size_t xsz;

  if ( mxsz != 0 && xp == NULL ) /* Null arg */
    {
      return (CDI_EINVAL);
    }

  vlistptr = vlist_to_pointer(vlistID);

  attsp = get_attsp(vlistptr, varID);
  assert(attsp != NULL);

  attp = find_att(attsp, name);
  if ( attp != NULL ) /* name in use */
    {
      xsz = attp->xsz;
      if ( mxsz < xsz ) xsz = mxsz;
      if ( xsz > 0 )
	memcpy(xp, attp->xvalue, xsz);
    }
  else
    {
      Warning(func, "Internal problem, attribute %s not found!", name);
    }

  return (status);
}


int vlistCopyVarAtts(int vlistID1, int varID_1, int vlistID2, int varID_2)
{
  static char func[] = "vlistCopyVarAtts";
  int status = CDI_NOERR;
  VLIST *vlistptr1;
  CDI_att *attp = NULL;
  CDI_atts *attsp1;
  int attid;

  vlistptr1 = vlist_to_pointer(vlistID1);

  attsp1 = get_attsp(vlistptr1, varID_1);
  assert(attsp1 != NULL);

  for ( attid = 0; attid < (int)attsp1->nelems; attid++ )
    {
      attp = &(attsp1->value[attid]);
      vlist_def_att(attp->type, vlistID2, varID_2, attp->name, attp->nelems, attp->xsz, attp->xvalue);
    }

  return (status);
}


/*
@Function  vlistDefAttInt
@Title     Define an integer attribute

@Prototype int vlistDefAttInt(int vlistID, int varID, const char *name, int len, const int *ip)

@Parameter
    @Item  vlistID  Variable list ID, from a previous call to @fref{vlistCreate}
    @Item  varID    Variable identifier, or CDI_GLOBAL for a global attribute
    @Item  name     Attribute name
    @Item  len      Number of values provided for the attribute
    @Item  ip       Pointer to one or more integer values

@Description
The function @func{vlistDefAttInt} defines an integer attribute.

@EndFunction
*/
int vlistDefAttInt(int vlistID, int varID, const char *name, int len, const int *ip)
{
  int status;

  status = vlist_def_att(DATATYPE_INT, vlistID, varID, name, (size_t) len, len*sizeof(int), (const void *) ip);

  return (status);
}


/*
@Function  vlistDefAttFlt
@Title     Define a floating point attribute

@Prototype int vlistDefAttFlt(int vlistID, int varID, const char *name, int len, const double *dp)

@Parameter
    @Item  vlistID  Variable list ID, from a previous call to @fref{vlistCreate}
    @Item  varID    Variable identifier, or CDI_GLOBAL for a global attribute
    @Item  name     Attribute name
    @Item  len      Number of values provided for the attribute
    @Item  dp       Pointer to one or more floating point values

@Description
The function @func{vlistDefAttFlt} defines a floating point attribute.

@EndFunction
*/
int vlistDefAttFlt(int vlistID, int varID, const char *name, int len, const double *dp)
{
  int status;

  status = vlist_def_att(DATATYPE_FLT, vlistID, varID, name, (size_t) len, len*sizeof(double), (const void *) dp);

  return (status);
}


/*
@Function  vlistDefAttTxt
@Title     Define a text attribute

@Prototype int vlistDefAttTxt(int vlistID, int varID, const char *name, int len, const char *tp)

@Parameter
    @Item  vlistID  Variable list ID, from a previous call to @fref{vlistCreate}
    @Item  varID    Variable identifier, or CDI_GLOBAL for a global attribute
    @Item  name     Attribute name
    @Item  len      Number of values provided for the attribute
    @Item  tp       Pointer to one or more character values

@Description
The function @func{vlistDefAttTxt} defines a text attribute.

@EndFunction
*/
int vlistDefAttTxt(int vlistID, int varID, const char *name, int len, const char *tp)
{
  int status;

  status = vlist_def_att(DATATYPE_TXT, vlistID, varID, name, (size_t) len, len*sizeof(char), (const void *) tp);

  return (status);
}


/*
@Function  vlistInqAttInt
@Title     Get the value(s) of an integer attribute

@Prototype int vlistInqAttInt(int vlistID, int varID, const char *name, int mlen, int *ip)
@Parameter
    @Item  vlistID  Variable list ID, from a previous call to @fref{vlistCreate}
    @Item  varID    Variable identifier, or CDI_GLOBAL for a global attribute
    @Item  name     Attribute name
    @Item  mlen     Number of allocated values provided for the attribute
    @Item  ip       Pointer location for returned integer attribute value(s)

@Description
The function @func{vlistInqAttInt} gets the values(s) of an integer attribute.

@EndFunction
*/
int vlistInqAttInt(int vlistID, int varID, const char *name, int mlen, int *ip)
{
  int status = CDI_NOERR;

  status = vlist_inq_att(DATATYPE_INT, vlistID, varID, name, mlen*sizeof(int), (void *) ip);

  return (CDI_NOERR);
}


/*
@Function  vlistInqAttFlt
@Title     Get the value(s) of a floating point attribute

@Prototype int vlistInqAttFlt(int vlistID, int varID, const char *name, int mlen, int *dp)
@Parameter
    @Item  vlistID  Variable list ID, from a previous call to @fref{vlistCreate}
    @Item  varID    Variable identifier, or CDI_GLOBAL for a global attribute
    @Item  name     Attribute name
    @Item  mlen     Number of allocated values provided for the attribute
    @Item  dp       Pointer location for returned floating point attribute value(s)

@Description
The function @func{vlistInqAttFlt} gets the values(s) of a floating point attribute.

@EndFunction
*/
int vlistInqAttFlt(int vlistID, int varID, const char *name, int mlen, double *dp)
{
  int status = CDI_NOERR;

  status = vlist_inq_att(DATATYPE_FLT, vlistID, varID, name, mlen*sizeof(double), (void *) dp);

  return (status);
}


/*
@Function  vlistInqAttTxt
@Title     Get the value(s) of a text attribute

@Prototype int vlistInqAttTxt(int vlistID, int varID, const char *name, int mlen, int *tp)
@Parameter
    @Item  vlistID  Variable list ID, from a previous call to @fref{vlistCreate}
    @Item  varID    Variable identifier, or CDI_GLOBAL for a global attribute
    @Item  name     Attribute name
    @Item  mlen     Number of allocated values provided for the attribute
    @Item  tp       Pointer location for returned text attribute value(s)

@Description
The function @func{vlistInqAttTxt} gets the values(s) of a text attribute.

@EndFunction
*/
int vlistInqAttTxt(int vlistID, int varID, const char *name, int mlen, char *tp)
{
  int status = CDI_NOERR;

  status = vlist_inq_att(DATATYPE_TXT, vlistID, varID, name, mlen*sizeof(char), (void *) tp);

  return (status);
}
