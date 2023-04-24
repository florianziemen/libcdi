#include "cdi_borges.h"

#include <assert.h>
#include <netdb.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include "binary.h"
#include "cdi_int.h"
#include "dmemory.h"
#include "error.h"

#include <borges/borges.h>

typedef struct borg_info_t
{
  int streamid;
  bool shutdown;
  borges_client *borges_cl;
  borges_stream *borges_st;
} borg_info_t;

static borges_context *borges_ctx = NULL;
static int borges_ctx_refcount = 0;
static borg_info_t *handles[1024] = { NULL };

static int
write_buf(int sock, const void *buf, size_t nbytes)
{
  borg_info_t *info = handles[sock];
  return borges_stream_write(info->borges_st, buf, nbytes);
}

int
borges_write_grib_message(stream_t *streamptr, const void *gribbuffer, size_t nbytes)
{
  return write_buf(streamptr->fileID, gribbuffer, nbytes);
}

static int
ensure_context()
{
  if (borges_ctx == NULL)
    {
      if (borges_context_create(&borges_ctx) != 0)
        {
          Error("Borges Error: Failed to create Borges context");
          return -1;
        }
    }
  ++borges_ctx_refcount;
  return 0;
}

static void
free_context()
{
  if (--borges_ctx_refcount == 0)
    {
      borges_context_free(borges_ctx);
      borges_ctx = NULL;
    }
}

static const char *
parse_path_meta(const char *path, borges_meta *meta)
{
  char expid[512];
  const char *p = path;
  const char *expid_begin = p;
  for (; *p != '?' && *p != '\0'; ++p)
    {
      if ((*p < 'A' || *p > 'Z') && (*p < 'a' || *p > 'z') && (*p < '0' || *p > '9') && *p != '+' && *p != '.' && *p != '-'
          && *p != '_')
        {
          Error("Borges Error: Invalid experiment id format");
          return NULL;
        }
    }
  snprintf(expid, sizeof(expid), "%.*s", (int) (p - expid_begin), expid_begin);
  if (borges_meta_set(meta, "expid", expid) != 0)
    {
      Error("Borges Error: Failed to set experiment id");
      return NULL;
    }
  if (*p == '?')
    {
      for (++p; *p != '\0'; ++p)
        {
          const char *key_begin = p;
          for (; *p != '=' && *p != '\0'; ++p)
            {
              if ((*p < 'A' || *p > 'Z') && (*p < 'a' || *p > 'z') && (*p < '0' || *p > '9') && *p != '+' && *p != '.' && *p != '-'
                  && *p != '_')
                {
                  Error("Borges Error: Invalid key format");
                  return NULL;
                }
            }
          if (*p != '=')
            {
              Error("Borges Error: Invalid key-value format");
              return NULL;
            }
          const char *key_end = p;
          const char *value_begin = ++p;
          for (; *p != '&' && *p != '\0'; ++p)
            {
              if ((*p < 'A' || *p > 'Z') && (*p < 'a' || *p > 'z') && (*p < '0' || *p > '9') && *p != '+' && *p != '.' && *p != '-'
                  && *p != '_')
                {
                  Error("Borges Error: Invalid value format");
                  return NULL;
                }
            }
          const char *value_end = p;
          char key[256];
          char value[256];
          snprintf(key, sizeof(key), "%.*s", (int) (key_end - key_begin), key_begin);
          snprintf(value, sizeof(value), "%.*s", (int) (value_end - value_begin), value_begin);
          if (borges_meta_set(meta, key, value) != 0)
            {
              Error("Borges Error: Failed to set key-value");
              return NULL;
            }
        }
    }
  return p;
}

static int
parse_path(const char *path, char *addr, size_t addr_len, borges_meta *meta)
{
  // find the first '/' after the host:port
  const char *p = path;
  for (; *p != '/' && *p != '\0'; ++p)
    ;
  snprintf(addr, addr_len, "%.*s", (int) (p - path), path);
  // We expect a '/' here
  if (*p != '/')
    {
      Error("Borges Error: Invalid path format");
      return -1;
    }
  ++p;
  const char *p2 = parse_path_meta(p, meta);
  if (p2 == NULL)
    {
      Error("Borges Error: Failed to parse meta");
      return -1;
    }
  return 0;
}

static void
destroy_stream(borg_info_t *info)
{
  if (info->borges_cl != NULL)
    {
      borges_client_free(info->borges_cl);
      info->borges_cl = NULL;
    }
  info->borges_st = NULL;
  free_context();
}

static int
setup_stream(borg_info_t *info, const char *addr, borges_meta *meta)
{
  ensure_context();
  if (borges_client_create(&info->borges_cl, borges_ctx) != 0)
    {
      Error("Borges Error: Failed to create BORGES client");
      destroy_stream(info);
      return -1;
    }

  if (borges_client_connect(info->borges_cl, addr) != 0)
    {
      Error("Borges Error: Failed to connect to BORGES");
      destroy_stream(info);
      return -1;
    }

  if (borges_client_open_out_stream(info->borges_cl, &info->borges_st, meta) != 0)
    {
      Error("Borges Error: Failed to open BORGES stream");
      destroy_stream(info);
      return -1;
    }

  return 0;
}

int
borges_connect_int(const char *path, char filemode, stream_t *streamptr)
{
  borg_info_t *info = (borg_info_t *) Calloc(1, sizeof(borg_info_t));
  char addr[512];
  borges_meta *meta = NULL;

  if (borges_meta_create(&meta) != 0)
    {
      Error("Borges Error: Failed to create meta");
      goto cleanup;
    }

  // parse BORGES address format: borges://[<host>][:<port>]/<expid>[?<key>=<value>][&<key>=<value>][&...]
  if (parse_path(path, addr, sizeof(addr), meta) != 0)
    {
      Error("Borges Error: Failed to parse path");
      goto cleanup;
    }

  // create BORGES client
  if (setup_stream(info, addr, meta) != 0)
    {
      Error("Borges Error: Failed to setup stream");
      goto cleanup;
    }
cleanup:
  if (meta != NULL)
    {
      borges_meta_free(meta);
    }
}

int
borges_connect(const char *path, char filemode, stream_t *streamptr)
{
  if (filemode != 'w')
    {
      Warning("Reading from BORGES not implemented yet");  // TODO read from BORGES
      return CDI_EINVAL;
    }
  int sock = borges_connect_int(path, filemode, streamptr);
  return sock;
}

void
borges_disconnect(int sock)
{
  borg_info_t *info = handles[sock];
  if (info == NULL)
    {
      Error("Borges Error: Invalid socket %d", sock);
      return;
    }
  handles[sock] = NULL;
  destroy_stream(info);
  Free(info);
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
