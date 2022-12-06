#include "cdi_borges.h"

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

#define BORGES_DEFAULT_PORT "7979"

static int borges_ev_handler(borg_connection *conn, borg_event_type event_type, void *event_data, void *userp);

typedef struct borg_info_t
{
  const char *expid;
  int streamid;
  bool shutdown;
  borg_context borg_ctx;
  borg_connection borg_conn;
  struct
  {
    const void *ptr;
    size_t size;
  } buffer;
  uint8_t meta[512];
  size_t meta_len;
} borg_info_t;

static borg_info_t *handles[1024] = {NULL};

static void log_err(const borg_err *err, const char *msg)
{
  char err_msg[256];
  borg_err_snprint(err, err_msg, sizeof(err_msg));
  Error("Borges Error: %s, %s", msg, err_msg);
}

static int borges_write_buf(int sock, const void *buf, size_t nbytes)
{
  borg_info_t *info = handles[sock];
  info->buffer.ptr = buf;
  info->buffer.size = nbytes;
  borg_connection_unpause_stream(&info->borg_conn, info->streamid);
  borg_context_run(&info->borg_ctx);
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
  *host = p;
  for (; *p != '/' && *p != ':' && *p != '\0'; ++p)
  {
    if ((*p < 'A' || *p > 'Z') && (*p < 'a' || *p > 'z') && *p != '+' && *p != '.' && *p != '-')
      return -1;
  }
  *host_len = (p - *host);
  if (*p == ':')
  {
    ++p;
    *port = p;
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
  if (port == NULL)
  {
    port = BORGES_DEFAULT_PORT;
    meta_len = sizeof(BORGES_DEFAULT_PORT);
  }

  borg_info_t *info = (borg_info_t *)Calloc(1, sizeof(borg_info_t));
  memcpy(info->meta, meta, meta_len);
  info->meta_len = meta_len;

  borg_err err;
  if (borg_context_init(&info->borg_ctx, &err) < 0)
  {
    log_err(&err, "Failed to init borges context");
    Free(info);
    return -1;
  }
  char borges_path[256];
  snprintf(borges_path, sizeof(borges_path), "tcp://%.*s:%.*s", host_len, host, port_len, port);
  if (borg_connection_init(&info->borg_conn, &info->borg_ctx, borges_path, borges_ev_handler, info, &err) < 0)
  {
    Warning("Path is %s", path);
    log_err(&err, "Failed to init borges connection");
    borg_context_cleanup(&info->borg_ctx);
    Free(info);
    return -1;
  }
  borg_context_run(&info->borg_ctx);
  if (info->shutdown)
  {
    borg_connection_cleanup(&info->borg_conn);
    borg_context_cleanup(&info->borg_ctx);
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

  borg_connection_cleanup(&info->borg_conn);
  borg_context_cleanup(&info->borg_ctx);
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
  int sock = borg_connect_int(path, filemode, streamptr);
  return sock;
}

void borges_disconnect(int sock)
{
  borg_info_t *info = handles[sock];
  info->shutdown = true;
  borg_context_run(&info->borg_ctx);
  borg_connection_cleanup(&info->borg_conn);
  borg_context_cleanup(&info->borg_ctx);
  Free(info);
}

static int borges_ev_handler(borg_connection *conn, borg_event_type event_type, void *event_data, void *userp)
{
  borg_info_t *info = userp;
  switch (event_type)
  {
  case BORG_EVENT_TYPE_CONNECT:
  {
    borg_err err;
    if (borg_connection_open_stream(conn, BORG_STREAM_MODE_OUT, info->meta, info->meta_len, &err) < 0)
    {
      log_err(&err, "Failed to open stream");
      return -1;
    }
    break;
  }
  case BORG_EVENT_TYPE_OPEN_STREAM:
  {
    struct borg_event_open_stream ev;
    info->streamid = ev.id;
    break;
  }
  case BORG_EVENT_TYPE_DATA_OUT:
  {
    struct borg_event_data_out *out = event_data;
    if (info->shutdown == true)
    {
      borg_err err;
      if (borg_connection_close_stream(conn, info->streamid, &err) < 0)
        log_err(&err, "Failed to close stream");
      return 0;
    }

    if (info->buffer.size == 0)
      return BORG_USER_STATUS_PAUSE;
    size_t chunk_len = out->len < info->buffer.size ? out->len : info->buffer.size;
    memcpy(out->data, info->buffer.ptr, chunk_len);
    info->buffer.ptr += chunk_len;
    info->buffer.size -= chunk_len;
    return chunk_len;
  }
  case BORG_EVENT_TYPE_ERROR:
  {
    struct borg_event_error *err_event = event_data;
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
