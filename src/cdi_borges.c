#include "cdi_borges.h"
#define HAVE_BORGES
#ifdef HAVE_BORGES

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

#define BORGES_SECTION2_VERSION 1
#define EXPID_MIN_LENGTH 7
#define BORGES_DEFAULT_PORT "7979"

static int borges_ev_handler(borges_connection *conn, borges_event_type event_type, void *event_data, void *userp);

typedef struct borges_info_t
{
  const char *expid;
  int streamid;
  bool shutdown;
  borges_context borges_ctx;
  borges_connection borges_conn;
  struct
  {
    const void *ptr;
    size_t size;
  } buffer;
  uint8_t meta[512];
  size_t meta_len;
} borges_info_t;

static borges_info_t *handles[1024] = {NULL};

static void log_err(const borges_err *err, const char *msg)
{
  char err_msg[256];
  borges_err_snprint(err, err_msg, sizeof(err_msg));
  Error("Borges Error: %s, %s", msg, err_msg);
}

static int borges_write_buf(int sock, const void *buf, size_t nbytes)
{
  borges_info_t *info = handles[sock];
  info->buffer.ptr = buf;
  info->buffer.size = nbytes;
  borges_connection_unpause_stream(&info->borges_conn, info->streamid);
  borges_context_run(&info->borges_ctx);
  if (info->shutdown)
    return -1;
  return 0;
}

int borges_write_grib_message(stream_t *streamptr, const void *gribbuffer, size_t nbytes)
{
  return borges_write_buf(streamptr->fileID, gribbuffer, nbytes);
}

static int parse(const char *path, const char **host, size_t *host_len, const char **port, size_t *port_len, const char **meta, size_t *meta_len)
{
  const char *p = path;
  if (*(p++) != 'b')
    return -1;
  if (*(p++) != 'o')
    return -1;
  if (*(p++) != 'r')
    return -1;
  if (*(p++) != 'g')
    return -1;
  if (*(p++) != 'e')
    return -1;
  if (*(p++) != 's')
    return -1;
  if (*(p++) != ':')
    return -1;
  if (*(p++) != '/')
    return -1;
  if (*(p++) != '/')
    return -1;
  *host = p;
  for (; *p != '/' && *p != ':' && *p != '\0'; ++p)
  {
    if ((*p < 'A' || *p > 'Z') && (*p < 'a' || *p > 'z') && *p != '+' && *p != '.' && *p != '-')
      return -1;
  }
  *host_len = (p - *host);
  if (*p == ':')
  {
    *port = ++p;
    for (; *p != '/' && *p != '\0'; ++p)
    {
      if (*p < '0' || *p > '9')
        return -1;
    }
    *port_len = (p - *port);
  }
  if (*p == '/')
  {
    *meta = ++p;
    for (; *p != '\0'; ++p)
    {
      if ((*p < 'A' || *p > 'Z') && (*p < 'a' || *p > 'z') && *p != '+' && *p != '.' && *p != '-' && *p != '=')
        return -1;
    }
    *meta_len = (p - *meta);
  }
  return 0;
}

int borges_connect_int(const char *path, char filemode, stream_t *streamptr)
{
  // parse BORGES address format: borges://<host>[:<port>]/[meta_info]
  const char *host = NULL;
  size_t host_len = 0;
  const char *port = NULL;
  size_t port_len = 0;
  const char *meta = NULL;
  size_t meta_len = 0;
  if (parse(path, &host, &host_len, &port, &port_len, &meta, &meta_len) < 0)
  {
    Warning("Invalid path");
    return -1;
  }
  if (port = NULL)
  {
    port = BORGES_DEFAULT_PORT;
    meta_len = sizeof(BORGES_DEFAULT_PORT);
  }

  borges_info_t *info = (borges_info_t *)Calloc(1, sizeof(borges_info_t));
  memcpy(info->meta, meta, meta_len);
  info->meta_len = meta_len;

  borges_err err;
  if (borges_context_init(&info->borges_ctx, &err) < 0)
  {
    log_err(&err, "Failed to init borges context");
    Free(info);
    return -1;
  }
  char borges_path[256];
  snprintf(borges_path, sizeof(borges_path), "tcp://%*s:%*s", host_len, host, port_len, port);
  if (borges_connection_init(&info->borges_conn, &info->borges_ctx, borges_path, borges_ev_handler, info, &err) < 0)
  {
    Warning("Path is %s", path);
    log_err(&err, "Failed to init borges connection");
    borges_context_cleanup(&info->borges_ctx);
    Free(info);
    return -1;
  }
  borges_context_run(&info->borges_ctx);
  if (info->shutdown)
  {
    borges_connection_cleanup(&info->borges_conn);
    borges_context_cleanup(&info->borges_ctx);
    Free(info);
    return -1;
  }
  streamptr->protocolData = info;
  for (int idx = 0; idx < sizeof(handles) / sizeof(handles[0]); ++idx)
  {
    if (handles[idx] == NULL)
    {
      handles[idx] = info;
      return idx;
    }
  }

  borges_connection_cleanup(&info->borges_conn);
  borges_context_cleanup(&info->borges_ctx);
  Free(info);
  return -1;
}

int borges_connect(const char *path, char filemode, stream_t *streamptr)
{
  if (filemode != 'w')
  {
    Warning("Reading from BORGES not implemented yet"); // TODO read from BORGES
    return CDI_EINVAL;
  }
  int sock = borges_connect_int(path, filemode, streamptr);
  return sock;
}

void borges_disconnect(int sock)
{
  borges_info_t *info = handles[sock];
  info->shutdown = true;
  borges_context_run(&info->borges_ctx);
  borges_connection_cleanup(&info->borges_conn);
  borges_context_cleanup(&info->borges_ctx);
  Free(info);
}

static int borges_ev_handler(borges_connection *conn, borges_event_type event_type, void *event_data, void *userp)
{
  borges_info_t *info = userp;
  switch (event_type)
  {
  case BORGES_EVENT_TYPE_CONNECT:
  {
    borges_err err;
    if (borges_connection_open_stream(conn, BORGES_STREAM_MODE_OUT, info->meta, info->meta_len, &err) < 0)
    {
      log_err(&err, "Failed to open stream");
      return -1;
    }
    break;
  }
  case BORGES_EVENT_TYPE_OPEN_STREAM:
  {
    struct borges_event_open_stream ev;
    info->streamid = ev.id;
    break;
  }
  case BORGES_EVENT_TYPE_DATA_OUT:
  {
    struct borges_event_data_out *out = event_data;
    if (info->shutdown == true)
    {
      borges_err err;
      if (borges_connection_close_stream(conn, info->streamid, &err) < 0)
        log_err(&err, "Failed to close stream");
      return 0;
    }

    if (info->buffer.size == 0)
      return BORGES_USER_STATUS_PAUSE;
    size_t chunk_len = out->len < info->buffer.size ? out->len : info->buffer.size;
    memcpy(out->data, info->buffer.ptr, chunk_len);
    info->buffer.ptr += chunk_len;
    info->buffer.size -= chunk_len;
    return chunk_len;
  }
  case BORGES_EVENT_TYPE_ERROR:
  {
    struct borges_event_error *err_event = event_data;
    log_err(err_event->err, "Failed to execute task");
    info->shutdown = true;
    break;
  }
  default:
    break;
  }
  return 0;
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
