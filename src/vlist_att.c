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
	if ( strncmp(attp->name, name, slen) == 0)
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

  attp->xvalue = (void *) malloc(xsz);
  memcpy(attp->xvalue, xvalue, xsz);
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
  if ( attp != NULL ) /* name in use */
    {
    }
  else
    {
      attp = new_att(attsp, name);
      if ( attp != NULL )
	fill_att(attp, type, len, xsz, xp);
    }

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


int vlistDefAttInt(int vlistID, int varID, const char *name, int len, const int *ip)
{
  int status;

  status = vlist_def_att(DATATYPE_INT, vlistID, varID, name, (size_t) len, len*sizeof(int), (const void *) ip);

  return (status);
}


int vlistDefAttFlt(int vlistID, int varID, const char *name, int len, const double *dp)
{
  int status;

  status = vlist_def_att(DATATYPE_FLT, vlistID, varID, name, (size_t) len, len*sizeof(double), (const void *) dp);

  return (status);
}


int vlistDefAttTxt(int vlistID, int varID, const char *name, int len, const char *tp)
{
  int status;

  status = vlist_def_att(DATATYPE_TXT, vlistID, varID, name, (size_t) len, len*sizeof(char), (const void *) tp);

  return (status);
}



int vlistInqAttInt(int vlistID, int varID, const char *name, int mlen, int *ip)
{
  int status = CDI_NOERR;

  status = vlist_inq_att(DATATYPE_INT, vlistID, varID, name, mlen*sizeof(int), (void *) ip);

  return (CDI_NOERR);
}


int vlistInqAttFlt(int vlistID, int varID, const char *name, int mlen, double *dp)
{
  int status = CDI_NOERR;

  status = vlist_inq_att(DATATYPE_FLT, vlistID, varID, name, mlen*sizeof(double), (void *) dp);

  return (status);
}


int vlistInqAttTxt(int vlistID, int varID, const char *name, int mlen, char *tp)
{
  int status = CDI_NOERR;

  status = vlist_inq_att(DATATYPE_TXT, vlistID, varID, name, mlen*sizeof(char), (void *) tp);

  return (status);
}
