#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>

#include "cdf.h"
#include "cdi.h"
#include "cdi_int.h"
#include "cdf_int.h"

#ifdef HAVE_LIBNETCDF
#include "error.h"
#include <netcdf.h>

const char *
cdfLibraryVersion(void)
{
  return nc_inq_libvers();
}

int CDF_Debug = 0;  // If set to 1, debugging

void
cdfDebug(int debug)
{
  CDF_Debug = debug;
  if (CDF_Debug) Message("debug level %d", debug);
}

static void
cdfComment(int ncid)
{
  static char comment[256] = "Climate Data Interface version";
  static bool init = false;
  if (!init)
  {
    init = true;
    const char *libvers = cdiLibraryVersion();
    size_t len = strlen(comment);
    snprintf(comment + len, sizeof(comment) - len, " %s %s", isdigit((int) *libvers) ? libvers : "??",
             "(https://mpimet.mpg.de/cdi)");
  }

  cdf_put_att_text(ncid, NC_GLOBAL, "CDI", strlen(comment), comment);
}

static bool
has_uri_scheme(const char *uri)
{
  const char *pos = strstr(uri, "://");
  if (pos)
  {
    size_t len = (size_t) (pos - uri);
    if (strncmp(uri, "file", len) == 0 || strncmp(uri, "https", len) == 0 || strncmp(uri, "s3", len) == 0) return true;
  }

  return false;
}

static int
cdf_open_read(const char *filename, int *filetype)
{
  int ncid = -1;
  int readMode = NC_NOWRITE;
  int status = cdf_open(filename, readMode, &ncid);
  if (status > 0 && ncid < 0) return CDI_ESYSTEM;
  if (status < -1000 && ncid == -1) return status;

  int format = -1;
  status = nc_inq_format(ncid, &format);
  if (status == NC_NOERR && format == NC_FORMAT_NETCDF4_CLASSIC) { *filetype = CDI_FILETYPE_NC4C; }

#ifdef NC_FORMATX_NCZARR
  int modeNC;
  status = nc_inq_format_extended(ncid, &format, &modeNC);
  if (status == NC_NOERR && format == NC_FORMATX_NCZARR) { *filetype = CDI_FILETYPE_NCZARR; }
#endif

  return ncid;
}

static int
cdf_open_write(const char *filename, int filetype)
{
  int ncid = -1;
  int writemode = NC_CLOBBER;

#ifdef NC_64BIT_OFFSET
  if (filetype == CDI_FILETYPE_NC2) writemode |= NC_64BIT_OFFSET;
#endif
#ifdef NC_64BIT_DATA
  if (filetype == CDI_FILETYPE_NC5) writemode |= NC_64BIT_DATA;
#endif
  if (filetype == CDI_FILETYPE_NC4C) writemode |= (NC_NETCDF4 | NC_CLASSIC_MODEL);
  if (filetype == CDI_FILETYPE_NC4) writemode |= NC_NETCDF4;
  if (filetype == CDI_FILETYPE_NCZARR) writemode |= NC_NETCDF4;
  if (filetype == CDI_FILETYPE_NCZARR)
  {
    if (!has_uri_scheme(filename))
    {
      fprintf(stderr, "URI scheme is missing in NCZarr path!\n");
      return CDI_EINVAL;
    }

    cdf_create(filename, writemode, &ncid);
  }
  else
  {
    if (has_uri_scheme(filename)) fprintf(stderr, "URI scheme defined for non NCZarr Data Model!\n");

    cdf__create(filename, writemode, &ncid);
  }

  return ncid;
}

static int
cdfOpenFile(const char *filename, const char *mode, int *filetype)
{
  if (filename == NULL) return CDI_EINVAL;

  int ncid = -1;
  int fmode = tolower(*mode);
  switch (fmode)
  {
    case 'r': ncid = cdf_open_read(filename, filetype); break;
    case 'w':
      ncid = cdf_open_write(filename, *filetype);
      if (ncid != CDI_EINVAL)
      {
        if (CDI_Version_Info) cdfComment(ncid);
        cdf_put_att_text(ncid, NC_GLOBAL, "Conventions", 6, "CF-1.6");
      }
      break;
    case 'a': cdf_open(filename, NC_WRITE, &ncid); break;
    default: ncid = CDI_EINVAL;
  }

  return ncid;
}

int
cdfOpen(const char *fileName, const char *mode, int fileType)
{
  int fileID = -1;
  bool openFile = true;

  if (CDF_Debug) Message("Open %s with mode %c", fileName, *mode);

#ifndef NC_64BIT_OFFSET
  if (fileType == CDI_FILETYPE_NC2) open_file = false;
#endif
#ifndef NC_64BIT_DATA
  if (fileType == CDI_FILETYPE_NC5) open_file = false;
#endif

  if (openFile)
  {
    fileID = cdfOpenFile(fileName, mode, &fileType);
    if (CDF_Debug) Message("File %s opened with id %d", fileName, fileID);
  }
  else
  {
    fileID = CDI_ELIBNAVAIL;
  }

  return fileID;
}

int
cdf4Open(const char *fileName, const char *mode, int *fileType)
{
  if (CDF_Debug) Message("Open %s with mode %c", fileName, *mode);

  int fileID = cdfOpenFile(fileName, mode, fileType);
  if (CDF_Debug) Message("File %s opened with id %d", fileName, fileID);
  return fileID;
}

static void
cdfCloseFile(int fileID)
{

  if (CDF_Debug) Message("Closing cdf file: %d", fileID);
  cdf_close(fileID);
}

void
cdfClose(int fileID)
{
  cdfCloseFile(fileID);
}

int
cdf_inq_group(int fileID, const char *groupName)
{
  int ncGroupID = -1;
  int status = nc_inq_ncid(fileID, groupName, &ncGroupID);
  int groupID = (status == NC_NOERR) ? ncGroupID : status - 1000;

  if (status != NC_NOERR)
  {
    Warning("Group %s not found!", groupName);
    int numGroups = 0;
    int ncids[NC_MAX_VARS];
    char gname[CDI_MAX_NAME];
    nc_inq_grps(fileID, &numGroups, ncids);
    printf("Available groups: ");
    for (int i = 0; i < numGroups; ++i)
    {
      nc_inq_grpname(ncids[i], gname);
      if (i > 0) printf(",");
      printf("%s", gname);
    }
    printf("\n");
  }

  return groupID;
}

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
