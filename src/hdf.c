#if defined (HAVE_CONFIG_H)
#  include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>

#include "hdf.h"
#include "cdi.h"
#include "stream_int.h"
#include "hdf_int.h"


const char *hdfLibraryVersion(void)
{
#if  defined  (HAVE_LIBHDF5)
  static char libversion[128];
  unsigned majnum, minnum, relnum;
  H5get_libversion(&majnum, &minnum, &relnum);
  sprintf(libversion, "%u.%u.%u", majnum, minnum, relnum);
  return (libversion);
#else
  return ("library undefined");
#endif
}


int HDF_Debug   = 0;    /* If set to 1, debugging           */


void hdfDebug(int debug)
{
  static char func[] = "hdfDebug";

  HDF_Debug = debug;

  if ( HDF_Debug )
    Message(func, "debug level %d", debug);
}

void hdfComment(int hfid)
{
#if  defined  (HAVE_LIBHDF5)
  static char comment[256] = "using CDI library version ";
  static int init = 0;
  char *blank;
  int size = 0;

  if ( ! init )
    {
      init = 1;
      blank = strchr(cdiLibraryVersion(), ' ');
      if ( blank ) size = blank - cdiLibraryVersion();

      if ( size == 0 || ! isdigit((int) *cdiLibraryVersion()) )
	strcat(comment, "??");
      else
	strncat(comment, cdiLibraryVersion(), size);
  /*  strcat(comment, " available from http://.../software/cdi.html"); */
    }

  hdf_put_att_text(hfid, -1, "comment", strlen(comment), comment);

#endif
}

int hdfOpenFile(const char *filename, const char *mode)
{
  int hfid = 0;
#if  defined  (HAVE_LIBHDF5)
  static char func[] = "hdfOpenFile";
  int fmode;

  switch (*mode)
    {
    case 'r':
    case 'R':
      Error(func, "read of HDF5 files unsupported\n");
      /*
      fmode = 'r';
      hfid = hdf_open(filename, H5F_ACC_RDONLY);
      */
      break;
    case 'w':
    case 'W':
      fmode = 'w';
      hfid = hdf_create(filename, H5F_ACC_TRUNC);
      hdfComment(hfid);

      break;
    /*
    case 'a':
    case 'A':
      fmode = 'a';
      hdf_open(filename, NC_APPEND, &hfid);
      break;
    */
    default:
      Error(func, "mode %c unexpected\n", *mode);
    }

#endif
  return (hfid);
}

int hdfOpen(const char *filename, const char *mode)
{
  static char func[] = "hdfOpen";
  int fileID = 0;

  if ( HDF_Debug )
    Message(func, "open %s with mode %c", filename, *mode);

  if ( !filename )
    {
      Error(func, "filename undefined\n");
      return (-1);
    }

  fileID = hdfOpenFile(filename, mode);

  if ( HDF_Debug )
    Message(func, "file %s opened with id %d", filename, fileID);

  return (fileID);
}

void hdfCloseFile(int fileID)
{
#if  defined  (HAVE_LIBHDF5)
  hdf_close(fileID);
#endif
}

void hdfClose(int fileID)
{
  hdfCloseFile(fileID);
}
