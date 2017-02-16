#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 600
#endif

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>

#include "dmemory.h"

#include "pio_impl.h"
#include "pio_util.h"

int dbuffer_init(struct dBuffer **dbuffer, size_t size)
{
  int status;
  assert(dbuffer);
  struct dBuffer *db = *dbuffer = (struct dBuffer *)Malloc(sizeof (*db));
  db->wr_pointer = 0;

#ifndef _SX
  size_t pagesize = (size_t)(sysconf(_SC_PAGESIZE));

  xdebug("dbuffer_init(): pagesize = %zu bytes, size = %zu", pagesize, size);

  db->size = size = (size + pagesize - 1)/pagesize*pagesize;
  void *buf = NULL;
  if ( (status = posix_memalign(&buf, pagesize, size)) != 0 )
    {
      switch ( status )
	{
	case EINVAL:
	  xabort("The alignment argument was not a power of two,"
                 " or was not a multiple of sizeof(void *).");
#ifndef __GNUC__
	  break;
#endif
	case ENOMEM:
	  xabort("There was insufficient memory to fulfill the"
                 " allocation request.");
#ifndef __GNUC__
	  break;
#endif
	}
    }
  db->buffer = (unsigned char *)buf;
#else
  db->size = size;
  db->buffer = (unsigned char *)malloc(size);
  assert(db->buffer);
#endif
  return 0;
}

void dbuffer_cleanup ( struct dBuffer **dbuffer )
{
  struct dBuffer *db = *dbuffer;

  free( db->buffer );
  Free( db );

  return;
}

static size_t
dbuffer_freesize(struct dBuffer *dbuffer)
{
  size_t free_size = (size_t)(dbuffer->size - dbuffer_data_size(dbuffer));
  return free_size;
}

int
dbuffer_push(struct dBuffer *dbuffer, const void *buffer, size_t len)
{
  size_t space_left = dbuffer_freesize(dbuffer);
  if ( len <= space_left )
    {
      size_t wr_ptr = dbuffer->wr_pointer;
      memcpy(dbuffer->buffer + wr_ptr, buffer, len);
      dbuffer->wr_pointer = wr_ptr + len;
      return 0;
    }
  else
    return 1; /* not enough space left */
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
