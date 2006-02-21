#ifndef _HDF_INT_H
#define _HDF_INT_H

#if  defined  (HAVE_LIBHDF5)

#include "hdf5.h"

#define  HDF_GLOBAL  -1
int    hdf_create(const char *path, int cmode);
int    hdf_open(const char *path, int omode);
void   hdf_close(int hfid);

/*

void hdf_redef (int hfid);
void hdf_enddef (int hfid);

int  hdf_inq_rec (int fileID, int *code, int *gridid, int *levelID, int *date, int *time);

void hdf_inq (int hfid, int *ndimsp, int *nvarsp, int *ngattsp, int *unlimdimidp);

void hdf_def_dim (int hfid, const char *name, size_t len, int *idp);
void hdf_inq_dimid (int hfid, const char *name, int *dimidp);
void hdf_inq_dim (int hfid, int dimid, char *name, size_t * lengthp);
void hdf_inq_dimname (int hfid, int dimid, char *name);
void hdf_inq_dimlen (int hfid, int dimid, size_t * lengthp);
*/
void hdf_def_var(int hfid, const char *name, int xtype, int ndims,
		 const int dimids[], int *varidp, int plistid);

void hdf_inq_varid (int hfid, const char *name, int *varidp);

int  hdf_inq_nvars(int hfid);
int  hdf_inq_ngatts(int hfid);

void hdf_inq_vartype (int hfid, int varid, int *xtypep);
int  hdf_inq_varndims(int varid);
int  hdf_inq_varxtype(int varid);
void hdf_inq_vardimid (int hfid, int varid, int dimids[]);
void hdf_inq_varnatts (int hfid, int varid, int *nattsp);
/*
void hdf_copy_att (int hfid_in, int varid_in, const char *name, int hfid_out, int varid_out);
void hdf_put_var_text   (int hfid, int varid, const char *tp);
void hdf_put_var_uchar  (int hfid, int varid, const unsigned char *up);
void hdf_put_var_schar  (int hfid, int varid, const signed char *cp);
void hdf_put_var_short  (int hfid, int varid, const short *sp);
*/
void hdf_put_var_int    (int hfid, int varid, const int *ip);
/*
void hdf_put_var_long   (int hfid, int varid, const long *lp);
void hdf_put_var_float  (int hfid, int varid, const float *fp);
*/
void hdf_put_var_double (int hfid, int varid, const double *dp);
/*
void hdf_get_var_text   (int hfid, int varid, char *tp);
void hdf_get_var_uchar  (int hfid, int varid, unsigned char *up);
void hdf_get_var_schar  (int hfid, int varid, signed char *cp);
void hdf_get_var_short  (int hfid, int varid, short *sp);
void hdf_get_var_int    (int hfid, int varid, int *ip);
void hdf_get_var_long   (int hfid, int varid, long *lp);
void hdf_get_var_float  (int hfid, int varid, float *fp);
void hdf_get_var_double (int hfid, int varid, double *dp);

void hdf_get_var1_double(int hfid, int varid, const size_t index[], double *dp);
*/
void hdf_put_var1_double(int hfid, int varid, const size_t index[], const double *dp);
/*
void hdf_get_vara_double(int hfid, int varid, const size_t start[],
                         const size_t count[], double *dp);
*/
void hdf_put_vara_double(int hfid, int varid, const size_t start[],
                         const size_t count[], const double *dp);
void hdf_put_att_text (int hfid, int varid, const char *name, size_t len,
		      const char *tp);
/*
void hdf_put_att_int (int hfid, int varid, const char *name, int xtype,
		     size_t len, const int *ip);
void hdf_put_att_double (int hfid, int varid, const char *name, int xtype,
			size_t len, const double *dp);

*/
void hdf_get_att_text (int hfid, int varid, char *name, char *tp);
void hdf_get_att_int (int hfid, int varid, char *name, int *ip);
void hdf_get_att_double (int hfid, int varid, char *name, double *dp);

void hdf_inq_att (int hfid, int varid, const char *name, int *xtypep,
		 size_t * lenp);
void hdf_inq_atttype (int hfid, int varid, const char *name, int *xtypep);
void hdf_inq_attlen (int hfid, int varid, const char *name, size_t *lenp);
void hdf_inq_attname (int hfid, int varid, int attnum, char *name);
void hdf_inq_attid (int hfid, int varid, const char *name, int *attnump);

#endif

#endif  /* _HDF_INT_H */
