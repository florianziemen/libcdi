#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_LIBNETCDF

#include <setjmp.h>
#include <stdbool.h>

#include <netcdf.h>
#if defined HAVE_PARALLEL_NC4 && defined HAVE_NETCDF_PAR_H
#include <netcdf_par.h>
#endif

#include "namespace.h"
#include "pio.h"
#include "cdipio.h"
#include "pio_comm.h"
#include "pio_cdf_int.h"
#include "pio_util.h"

#include "pio_cdf_int.h"

#if defined HAVE_PARALLEL_NC4
#if !defined TLS && defined HAVE_PTHREAD
pthread_key_t cdiPioCdfJmpKey;
#else
TLS struct cdiPioNcCreateLongJmpRetBuf *cdiPioCdfJmpBuf;
#endif

static int
cdiPio_nc__create(const char *path, int cmode, size_t initialsz, size_t *chunksizehintp, int *ncidp)
{
  int status, ioMode = commInqIOMode();
  if (ioMode != PIO_NONE)
    {
      if (cmode & NC_NETCDF4)
        {
          cmode |= NC_MPIPOSIX;
          status = nc_create_par(path, cmode, commInqCommColl(), MPI_INFO_NULL, ncidp);
          cdiPioCdfJmpBuf->openRank = CDI_PIO_COLLECTIVE_OPEN;
        }
      else
        {
#ifdef NC_PNETCDF
#if !defined TLS && defined HAVE_PTHREAD
          struct cdiPioNcCreateLongJmpRetBuf *cdiPioCdfJmpBuf = pthread_getspecific(cdiPioCdfJmpKey);
#endif
          /* which combination of cmode flags has already been tested? */
          static bool pnetcdfWontWork[] = {
            false, /* CDF-1 */
            false, /* CDF-2 */
            false  /* CDF-5 */
          };
          static const char cdfVers[] = { '1', '2', '5' };
          size_t cdfIdx;
          if (cmode & NC_64BIT_OFFSET)
            cdfIdx = 1;
          else if (cmode & NC_CLASSIC_MODEL)
            cdfIdx = 2;
          else
            cdfIdx = 0;
          MPI_Comm collComm = commInqCommColl();
          int rank = commInqRankColl();
          cmode |= NC_PNETCDF;
          if (!pnetcdfWontWork[cdfIdx])
            {
              status = nc_create_par(path, cmode, collComm, MPI_INFO_NULL, ncidp);
              if (status == NC_EINVAL)
                {
                  if (rank == 0)
                    fprintf(stderr,
                            "warning: parallel create not implemented"
                            " for cdf-%c format!\n",
                            cdfVers[cdfIdx]);
                  pnetcdfWontWork[cdfIdx] = true;
                }
              else
                cdiPioCdfJmpBuf->openRank = CDI_PIO_COLLECTIVE_OPEN;
            }
          if (pnetcdfWontWork[cdfIdx])
            {
              /* no pnetcdf is implied if not even NC_PNETCDF is defined */
              cmode &= ~NC_PNETCDF;
#endif
              if (rank == cdiPioCdfJmpBuf->openRank)
                status = nc__create(path, cmode, initialsz, chunksizehintp, ncidp);
              else
                longjmp(cdiPioCdfJmpBuf->jmpBuf, 1);
#ifdef NC_PNETCDF
            }
#endif
        }
    }
  else
    status = nc__create(path, cmode, initialsz, chunksizehintp, ncidp);
  return status;
}

static void
cdiPioCdfDefVar(int ncid, const char *name, nc_type xtype, int ndims, const int dimids[], int *varidp)
{
  cdf_def_var_serial(ncid, name, xtype, ndims, dimids, varidp);
  int cf_format;
  int status = nc_inq_format(ncid, &cf_format);
  if (status != NC_NOERR) Error("%s", nc_strerror(status));
  if (commInqIOMode() != PIO_NONE && cf_format == NC_FORMAT_NETCDF4)
    {
      xdebug("%s", "calling nc_var_par_access");
      int status = nc_var_par_access(ncid, *varidp, NC_COLLECTIVE);
      if (status != NC_NOERR) Error("%s", nc_strerror(status));
    }
}

void
cdiPioEnableNetCDFParAccess(void)
{
  namespaceSwitchSet(NSSWITCH_NC__CREATE, NSSW_FUNC(cdiPio_nc__create));
  namespaceSwitchSet(NSSWITCH_CDF_DEF_VAR, NSSW_FUNC(cdiPioCdfDefVar));
#if !defined TLS && defined HAVE_PTHREAD
  int ierror = pthread_key_create(&cdiPioCdfJmpKey, NULL);
  if (ierror)
    {
      Error("%s: error creating pthread key: %s\n", __func__, strerror(ierror));
    }
#endif
}

void
cdiPioDisableNetCDFParAccess(void)
{
#if !defined TLS && defined HAVE_PTHREAD
  int ierror = pthread_key_delete(cdiPioCdfJmpKey);
  if (ierror)
    {
      Error("%s: error deleting pthread key: %s\n", __func__, strerror(ierror));
    }
#endif
}
#endif /* ifdef HAVE_PARALLEL_NC4 */

#endif /* ifdef HAVE_LIBNETCDF */
/*
 * Local Variables:
 * c-file-style: "Java"
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * show-trailing-whitespace: t
 * require-trailing-newline: t
 * End:
 */
