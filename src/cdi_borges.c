#include "cdi_borges.h"

#include <assert.h>
#include <msgpack/msgpack.h>
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

static int borges_ev_handler(borg_connection *conn, borg_event_type event_type, void *event_data, void *userp);

typedef struct borg_info_t
{
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

int borges_connect_int(const char *path, char filemode, stream_t *streamptr)
{
  borg_info_t *info = (borg_info_t *)Calloc(1, sizeof(borg_info_t));

  // parse BORGES address format: borges://[<host>][:<port>]/[<meta>]
  const char *host = NULL;
  size_t host_len = 0;
  const char *port = NULL;
  size_t port_len = 0;

  const char *p = path;
  host = p;
  for (; *p != '/' && *p != ':' && *p != '\0'; ++p)
  {
    if ((*p < 'A' || *p > 'Z') && (*p < 'a' || *p > 'z') && (*p < '0' || *p > '9') && *p != '+' && *p != '.' && *p != '-' && *p != '_')
    {
      Error("Borges Error: Invalid host");
      goto error1;
    }
  }
  host_len = p - host;
  if (*p == ':')
  {
    port = ++p;
    for (; *p != '/' && *p != '\0'; ++p)
    {
      if (*p < '0' || *p > '9')
      {
        Error("Borges Error: Invalid port");
        goto error1;
      }
    }
    port_len = p - port;
  }
  borg_msgpack_writer writer;
  borg_msgpack_init_writer(info->meta, sizeof(info->meta), &writer);
  borg_msgpack_write_start_map(&writer);
  if (*p == '/')
  {
    const char *key = ++p;
    const char *value = NULL;
    for (; *p != '/' && *p != '\0'; ++p)
    {
      switch (*p)
      {
      case '=':
        if (key == NULL)
        {
          Error("Borges Error: '=' in value in metadata dict");
          goto error1;
        }
        if (key == p)
        {
          Error("Borges Error: Empty key given");
          goto error1;
        }
        if (borg_msgpack_write_stringn(key, p - key, &writer) != BORG_MSGPACK_SUCCESS)
        {
          Error("Borges Error: Metadata dict too long");
          goto error1;
        }
        value = p + 1;
        key = NULL;
        break;

      case ',':
        if (value == NULL)
        {
          Error("Borges Error: No value given for key in metadata dict");
          goto error1;
        }
        if (borg_msgpack_write_stringn(value, p - value, &writer) != BORG_MSGPACK_SUCCESS)
        {
          Error("Borges Error: Metadata dict too long");
          goto error1;
        }
        key = p + 1;
        value = NULL;
        break;

      default:
        if ((*p < 'A' || *p > 'Z') && (*p < 'a' || *p > 'z') && (*p < '0' || *p > '9') && *p != '+' && *p != '.' && *p != '-' && *p != '_')
        {
          Error("Borges Error: Invalid character in metadata dict");
          goto error1;
        }
        break;
      }
    }
    if (value == NULL)
    {
      Error("Borges Error: No value given for key in metadata dict");
      goto error1;
    }
    if (borg_msgpack_write_stringn(value, p - value, &writer) != BORG_MSGPACK_SUCCESS)
    {
      Error("Borges Error: Metadata dict too long");
      goto error1;
    }
  }
  if (borg_msgpack_write_end_map(&writer) != BORG_MSGPACK_SUCCESS)
  {
    Error("Borges Error: Metadata dict too long");
    goto error1;
  }
  if (borg_msgpack_release_write_buffer(&writer, NULL, &info->meta_len) != BORG_MSGPACK_SUCCESS)
  {
    Error("Borges Error: Metadata dict too long");
    goto error1;
  }

  borg_err err;
  if (borg_context_init(&info->borg_ctx, &err) < 0)
  {
    log_err(&err, "Failed to initialize context");
    goto error1;
  }

  if (port_len == 0)
  {
    static const char* BORGES_DEFAULT_PORT = "7979";
    port = BORGES_DEFAULT_PORT;
    port_len = strlen(BORGES_DEFAULT_PORT);
  }
  if (host_len == 0)
  {
    if (borg_maybe_spawn_and_connect(&info->borg_conn, &info->borg_ctx, NULL, borges_ev_handler, info, &err) < 0)
    {
      log_err(&err, "Failed to connect");
      goto error2;
    }
  }
  else
  {
    char borges_path[256];
    snprintf(borges_path, sizeof(borges_path), "tcp://%.*s:%.*s", host_len, host, port_len, port);
    if (borg_connection_init(&info->borg_conn, &info->borg_ctx, borges_path, borges_ev_handler, info, &err) < 0)
    {
      char err_msg[256];
      borg_err_snprint(&err, err_msg, sizeof(err_msg));
      Error("Borges Error: Failed to connect, %s (for %s)", err_msg, borges_path);
      goto error2;
    }
  }

  borg_context_run(&info->borg_ctx);
  if (info->shutdown)
  {
    goto error3;
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

error3:
  borg_connection_cleanup(&info->borg_conn);
error2:
  borg_context_cleanup(&info->borg_ctx);
error1:
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

/*
 * Local Variables:
 * c-file-style: "Java"
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * show-trailing-whitespace: t
 * require-trailing-newline: t
 * End:
 */
