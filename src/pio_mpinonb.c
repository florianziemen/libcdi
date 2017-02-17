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

struct fileMPIFWS
{
  char *name;
  size_t size;
  struct dBuffer *db1;
  struct dBuffer *db2;
  struct dBuffer *db;
  MPI_File fh;
  MPI_Request request;
  int fileID;
  int tsID;
};

static listSet *bibAFiledataM;

static int
fileIDTest(void *a, void *fileID)
{
  return ((struct fileMPIFWS *)a)->fileID == (int)(intptr_t)fileID;
}


/***************************************************************/

static struct fileMPIFWS *initAFiledataMPINONB ( const char *filename, size_t bs )
{
  MPI_Comm commPio = commInqCommPio();
  struct fileMPIFWS *of = (struct fileMPIFWS *)Malloc(sizeof (*of));
  {
    size_t nameSize = strlen(filename) + 1;
    char *name = of->name = Malloc(nameSize);
    memcpy(name, filename, nameSize);
  }
  of->size = bs;
  of->db1 = NULL;
  of->db2 = NULL;

  /* init output buffer */
  int iret = dbuffer_init ( &( of->db1 ), of->size );
  iret += dbuffer_init ( &( of->db2 ), of->size );

  if ( iret > 0 ) xabort ( "dbuffer_init did not succeed" );

  of->db = of->db1;

  of->tsID = CDI_UNDEFID;


  MPI_Info open_info = MPI_INFO_NULL;
  /* tell IBM PE to buffer just as much as one buffer holds */
  {
    xmpi(MPI_Info_create(&open_info));
    char buf_size_str[3*sizeof(size_t)*CHAR_BIT/8+1];
    snprintf(buf_size_str, sizeof (buf_size_str), "%zu", bs);
    xmpi(MPI_Info_set(open_info, "IBM_io_buffer_size", buf_size_str));
    xmpi(MPI_Info_set(open_info, "IBM_largeblock_io", "true"));
  }
  xmpi(MPI_File_open(commPio, of->name, MPI_MODE_CREATE|MPI_MODE_WRONLY,
                     open_info, &of->fh));
  xmpi(MPI_Info_free(&open_info));
  of->request = MPI_REQUEST_NULL;
  return of;
}

/***************************************************************/

static int
destroyAFiledataMPINONB(void *v)
{
  int iret = 0;
  struct fileMPIFWS *of;
  MPI_Status status;
  MPI_Offset endpos;

  of = (struct fileMPIFWS * ) v;

  xdebug("IOPE%d: close file %d, name=\"%s\"", commInqRankGlob(), of->fileID,
         of->name);

  /* close file */
  xmpi(MPI_Wait(&of->request, &status));
  xmpi(MPI_Barrier(commInqCommPio()));
  xmpi(MPI_File_get_position_shared(of->fh, &endpos));
  xmpi(MPI_File_set_size(of->fh, endpos));
  iret = MPI_File_close ( & ( of->fh ));

  /* file closed, cleanup */

  dbuffer_cleanup ( & ( of->db1 ));
  dbuffer_cleanup ( & ( of->db2 ));
  Free(of->name);
  of->name = NULL;
  Free( of );

  xdebug("IOPE%d: closed file, cleaned up, return", commInqRankGlob());

  return iret == MPI_SUCCESS ? 0 : -1;
}

/***************************************************************/

static bool
compareNamesMPINONB(void *v1, void *v2)
{
  struct fileMPIFWS *afm1 = v1, *afm2 = v2;
  return !strcmp(afm1->name, afm2->name);
}

/***************************************************************/

static void
writeMPINONB(struct fileMPIFWS *of)
{
  int amount;
  MPI_Status status;
  int fileID = of->fileID;

  /* write buffer */

  amount = ( int ) dbuffer_data_size ( of->db );

  if ( amount == 0 ) return;

  xdebug3("IOPI%d: Write buffer, size %d bytes, in",
          commInqRankGlob(), amount);

  xmpi ( MPI_Wait ( & ( of->request ), &status ));
  xmpi(MPI_File_iwrite_shared(of->fh, of->db->buffer, amount, MPI_UNSIGNED_CHAR,
                              &of->request));
  xdebug("%d bytes written for fileID=%d", amount, fileID);

  /* change outputBuffer */

  dbuffer_reset ( of->db );

  if ( of->db == of->db1 )
    {
        xdebug3("IOPE%d: fileID=%d, change to buffer 2 ...",
                commInqRankGlob(), fileID);
      of->db =  of->db2;
    }
  else
    {
        xdebug3("IOPE%d: fileID=%d, change to buffer 1 ...",
                commInqRankGlob(), fileID);
      of->db =  of->db1;
    }

  return;
}

/***************************************************************/

static size_t
fwMPINONB(int fileID, const void *buffer, size_t len, int tsID)
{
  int error = 0;
  int filled = 0;
  struct fileMPIFWS *of;
  int rankPio = commInqRankPio ();

  of = listSetGet(bibAFiledataM, fileIDTest, (void *)(intptr_t)fileID);
  xassert(of);

  bool flush = tsID != of->tsID;

  if (flush)
    {
      xdebug3("IOPE%d: tsID = %d, flush buffer", rankPio, tsID);
      writeMPINONB(of);
      of->tsID = tsID;
      MPI_Status status;
      xmpi(MPI_Wait(&(of->request), &status));
      xmpi(MPI_Barrier(commInqCommPio()));
    }

  filled = dbuffer_push(of->db, buffer, len);

  xdebug3("IOPE%d: fileID = %d, tsID = %d,"
          " pushed data on buffer, filled = %d",
          rankPio, fileID, tsID, filled);

  if ( filled == 1 )
    {
      if ( flush )
        error = filled;
      else
        {
          writeMPINONB(of);

          error = dbuffer_push(of->db, buffer, len);
        }
    }

  if ( error == 1 )
    xabort("did not succeed filling output buffer, fileID=%d", fileID);

  return len;
}

/***************************************************************/

static int fcMPINONB(int fileID)
{
  struct fileMPIFWS *of;

  xdebug("IOPE%d: write buffer, close file and cleanup, in %d",
         commInqRankPio(), fileID );

  if (!(of = listSetGet(bibAFiledataM, fileIDTest, (void *)(intptr_t)fileID)))
    xabort("listSet, fileID=%d not found", fileID);

  writeMPINONB(of);
  MPI_Status status;
  xmpi(MPI_Wait(&(of->request), &status));
  /* remove file element */
  int iret = listSetRemove(bibAFiledataM, fileIDTest, (void *)(intptr_t)fileID);
  return iret;
}

/***************************************************************/
static void
elemCheck(void *q, void *nm)
{
  struct fileMPIFWS *afm = q;
  const char *name = nm;

  if (!strcmp(name, afm->name))
    xabort("Filename %s has already been added to set\n", name);
}


static int
fowMPINONB(const char *filename, const char *mode)
{
  if ((mode[0] != 'w' && mode[0] != 'W') || mode[0] == 0 || mode[1] != 0)
    xabort("Unsupported mode \"%s\" in parallel file open.", mode);

  struct cdiPioConf *conf = cdiPioGetConf();

  listSetForeach(bibAFiledataM, elemCheck, (void *)filename);
  struct fileMPIFWS *of = initAFiledataMPINONB(filename, conf->writeAggBufLim);

  int id;
  if ((of->fileID = id = listSetAdd(bibAFiledataM, of)) < 0 )
    xabort("filename %s not unique", of->name);
  return id;
}

/***************************************************************/

static void finalizeMPINONB(void)
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
initMPINONB(void)
{
  bibAFiledataM = listSetNew( destroyAFiledataMPINONB, compareNamesMPINONB );

  namespaceSwitchSet(NSSWITCH_FILE_OPEN, NSSW_FUNC(fowMPINONB));
  namespaceSwitchSet(NSSWITCH_FILE_CLOSE, NSSW_FUNC(fcMPINONB));
  namespaceSwitchSet(NSSWITCH_FILE_WRITE, NSSW_FUNC(fwMPINONB));
  namespaceSwitchSet(cdiPioExtraNSKeys[cdiPioEKFileWritingFinalize],
                     NSSW_FUNC(finalizeMPINONB));

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
