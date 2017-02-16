#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <mpi.h>

#include "cdi.h"
#include "dmemory.h"
#include "namespace.h"
#include "pio.h"
#include "pio_comm.h"
#include "pio_impl.h"
#include "pio_util.h"

struct fileMPIFWO
{
  char *name;
  MPI_File fh;
  int fileID;
};

static listSet *bibAFiledataM;

static int
fileIDTest(void *a, void *fileID)
{
  return ((struct fileMPIFWO *)a)->fileID == (int)(intptr_t)fileID;
}


/***************************************************************/

static struct fileMPIFWO *
initAFiledataFileWriteOrdered(const char *filename, size_t bs)
{
  MPI_Comm commPio = commInqCommPio();
  size_t nameSize = strlen(filename) + 1;
  struct fileMPIFWO *of = Malloc(sizeof (*of));
  of->name = Malloc(nameSize);
  strcpy(of->name, filename);

  MPI_Info open_info = MPI_INFO_NULL;
  xmpi(MPI_Info_create(&open_info));
  xmpi(MPI_Info_set(open_info, "access_style", "sequential,write_once"));
  xmpi(MPI_Info_set(open_info, "collective_buffering", "true"));
  /* tell IBM PE to buffer just as much as one buffer holds */
  {
    char buf_size_str[3*sizeof(size_t)*CHAR_BIT/8+1];
    snprintf(buf_size_str, sizeof (buf_size_str), "%zu", bs);
    xmpi(MPI_Info_set(open_info, "IBM_io_buffer_size", buf_size_str));
    xmpi(MPI_Info_set(open_info, "IBM_largeblock_io", "false"));
  }
  xmpi(MPI_File_open(commPio, of->name,
                     MPI_MODE_CREATE|MPI_MODE_WRONLY|MPI_MODE_UNIQUE_OPEN,
                     open_info, &of->fh));
  xmpi(MPI_Info_free(&open_info));

  return of;
}

/***************************************************************/

static int
destroyAFiledataFileWriteOrdered(void *v)
{
  struct fileMPIFWO *of = v;
  /* close file */
  MPI_Offset endpos, fsize;
  xmpi(MPI_File_get_position_shared(of->fh, &endpos));
  xmpi(MPI_File_get_size(of->fh, &fsize));
  /* does the file need to be truncated? */
  MPI_Comm commPio = commInqCommPio();
  int trailingOctets = fsize > endpos;
  xmpi(MPI_Allreduce(MPI_IN_PLACE, &trailingOctets, 1, MPI_INT, MPI_LOR,
                     commPio));
  if (trailingOctets)
    xmpi(MPI_File_set_size(of->fh, endpos));
  int iret = MPI_File_close(&of->fh);
  Free(of->name);
  of->name = NULL;
  Free(of);

  return iret == MPI_SUCCESS ? 0 : -1;
}

/***************************************************************/

static bool
compareNamesFileWriteOrdered(void *v1, void *v2)
{
  struct fileMPIFWO *afm1 = v1, *afm2 = v2;
  return !strcmp(afm1->name, afm2->name);
}

/***************************************************************/

static size_t
fwFileWriteOrdered(int fileID, const void *buffer, size_t len)
{
  struct fileMPIFWO *of
    = listSetGet(bibAFiledataM, fileIDTest, (void *)(intptr_t)fileID);
  xassert(of);

  /* write buffer */
  xassert(len <= INT_MAX);
  MPI_Status status;
  xmpi(MPI_File_write_ordered(of->fh, (void *)buffer, (int)len,
                              MPI_UNSIGNED_CHAR, &status));
  return len;
}

/***************************************************************/

static int fcFileWriteOrdered(int fileID)
{
  struct fileMPIFWO *of
    = listSetGet(bibAFiledataM, fileIDTest, (void *)(intptr_t)fileID);
  if (!of)
    xabort("listSet, fileID=%d not found", fileID);
  int iret = listSetRemove(bibAFiledataM, fileIDTest, (void *)(intptr_t)fileID);
  return iret;
}

/***************************************************************/
static void
elemCheck(void *q, void *nm)
{
  struct fileMPIFWO *afm = q;
  const char *name = nm;

  if (!strcmp(name, afm->name))
    xabort("Filename %s has already been added to set\n", name);
}


static int
fowFileWriteOrdered(const char *filename, const char *mode)
{
  if ((mode[0] != 'w' && mode[0] != 'W') || mode[0] == 0 || mode[1] != 0)
    xabort("Unsupported mode \"%s\" in parallel file open.", mode);

  struct cdiPioConf *conf = cdiPioGetConf();

  listSetForeach(bibAFiledataM, elemCheck, (void *)filename);
  struct fileMPIFWO *of
    = initAFiledataFileWriteOrdered(filename, conf->writeAggBufLim);

  int id;
  if ((of->fileID = id = listSetAdd(bibAFiledataM, of)) < 0 )
    xabort("filename %s not unique", of->name);
  return id;
}

/***************************************************************/

static void finalizeFileWriteOrdered(void)
{
  if (!listSetIsEmpty(bibAFiledataM))
    xabort("set bibAFiledataM not empty");
  else
    {
      xdebug("%s", "destroy set");
      listSetDelete(bibAFiledataM);
    }
}

/***************************************************************/

void
cdiPioFileWriteOrderedInit(void)
{
  bibAFiledataM = listSetNew( destroyAFiledataFileWriteOrdered, compareNamesFileWriteOrdered );

  namespaceSwitchSet(NSSWITCH_FILE_OPEN, NSSW_FUNC(fowFileWriteOrdered));
  namespaceSwitchSet(NSSWITCH_FILE_CLOSE, NSSW_FUNC(fcFileWriteOrdered));
  namespaceSwitchSet(NSSWITCH_FILE_WRITE, NSSW_FUNC(fwFileWriteOrdered));
  namespaceSwitchSet(cdiPioExtraNSKeys[cdiPioEKFileWritingFinalize],
                     NSSW_FUNC(finalizeFileWriteOrdered));

  if ( bibAFiledataM == NULL )
    xabort ( "listSetNew did not succeed" );
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
