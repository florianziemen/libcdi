#if defined (HAVE_CONFIG_H)
#  include "config.h"
#endif

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>

#include "cdi.h"
#include "stream_int.h"
#include "hdf_int.h"


extern int HDF_Fatal;
extern int HDF_Verbose;
extern int HDF_Debug;

#if  defined  (HAVE_LIBHDF5)

int hdf_create(const char *path, int cmode)
{
  static char func[] = "hdf_create";
  int hfid;

  hfid = H5Fcreate(path, cmode, H5P_DEFAULT, H5P_DEFAULT);

  if ( HDF_Debug || hfid < 0 )
    Message(func, "hfid = %d  mode = %d  file = %s", hfid, cmode, path);

  if ( hfid < 0 )
    {
      H5Eprint(NULL);
      Error(func, "%s", path);
    }

  return (hfid);
}

int hdf_open(const char *path, int omode)
{
  static char func[] = "hdf_open";
  int hfid;

  hfid = H5Fopen(path, omode, H5P_DEFAULT);

  if ( HDF_Debug || hfid < 0 )
    Message(func, "hfid = %d  mode = %d  file = %s", hfid, omode, path);

  if ( hfid < 0 )
    {
      H5Eprint(NULL);
      Error(func, "%s", path);
    }

  return (hfid);
}

void hdf_close(int hfid)
{
  static char func[] = "hdf_close";
  int status;

  status = H5Fclose(hfid);

  if ( status < 0 )
    {
      H5Eprint(NULL);
      Error(func, "close problem on %d", hfid);
    }
}
/*
void
hdf_redef(int hfid)
{
  static char func[] = "hdf_redef";
  int status;

  status = nc_redef(hfid);

  if ( status != NC_NOERR )
    Error(func, "%s", nc_strerror(status));
}

void
hdf_enddef(int hfid)
{
  static char func[] = "hdf_enddef";
  int status;

  status = nc_enddef(hfid);

  if ( status != NC_NOERR )
    Error(func, "%s", nc_strerror(status));
}

void
hdf_def_dim(int hfid, const char *name, size_t len, int *dimidp)
{
  static char func[] = "hdf_def_dim";
  int status;

  status = nc_def_dim(hfid, name, len, dimidp);

  if ( HDF_Debug || status != NC_NOERR )
    Message(func, "hfid = %d  name = %s  len = %d", hfid, name, len);

  if ( status != NC_NOERR )
    Error(func, "%s", nc_strerror(status));
}

void
hdf_inq_dimid(int hfid, const char *name, int *dimidp)
{
  static char func[] = "hdf_inq_dimid";
  int status;

  status = nc_inq_dimid(hfid, name, dimidp);

  if (HDF_Debug || status != NC_NOERR)
    Message(func, "hfid = %d  name = %s  dimid= %d", hfid, name, *dimidp);

  if (status != NC_NOERR)
    Error(func, "%s", nc_strerror(status));
}

void
hdf_inq_dim(int hfid, int dimid, char *name, size_t * lengthp)
{
  static char func[] = "hdf_inq_dim";
  int status;

  status = nc_inq_dim(hfid, dimid, name, lengthp);

  if ( HDF_Debug || status != NC_NOERR )
    Message(func, "hfid = %d  dimid = %d  length = %d name = %s",
	    hfid, dimid, *lengthp, name);

  if ( status != NC_NOERR )
    Error(func, "%s", nc_strerror(status));
}

void
hdf_inq_dimname(int hfid, int dimid, char *name)
{
  static char func[] = "hdf_inq_dimname";
  int status;

  status = nc_inq_dimname(hfid, dimid, name);

  if ( HDF_Debug || status != NC_NOERR )
    Message(func, "hfid = %d  dimid = %d  name = %s", hfid, dimid, name);

  if ( status != NC_NOERR )
    Error(func, "%s", nc_strerror(status));
}

void
hdf_inq_dimlen(int hfid, int dimid, size_t * lengthp)
{
  static char func[] = "hdf_inq_dimlen";
  int status;

  status = nc_inq_dimlen(hfid, dimid, lengthp);

  if ( HDF_Debug || status != NC_NOERR )
    Message(func, "hfid = %d dimid = %d length = %d", hfid, dimid, *lengthp);

  if ( status != NC_NOERR )
    Error(func, "%s", nc_strerror(status));
}
*/
void hdf_def_var(int hfid, const char *name, int xtype, int ndims, const int dims[], int *varidp, int plistid)
{
  static char func[] = "hdf_def_var";
  int dataspace;
  hsize_t hdims[9], maxdims[9];
  int i;

  for ( i = 0; i < ndims; i++ )
    {
      hdims[i] = dims[i];
      maxdims[i] = dims[i];
    }

  if (  dims[0] == -1 )
    {
      hdims[0] = 1;
      maxdims[0] = H5S_UNLIMITED;
    }

  dataspace = H5Screate_simple(ndims, hdims, maxdims);
  *varidp   = H5Dcreate(hfid, name, xtype, dataspace, plistid);
  H5Sclose(dataspace);
  /*
  for ( i = 0; i < ndims; i++ ) printf("%s: dim %d size %d\n", func, i, dims[i]);
  */

  if ( HDF_Debug || *varidp < 0 )
    {
      int i;
      for ( i = 0; i < ndims; i++ ) printf("%s: dim %d size %d\n", func, i, dims[i]);
      Message(func, "hfid = %d  name = %s  xtype = %d  ndims = %d  varid = %d  plistid = %d",
	      hfid, name, xtype, ndims, *varidp, plistid);
    }

  if ( *varidp < 0 )
    {
      H5Eprint(NULL);
      Error(func, "%s", name);
    }
}

void hdf_inq_varid(int hfid, const char *name, int *varidp)
{
  static char func[] = "hdf_inq_varid";
  int status = 0;

  Message(func, "hfid = %d  name = %s  varid = %d ", hfid, name, *varidp);
  Error(func, "not implemented");

  if ( HDF_Debug || status < 0 )
    Message(func, "hfid = %d  name = %s  varid = %d ", hfid, name, *varidp);
  /*
  if ( status != NC_NOERR )
    Error(func, "%s", nc_strerror(status));
  */
}

int hdf_inq_ngatts(int hfid)
{
  int status;
  int obj;
  int ngatts = 0;

  obj = H5Gopen(hfid, "/");

  ngatts = H5Aget_num_attrs(obj);

  status = H5Gclose(obj);

  return (ngatts);
}

int hdf_inq_nvars(int hfid)
{
  static char func[] = "hdf_inq_nvars";
  int status;
  int nvars = 0;
  int groupID;
  char name[128];
  hsize_t num_obj, idx;

  groupID = H5Gopen(hfid, "/");
  status = H5Gget_num_objs(groupID, &num_obj);
  for ( idx = 0; idx < num_obj; idx++ )
    {
      if ( H5Gget_objtype_by_idx(groupID, idx) == H5G_DATASET )
	{
	  nvars++;
	}
      else if ( H5Gget_objtype_by_idx(groupID, idx) == H5G_GROUP )
	{
	  hsize_t num_obj2, idx2;
	  int groupID2;
	  H5Gget_objname_by_idx(groupID, idx, name, (size_t) 128);
	  groupID2 = H5Gopen(hfid, name);
	  status = H5Gget_num_objs(groupID2, &num_obj2);
	  for ( idx2 = 0; idx2 < num_obj2; idx2++ )
	    {
	      if ( H5Gget_objtype_by_idx(groupID2, idx2) == H5G_DATASET )
		{
		  nvars++;
		}
	      else if ( H5Gget_objtype_by_idx(groupID2, idx2) == H5G_GROUP )
		{
		  printf("group level2 not supported\n"); 
		}  
	    }
	  H5Gclose(groupID2);
	}
    }
  H5Gclose(groupID);

  if ( HDF_Debug || status < 0 )
    Message(func, "hfid = %d  nvars = %d", hfid, nvars);

  return (nvars);
}
/*

void
hdf_inq_vartype(int hfid, int varid, nc_type *xtypep)
{
  static char func[] = "hdf_inq_vartype";
  int status;

  status = nc_inq_vartype(hfid, varid, xtypep);

  if ( HDF_Debug || status != NC_NOERR )
    Message(func, "hfid = %d varid = %d xtype = %s", hfid, varid, *xtypep);

  if ( status != NC_NOERR )
    Error(func, "%s", nc_strerror(status));
}
*/
int hdf_inq_varndims(int varid)
{
  static char func[] = "hdf_inq_varndims";
  int status;
  int varspaceid;
  int ndims;

  varspaceid = H5Dget_space(varid);
  ndims = H5Sget_simple_extent_ndims(varspaceid);
  status = H5Sclose(varspaceid);

  if ( HDF_Debug || status < 0 )
    Message(func, "varid = %d\n", varid);

  return (ndims);
}
/*

int hdf_inq_varxtype(int varid)
{
  static char func[] = "hdf_inq_varxtype";
  int status;
  int varspaceid;
  int xtype;

  varspaceid = H5Dget_space(varid);
  status = H5Sclose(varspaceid);

  if ( HDF_Debug || status < 0 )
    Message(func, "varid = %d\n", varid);
    Message(func, "varid = %d not implemented\n", varid);

  return (xtype);
}

void
hdf_inq_vardimid(int hfid, int varid, int dimids[])
{
  static char func[] = "hdf_inq_vardimid";
  int status;

  status = nc_inq_vardimid(hfid, varid, dimids);

  if (HDF_Debug || status != NC_NOERR)
    fprintf (stderr, "hdf_inq_vardimid : %d %d \n", hfid, varid);

  if ( status != NC_NOERR )
    Error(func, "%s", nc_strerror(status));
}

void
hdf_inq_varnatts(int hfid, int varid, int *nattsp)
{
  static char func[] = "hdf_inq_varnatts";
  int status;

  status = nc_inq_varnatts(hfid, varid, nattsp);

  if (HDF_Debug || status != NC_NOERR)
    Message(func, "hfid = %d varid = %d nattsp = %d", hfid, varid, *nattsp);

  if ( status != NC_NOERR )
    Error(func, "%s", nc_strerror(status));
}

void
hdf_put_var_text(int hfid, int varid, const char *tp)
{
  static char func[] = "hdf_put_var_text";
  int status;

  status = nc_put_var_text(hfid, varid, tp);

  if (HDF_Debug || status != NC_NOERR)
    fprintf (stderr, "hdf_put_var_text : %d %d %s \n", hfid, varid, tp);

  if ( status != NC_NOERR )
    Error(func, "%s", nc_strerror(status));
}


void
hdf_put_var_short(int hfid, int varid, const short *sp)
{
  static char func[] = "hdf_put_var_short";
  int status;

  status = nc_put_var_short(hfid, varid, sp);

  if (HDF_Debug || status != NC_NOERR)
    fprintf (stderr, "hdf_put_var_short : %d %d %hd \n", hfid, varid, *sp);

  if (status != NC_NOERR)
    Error(func, "%s", nc_strerror(status));
}
*/
void
hdf_put_var_int(int hfid, int varid, const int *ip)
{
  static char func[] = "hdf_put_var_int";
  int status;

  status = H5Dwrite(varid, H5T_NATIVE_INT, H5S_ALL, H5S_ALL, H5P_DEFAULT, ip);

  if ( HDF_Debug || status < 0 )
    fprintf (stderr, "hdf_put_var_int : %d %d %d \n", hfid, varid, *ip);

  if ( status < 0 )
    {
      H5Eprint(NULL);
      Error(func, "write problem");
    }
}
/*
void
hdf_put_var_long(int hfid, int varid, const long *lp)
{
  static char func[] = "hdf_put_var_long";
  int status;

  status = nc_put_var_long(hfid, varid, lp);

  if (HDF_Debug || status != NC_NOERR)
    fprintf (stderr, "hdf_put_var_long : %d %d %ld \n", hfid, varid, *lp);

  if ( status != NC_NOERR )
    Error(func, "%s", nc_strerror(status));
}

void
hdf_put_var_float(int hfid, int varid, const float *fp)
{
  static char func[] = "hdf_put_var_float";
  int status;

  status = nc_put_var_float(hfid, varid, fp);

  if (HDF_Debug || status != NC_NOERR)
    fprintf (stderr, "hdf_put_var_float : %d %d %f \n", hfid, varid, *fp);

  if ( status != NC_NOERR )
    Error(func, "%s", nc_strerror(status));
}
*/
void  hdf_put_vara_double(int hfid, int varid, const size_t start[],
			  const size_t count[], const double *dp)
{
  static char func[] = "hdf_put_vara_double";
  int status;
  int i, ndims;
  int dataspace, filespace;
  int memspace;
  hsize_t hcount[9];
  hssize_t hstart[9];


  dataspace = H5Dget_space(varid);
  filespace = H5Dget_space(varid);

  ndims = H5Sget_simple_extent_ndims(dataspace);

  for ( i = 0; i < ndims; i++ )
    {
      /* printf("%s: %5d %5d\n", func, start[i], count[i]); */
      hstart[i] = start[i];
      hcount[i] = count[i];
      /* printf("%s: %5lld %5lld\n", func, hstart[i], hcount[i]); */
    }
  memspace = H5Screate_simple(ndims, hcount, NULL);

  status = H5Sselect_hyperslab(filespace, H5S_SELECT_SET, hstart, NULL, hcount, NULL);
  /*
  for ( i = 0; i < ndims; i++ )
    {
      printf("%s: %5d %5d\n", func, start[i], count[i]);
    }
  */
  /*
  if ( status < 0 ) H5Eprint(NULL);

  {
    hsize_t hdims[9], hmaxdims[9];

  H5Sget_simple_extent_dims(dataspace, hdims, hmaxdims);
  printf("data : %d", varid);
  for ( i = 0; i < ndims; i++ )
    printf("%4lld %4lld ", hdims[i], hmaxdims[i]);
  printf("\n");
  H5Sget_simple_extent_dims(filespace, hdims, hmaxdims);
  printf("file : %d", varid);
  for ( i = 0; i < ndims; i++ )
    printf("%4lld %4lld ", hdims[i], hmaxdims[i]);
  printf("\n");
  H5Sget_simple_extent_dims(memspace, hdims, hmaxdims);
  printf("mem  : %d", varid);
  for ( i = 0; i < ndims; i++ )
    printf("%4lld %4lld ", hdims[i], hmaxdims[i]);
  printf("\n");
  }
  */
  status = H5Dwrite(varid, H5T_NATIVE_DOUBLE, memspace, filespace, H5P_DEFAULT, dp);

  if ( HDF_Debug || status < 0 )
    Message(func, "hfid = %d varid = %d val0 = %f", hfid, varid, *dp);

  if ( status < 0 )
    {
      H5Eprint(NULL);
      Error(func, "write problem");
    }

  H5Sclose(memspace);
  H5Sclose(filespace);
  H5Sclose(dataspace);
}
/*
void 
hdf_get_vara_int (int hfid, int varid, const size_t start[],
		  const size_t count[], int *dp)
{
  static char func[] = "hdf_get_vara_int";
  int status;

  status = nc_get_vara_int(hfid, varid, start, count, dp);

  if ( HDF_Debug || status != NC_NOERR )
    Message(func, "hfid = %d varid = %d val0 = %d", hfid, varid, *dp);

  if ( status != NC_NOERR )
    Error(func, "%s", nc_strerror(status));
}

void 
hdf_get_vara_double (int hfid, int varid, const size_t start[],
		     const size_t count[], double *dp)
{
  static char func[] = "hdf_get_vara_double";
  int status;

  status = nc_get_vara_double(hfid, varid, start, count, dp);

  if ( HDF_Debug || status != NC_NOERR )
    Message(func, "hfid = %d varid = %d val0 = %f", hfid, varid, *dp);

  if ( status != NC_NOERR )
    Error(func, "%s", nc_strerror(status));
}
*/
void
hdf_put_var_double(int hfid, int varid, const double *dp)
{
  static char func[] = "hdf_put_var_double";
  int status;

  status = H5Dwrite(varid, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT, dp);

  if ( HDF_Debug || status < 0 )
    Message(func, "hfid = %d varid = %d val0 = %f", hfid, varid, *dp);

  if ( status < 0 )
    {
      H5Eprint(NULL);
      Error(func, "write problem");
    }
}
/*
void 
hdf_get_var1_double(int hfid, int varid, const size_t index[], double *dp)
{
  static char func[] = "hdf_get_var1_double";
  int status;

  status = nc_get_var1_double(hfid, varid, index, dp);

  if ( HDF_Debug || status != NC_NOERR )
    Message(func, "hfid = %d varid = %d val = %f", hfid, varid, *dp);

  if ( status != NC_NOERR )
    Error(func, "%s", nc_strerror(status));
}
*/
void
hdf_put_var1_double(int hfid, int varid, const size_t index[], const double *dp)
{
  static char func[] = "hdf_put_var1_double";
  int status;
  int filespace;
  int memspace;
  hsize_t hcount[1];
  hssize_t hstart[1];
    
  filespace = H5Dget_space(varid);

  hstart[0] = index[0];
  hcount[0] = 1;

  memspace = H5Screate_simple(1, hcount, NULL); 

  status = H5Sselect_hyperslab(filespace, H5S_SELECT_SET, hstart, NULL, hcount, NULL);

  if ( status < 0 ) H5Eprint(NULL);

  status = H5Dwrite(varid, H5T_NATIVE_DOUBLE, memspace, filespace, H5P_DEFAULT, dp);

  /*
  status = nc_put_var1_double(hfid, varid, index, dp);
  */
  if ( HDF_Debug || status < 0 )
    Message(func, "hfid = %d varid = %d val = %f", hfid, varid, *dp);

  if ( status < 0 )
    {
      H5Eprint(NULL);
      Error(func, "write problem");
    }

  H5Sclose(memspace);
  H5Sclose(filespace);
}
/*
void
hdf_get_var_text(int hfid, int varid, char *tp)
{
  static char func[] = "hdf_get_var_text";
  int status;

  status = nc_get_var_text(hfid, varid, tp);

  if (HDF_Debug || status != NC_NOERR)
    fprintf (stderr, "hdf_get_var_text : %d %d \n", hfid, varid);

  if (status != NC_NOERR)
    Error(func, "%s", nc_strerror(status));
}

void
hdf_get_var_short(int hfid, int varid, short *sp)
{
  static char func[] = "hdf_get_var_short";
  int status;

  status = nc_get_var_short(hfid, varid, sp);

  if (HDF_Debug || status != NC_NOERR)
    fprintf (stderr, "hdf_get_var_short : %d %d \n", hfid, varid);

  if ( status != NC_NOERR )
    Error(func, "%s", nc_strerror(status));
}

void
hdf_get_var_int(int hfid, int varid, int *ip)
{
  static char func[] = "hdf_get_var_int";
  int status;

  status = nc_get_var_int(hfid, varid, ip);

  if (HDF_Debug || status != NC_NOERR)
    fprintf (stderr, "hdf_get_var_int : %d %d \n", hfid, varid);

  if ( status != NC_NOERR )
    Error(func, "%s", nc_strerror(status));
}

void
hdf_get_var_long(int hfid, int varid, long *lp)
{
  static char func[] = "hdf_get_var_long";
  int status;

  status = nc_get_var_long(hfid, varid, lp);

  if (HDF_Debug || status != NC_NOERR)
    fprintf (stderr, "hdf_get_var_long : %d %d \n", hfid, varid);

  if ( status != NC_NOERR )
    Error(func, "%s", nc_strerror(status));
}

void
hdf_get_var_float(int hfid, int varid, float *fp)
{
  static char func[] = "hdf_get_var_float";
  int status;

  status = nc_get_var_float(hfid, varid, fp);

  if (HDF_Debug || status != NC_NOERR)
    fprintf (stderr, "hdf_put_var_float : %d %d \n", hfid, varid);

  if ( status != NC_NOERR )
    Error(func, "%s", nc_strerror(status));
}

void
hdf_get_var_double(int hfid, int varid, double *dp)
{
  static char func[] = "hdf_get_var_double";
  int status;

  status = nc_get_var_double(hfid, varid, dp);

  if (HDF_Debug || status != NC_NOERR)
    Message(func, "hfid = %d varid = %d val[0] = %f", hfid, varid, *dp);

  if ( status != NC_NOERR )
    Error(func, "%s", nc_strerror(status));
}

void
hdf_copy_att(int hfid_in, int varid_in, const char *name, int hfid_out,
	     int varid_out)
{
  static char func[] = "hdf_copy_att";
  int status;

  status = nc_copy_att(hfid_in, varid_in, name, hfid_out, varid_out);

  if (HDF_Debug || status != NC_NOERR)
    fprintf (stderr, "hdf_copy_att : %d %d %s %d %d\n", hfid_in, varid_out,
	     name, hfid_out, varid_out);

  if ( status != NC_NOERR )
    Error(func, "%s", nc_strerror(status));
}
*/
void hdf_put_att_text(int hfid, int varid, const char *name, size_t len, const char *tp)
{
  static char func[] = "hdf_put_att_text";
  int status;
  int aid, atype, obj, att;

  if ( varid == -1 )
    obj = H5Gopen(hfid, "/");
  else
    obj = varid;

  aid = H5Screate(H5S_SCALAR);
  atype = H5Tcopy(H5T_C_S1);
  H5Tset_size(atype, len);
  att = H5Acreate(obj, name, atype, aid, H5P_DEFAULT);
  status = H5Awrite(att, atype, tp);

  if ( HDF_Debug || status < 0 )
    Message(func, "hfid = %d varid = %d att = %s text = %s", hfid, varid, name, tp);

  if ( status < 0 )
    Error(func, "att problem");

  status = H5Sclose(aid);
  status = H5Tclose(atype);
  status = H5Aclose(att);
  if ( varid == -1 )
    status = H5Gclose(obj);
}
/*
void
hdf_put_att_int(int hfid, int varid, const char *name, nc_type xtype,
		size_t len, const int *ip)
{
  static char func[] = "hdf_put_att_int";
  int status;

  status = nc_put_att_int(hfid, varid, name, xtype, len, ip);

  if (HDF_Debug || status != NC_NOERR)
    Message(func, "hfid = %d varid = %d att = %s val = %d",
	    hfid, varid, name, *ip);

  if ( status != NC_NOERR )
    Error(func, "%s", nc_strerror(status));
}

void
hdf_put_att_double(int hfid, int varid, const char *name, nc_type xtype,
		   size_t len, const double *dp)
{
  static char func[] = "hdf_put_att_double";
  int status;

  status = nc_put_att_double(hfid, varid, name, xtype, len, dp);

  if (HDF_Debug || status != NC_NOERR)
    fprintf (stderr, "hdf_put_att_double : %d %d %f \n", hfid, varid, *dp);

  if ( status != NC_NOERR )
    Error(func, "%s", nc_strerror(status));
}
*/
void hdf_get_att_text(int hfid, int varid, char *name, char *tp)
{
  static char func[] = "hdf_get_att_text";
  int status;
  int aid, atype, obj, att;

  if ( varid == -1 )
    obj = H5Gopen(hfid, "/");
  else
    obj = varid;

  att = H5Aopen_name(obj, name);
  atype = H5Aget_type(att);
  aid = H5Aget_space(att);

  status = H5Aread(att, atype, tp);

  if ( HDF_Debug || status < 0 )
    Message(func, "hfid = %d varid = %d name = %s", hfid, varid, name);
  /*

  if ( status != NC_NOERR )
    Error(func, "%s", nc_strerror(status));
  */

  status = H5Sclose(aid);
  status = H5Tclose(atype);
  status = H5Aclose(att);
  if ( varid == -1 )
    status = H5Gclose(obj);
}

void hdf_get_att_int(int hfid, int varid, char *name, int *ip)
{
  static char func[] = "hdf_get_att_int";
  int status;
  int obj, att;

  if ( varid == -1 )
    obj = H5Gopen(hfid, "/");
  else
    obj = varid;

  att = H5Aopen_name(obj, name);

  status = H5Aread(att, H5T_NATIVE_INT, ip);

  if ( HDF_Debug || status < 0 )
    Message(func, "hfid = %d varid = %d att = %s val = %d",
	    hfid, varid, name, *ip);
  /*

  if ( status != NC_NOERR )
    Error(func, "%s", nc_strerror(status));
  */

  status = H5Aclose(att);
  if ( varid == -1 )
    status = H5Gclose(obj);
}

void hdf_get_att_double(int hfid, int varid, char *name, double *dp)
{
  static char func[] = "hdf_get_att_double";
  int status;
  int obj, att;

  if ( varid == -1 )
    obj = H5Gopen(hfid, "/");
  else
    obj = varid;

  att = H5Aopen_name(obj, name);

  status = H5Aread(att, H5T_NATIVE_DOUBLE, dp);

  if ( HDF_Debug || status < 0 )
    Message(func, "hfid = %d varid = %d att = %s val = %d",
	    hfid, varid, name, *dp);
  /*

  if ( status != NC_NOERR )
    Error(func, "%s", nc_strerror(status));
  */

  status = H5Aclose(att);
  if ( varid == -1 )
    status = H5Gclose(obj);
}

/*
void
hdf_inq_att(int hfid, int varid, const char *name, nc_type *xtypep,
	    size_t *lenp)
{
  static char func[] = "hdf_inq_att";
  int status;

  status = nc_inq_att(hfid, varid, name, xtypep, lenp);

  if (HDF_Debug || status != NC_NOERR)
    fprintf (stderr, "hdf_inq_att : %d %d \n", hfid, varid);

  if ( status != NC_NOERR )
    Error(func, "%s", nc_strerror(status));
}

void
hdf_inq_atttype(int hfid, int varid, const char *name, nc_type * xtypep)
{
  static char func[] = "hdf_inq_atttype";
  int status;

  status = nc_inq_atttype(hfid, varid, name, xtypep);

  if (HDF_Debug || status != NC_NOERR)
    fprintf (stderr, "hdf_inq_atttype : %d %d \n", hfid, varid);

  if ( status != NC_NOERR )
    Error(func, "%s", nc_strerror(status));
}
*/
void hdf_inq_attlen(int hfid, int varid, const char *name, size_t *lenp)
{
  static char func[] = "hdf_inq_attlen";
  int status = 0;
  int atype, obj, att;

  if ( varid == -1 )
    obj = H5Gopen(hfid, "/");
  else
    obj = varid;

  att = H5Aopen_name(obj, name);
  atype = H5Aget_type(att);

  *lenp = H5Tget_size(atype);

  if ( HDF_Debug || status < 0 )
    Message(func, "hfid = %d varid = %d name = %s len = %d",
	    hfid, varid, name, *lenp);
  /*
  if ( status != NC_NOERR )
    Error(func, "%s", nc_strerror(status));
  */

  status = H5Tclose(atype);
  status = H5Aclose(att);
  if ( varid == -1 )
    status = H5Gclose(obj);
}

void hdf_inq_attname(int hfid, int varid, int attnum, char *name)
{
  static char func[] = "hdf_inq_attname";
  int status;
  int obj, attid;
  size_t len;

  if ( varid == -1 )
    obj = H5Gopen(hfid, "/");
  else
    obj = varid;

  attid = H5Aopen_idx(obj, attnum);
  len = (size_t) H5Aget_name(attid, (size_t) 128, name);

  status = H5Aclose(attid);
  if ( varid == -1 )
    status = H5Gclose(obj);

  if ( HDF_Debug || status < 0 )
    Message(func, "hfid = %d varid = %d attnum = %d name = %s",
	    hfid, varid, attnum, name);

  /*
  if ( status != NC_NOERR )
    Error(func, "%s", nc_strerror(status));
  */
}
/*
void
hdf_inq_attid(int hfid, int varid, const char *name, int *attnump)
{
  static char func[] = "hdf_inq_attid";
  int status;

  status = nc_inq_attid(hfid, varid, name, attnump);

  if (HDF_Debug || status != NC_NOERR)
    fprintf (stderr, "hdf_inq_attid : %d %d \n", hfid, varid);

  if ( status != NC_NOERR )
    Error(func, "%s", nc_strerror(status));
}
*/

#endif
