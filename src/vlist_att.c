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
      /*      attsp = vlistptr->vars[varID]->atts; */
    }

  return (attsp);
}


static
CDI_attr *find_attr(CDI_atts *attsp, const char *name)
{
  CDI_attr *attrp;
  size_t attrid;
  size_t slen;

  assert(attsp != NULL);

  if ( attsp->nelems == 0 ) return NULL;

  slen = strlen(name);

  for ( attrid = 0; attrid < attsp->nelems; attrid++ )
    {
      attrp = &(attsp->value[attrid]);
      if ( attrp->namesz == slen )
	if ( strncmp(attrp->name, name, slen) == 0)
	  {
	    return (attrp); /* Normal return */
	  }
    }

  return (NULL);
}


static
CDI_attr *new_attr(CDI_atts *attsp, const char *name)
{
  static char func[] = "new_attr";
  CDI_attr *attrp;
  size_t slen;

  assert(attsp != NULL);
  assert(name  != NULL);

  if ( attsp->nelems == attsp->nalloc ) return (NULL);

  attrp = &(attsp->value[attsp->nelems]);
  attsp->nelems++;

  slen = strlen(name);

  attrp->name = (char *) malloc(slen+1);
  memcpy(attrp->name, name, slen+1);
  attrp->namesz = slen;

  return (attrp);
}


static
void fill_attr(CDI_attr *attrp, int type, size_t nelems, size_t xsz, const void *xvalue)
{
  static char func[] = "fill_attr";

  assert(attrp != NULL);

  attrp->xsz = xsz;
  attrp->type = type;
  attrp->nelems = nelems;

  attrp->xvalue = (void *) malloc(xsz);
  memcpy(attrp->xvalue, xvalue, xsz);
}


int vlistInqNatts(int vlistID, int varID, int *nattsp)
{
  static char func[] = "vlistInqNatts";
  int status = CDI_NOERR;
  VLIST *vlistptr;
  CDI_atts *attsp;

  vlistptr = vlist_to_pointer(vlistID);
  
  attsp = get_attsp(vlistptr, varID);
  if ( attsp == NULL ) Error(func, "Internal problem, attributes not found!");

  *nattsp = attsp->nelems;

  return (status);
}


int vlistInqAttr(int vlistID, int varID, int attrnum, char *name, int *typep, int *lenp)
{
  static char func[] = "vlistInqAttr";
  int status = CDI_NOERR;
  VLIST *vlistptr;
  CDI_attr *attrp = NULL;
  CDI_atts *attsp;

  assert(name != NULL);

  vlistptr = vlist_to_pointer(vlistID);

  attsp = get_attsp(vlistptr, varID);
  if ( attsp == NULL ) Error(func, "Internal problem, attributes not found!");

  if ( attrnum >= 0 && attrnum < (int)attsp->nelems )
    attrp = &(attsp->value[attrnum]);

  if ( attrp != NULL ) /* name in use */
    {
      memcpy(name, attrp->name, attrp->namesz+1);
      *typep  = attrp->type;
      *lenp   = attrp->nelems;
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
  CDI_attr *attrp = NULL;
  CDI_atts *attsp;
  int attrid;

  vlistptr = vlist_to_pointer(vlistID);

  attsp = get_attsp(vlistptr, varID);
  if ( attsp == NULL ) Error(func, "Internal problem, attributes not found!");

  for ( attrid = 0; attrid < (int)attsp->nelems; attrid++ )
    {
      attrp = &(attsp->value[attrid]);
      if ( attrp->name   ) free(attrp->name);
      if ( attrp->xvalue ) free(attrp->xvalue);
    }

  attsp->nelems = 0;

  return (status);
}


int vlistDelAttr(int vlistID, int varID, const char *name)
{
  int status = CDI_NOERR;

  fprintf(stderr, "vlistDelAttr not implemented!\n");

  return (status);
}


static
int vlist_def_attr(int type, int vlistID, int varID, const char *name, size_t len, size_t xsz, const void *xp)
{
  static char func[] = "vlist_def_attr";
  int status = CDI_NOERR;
  VLIST *vlistptr;
  CDI_attr *attrp;
  CDI_atts *attsp;

  if ( len != 0 && xp == NULL ) /* Null arg */
    {
      return (CDI_EINVAL);
    }

  vlistptr = vlist_to_pointer(vlistID);

  attsp = get_attsp(vlistptr, varID);
  if ( attsp == NULL ) Error(func, "Internal problem, attributes not found!");

  attrp = find_attr(attsp, name);
  if ( attrp != NULL ) /* name in use */
    {
    }
  else
    {
      attrp = new_attr(attsp, name);
      if ( attrp != NULL )
	fill_attr(attrp, type, len, xsz, xp);
    }

  return (status);
}


static
int vlist_inq_attr(int type, int vlistID, int varID, const char *name, size_t mxsz, void *xp)
{
  static char func[] = "vlist_inq_attr";
  int status = CDI_NOERR;
  VLIST *vlistptr;
  CDI_attr *attrp;
  CDI_atts *attsp;
  size_t xsz;

  if ( mxsz != 0 && xp == NULL ) /* Null arg */
    {
      return (CDI_EINVAL);
    }

  vlistptr = vlist_to_pointer(vlistID);

  attsp = get_attsp(vlistptr, varID);
  if ( attsp == NULL ) Error(func, "Internal problem, attributes not found!");

  attrp = find_attr(attsp, name);
  if ( attrp != NULL ) /* name in use */
    {
      xsz = attrp->xsz;
      if ( mxsz < xsz ) xsz = mxsz;
      memcpy(xp, attrp->xvalue, xsz);
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
  CDI_attr *attrp = NULL;
  CDI_atts *attsp1;
  int attrid;

  vlistptr1 = vlist_to_pointer(vlistID1);

  attsp1 = get_attsp(vlistptr1, varID_1);
  if ( attsp1 == NULL ) Error(func, "Internal problem, attributes not found!");

  for ( attrid = 0; attrid < (int)attsp1->nelems; attrid++ )
    {
      attrp = &(attsp1->value[attrid]);
      vlist_def_attr(attrp->type, vlistID2, varID_2, attrp->name, attrp->nelems, attrp->xsz, attrp->xvalue);
    }

  return (status);
}


int vlistDefAttrInt(int vlistID, int varID, const char *name, int len, const int *ip)
{
  int status;

  status = vlist_def_attr(DATATYPE_INT, vlistID, varID, name, (size_t) len, len*sizeof(int), (const void *) ip);

  return (status);
}


int vlistDefAttrFlt(int vlistID, int varID, const char *name, int len, const double *dp)
{
  int status;

  status = vlist_def_attr(DATATYPE_FLT, vlistID, varID, name, (size_t) len, len*sizeof(double), (const void *) dp);

  return (status);
}


int vlistDefAttrTxt(int vlistID, int varID, const char *name, int len, const char *tp)
{
  int status;

  status = vlist_def_attr(DATATYPE_TXT, vlistID, varID, name, (size_t) len, len*sizeof(char), (const void *) tp);

  return (status);
}



int vlistInqAttrInt(int vlistID, int varID, const char *name, int mlen, int *ip)
{
  int status = CDI_NOERR;

  status = vlist_inq_attr(DATATYPE_INT, vlistID, varID, name, mlen*sizeof(int), (void *) ip);

  return (CDI_NOERR);
}


int vlistInqAttrFlt(int vlistID, int varID, const char *name, int mlen, double *dp)
{
  int status = CDI_NOERR;

  status = vlist_inq_attr(DATATYPE_FLT, vlistID, varID, name, mlen*sizeof(double), (void *) dp);

  return (status);
}


int vlistInqAttrTxt(int vlistID, int varID, const char *name, int mlen, char *tp)
{
  int status = CDI_NOERR;

  status = vlist_inq_attr(DATATYPE_TXT, vlistID, varID, name, mlen*sizeof(char), (void *) tp);

  return (status);
}
