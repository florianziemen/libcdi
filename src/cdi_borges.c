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
#define BORGES_DEFAULT_PORT 7979

static int borges_ev_handler(borges_connection* conn, borges_event_type event_type, void* event_data, void* userp);

typedef struct borges_info_t
{
  const char* expid;
  int expver;
  bool shutdown;
  borges_context borges_ctx;
  borges_connection borges_conn;
  struct {
    const void* ptr;
    size_t size;
  } buffer;
} borges_info_t;

static borges_info_t* handles[1024] = {NULL};

static void log_err(borges_err* err, const char* msg)
{
  char err_msg[256];
  borges_err_snprint(err, err_msg, sizeof(err_msg));
  Error("Borges Error: %s, %s", msg, err_msg);
}

static uint16_t swap_endianness16(uint16_t v)
{
  return ((v>>8) & 0xff)
    | ((v<<8) & 0xff00);
}

static uint32_t swap_endianness32(uint32_t v)
{
  return ((v>>24) & 0xff)
    | ((v<<8) & 0xff0000)
    | ((v>>8) & 0xff00)
    | ((v<<24) & 0xff000000);
}

static uint64_t swap_endianness64(uint64_t v)
{
  return ((v >> 56) & 0xff)
    | ((v >> 40) & 0xff00)
    | ((v >> 24) & 0xff0000)
    | ((v >> 8) & 0xff000000)
    | ((v << 8) & 0xff00000000)
    | ((v << 24) & 0xff0000000000)
    | ((v << 40) & 0xff000000000000)
    | ((v << 56) & 0xff00000000000000);
}

static int set8(unsigned char* buf, uint8_t v)
{
  *buf = v;
  return sizeof(v);
}

static int set16(unsigned char* buf, uint16_t v)
{
  if (HOST_ENDIANNESS == CDI_LITTLEENDIAN) v = swap_endianness16(v);
  memcpy(buf, &v, sizeof(v));
  return sizeof(v);
}

static int set32(unsigned char* buf, uint32_t v)
{
  if (HOST_ENDIANNESS == CDI_LITTLEENDIAN) v = swap_endianness32(v);
  memcpy(buf, &v, sizeof(v));
  return sizeof(v);
}

static int set64(unsigned char* buf, uint64_t v)
{
  if (HOST_ENDIANNESS == CDI_LITTLEENDIAN) v = swap_endianness64(v);
  memcpy(buf, &v, sizeof(v));
  return sizeof(v);
}

static void fletcher8(const unsigned char *buf, size_t len, uint8_t *sum1out, uint8_t *sum2out)
{
  uint16_t sum1 = 0;
  uint16_t sum2 = 0;
  for (size_t i = 0; i < len; ++i)
    {
      sum1 += *buf++;
      if (sum1 >= 255) sum1 -= 255;
      sum2 += sum1;
      if (sum2 >= 255) sum2 -= 255;
    }
  *sum1out = sum1;
  *sum2out = sum2;
}

static uint8_t fletcher8_check1(uint8_t sum1, uint8_t sum2)
{
  return 255 - ((sum1 + sum2) % 255);
}

static uint8_t fletcher8_check2(uint8_t sum1, uint8_t sum2)
{
  return 255 - ((sum1 + fletcher8_check1(sum1, sum2)) % 255);
}

static int borges_write_buf(int sock, const void* buf, size_t nbytes)
{
  borges_info_t *info = handles[sock];
  info->buffer.ptr = buf;
  info->buffer.size = nbytes;
  borges_connection_unpause_stream(&info->borges_conn);
  borges_err err;
  if (borges_context_run(&info->borges_ctx, &err) < 0)
  {
    log_err(&err, "Failed to complete work queue");
    return -1;
  }
  if (info->shutdown)
    return -1;
  return 0;
}

int borges_write_grib_message(stream_t *streamptr, const void *gribbuffer, size_t nbytes)
{
  unsigned char section0[] = {'G', 'R', 'I', 'B', 255, 255, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0};
  const size_t section0_len = sizeof(section0);

  uint32_t section1_len;
  const unsigned char *section1 = gribbuffer + section0_len;
  if (nbytes < section0_len + sizeof(section1_len) + 1)
    {
      Error("GRIB2 stream is too short");
      return 1;
    }
  if (section1[sizeof(section1_len)] != 1)
    {
      Error("Section 1 not found at expected position in GRIB2 stream");
      return 1;
    }
  memcpy(&section1_len, section1, sizeof(section1_len));
  if (HOST_ENDIANNESS == CDI_LITTLEENDIAN) section1_len = swap_endianness32(section1_len);

  if (nbytes < section0_len
      + section1_len
      + 4 // next section size
      + 1 // section number ("3")
      + 4 // footer "7777"
      )
    {
      Error("GRIB2 stream is too short");
      return 1;
    }
  size_t other_sections_len = nbytes - section0_len - section1_len;
  const unsigned char *other_sections = section1 + section1_len;
  if (other_sections[4] != 3)
    {
      if (other_sections[4] == 2) Error("Section 2 must not already be present in GRIB2 stream");
      else Error("Section 3 not found at expected position in GRIB2 stream");
      return 1;
    }

  borges_info_t* info = (borges_info_t*)streamptr->protocolData;

  int expid_len = strlen(info->expid);
  if (expid_len < EXPID_MIN_LENGTH)
    {
      Error("expid is too short");
      return 1;
    }
  if (expid_len > 255)
    {
      Error("expid is too long");
      return 1;
    }
  uint32_t section2_len = 4 // section size
    + 1 // section number ("2")
    + 2 // local section number ("0")
    + 1 // borges local section version
    + 1 // expid length
    + expid_len
    + 4 // expver
    + 4 // timestamp
    + 2 // check bytes
    ;
  unsigned char* section2 = (unsigned char*) Malloc(section2_len);
  {
    unsigned char pos = 0;
    pos += set32(section2 + pos, section2_len);
    pos += set8(section2 + pos, 2); // section number
    pos += set16(section2 + pos, 0); // local section number
    pos += set8(section2 + pos, BORGES_SECTION2_VERSION);
    pos += set8(section2 + pos, expid_len);
    memcpy(section2 + pos, info->expid, expid_len);
    pos += expid_len;
    pos += set32(section2 + pos, info->expver);
    pos += set32(section2 + pos, time(NULL));
    uint8_t fletchersum1;
    uint8_t fletchersum2;
    fletcher8(section2 + 5, section2_len - 7, &fletchersum1, &fletchersum2);
    pos += set8(section2 + pos, fletcher8_check1(fletchersum1, fletchersum2));
    pos += set8(section2 + pos, fletcher8_check2(fletchersum1, fletchersum2));
    assert(pos == section2_len);
  }

  set64(section0 + section0_len - 8, nbytes + section2_len); // total message length

  int res = 0;
  if (borges_write_buf(streamptr->fileID, section0, section0_len)
      || borges_write_buf(streamptr->fileID, section1, section1_len)
      || borges_write_buf(streamptr->fileID, section2, section2_len)
      || borges_write_buf(streamptr->fileID, other_sections, other_sections_len)) res = 1;

  Free(section2);
  return res;
}

int borges_connect_int(char *path, char filemode, stream_t *streamptr)
{
  // parse BORGES address format: borges://<host>[:<port>]/<expid>[@<expver>]

  const char *host = path;
  const char *port = BORGES_DEFAULT_PORT;
  const char *expid = NULL;
  int expver = 1;

  char *c = path;
  while (*c != '\0')
    {
      switch (*c)
        {
        case ':':
          if (expid == NULL)
            {
              port = c + 1;
              *c = '\0';
            }
          ++c;
          break;

        case '/':
          if (expid != NULL)
            {
              Warning("Experiment id must not contain '/'");
              return CDI_EINVAL;
            }
          expid = c + 1;
          *c = '\0';
          ++c;
          break;

        case '@':
          if (expid == NULL)
            {
              ++c;
              break;
            }
          *c = '\0';
          expver = strtoul(c + 1, &c, 10);
          if (*c != '\0')
            {
              Warning("Experiment version must be an integer");
              return CDI_EINVAL;
            }
          break;

        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
        case 'a': case 'b': case 'c': case 'd': case 'e':
        case 'f': case 'g': case 'h': case 'i': case 'j':
        case 'k': case 'l': case 'm': case 'n': case 'o':
        case 'p': case 'q': case 'r': case 's': case 't':
        case 'u': case 'v': case 'w': case 'x': case 'y': case 'z':
        case 'A': case 'B': case 'C': case 'D': case 'E':
        case 'F': case 'G': case 'H': case 'I': case 'J':
        case 'K': case 'L': case 'M': case 'N': case 'O':
        case 'P': case 'Q': case 'R': case 'S': case 'T':
        case 'U': case 'V': case 'W': case 'X': case 'Y': case 'Z':
        case '-': case '.': case '_': case '~':
          ++c;
          break;

        default:
          if (expid != NULL)
            {
              Warning("Experiment id must not contain '%c'", c);
              return CDI_EINVAL;
            }
          ++c;
          break;
        }
    }

  if (expid == NULL || expid[0] == '\0')
    {
      Warning("No experiment id given");
      return CDI_EINVAL;
    }

  if (strlen(expid) < EXPID_MIN_LENGTH)
    {
      Warning("Experiment id must be longer than %d chars", EXPID_MIN_LENGTH - 1);
      return CDI_EINVAL;
    }
  if (strlen(expid) > 255)
    {
      Warning("Experiment id must be shorter than 256 chars");
      return CDI_EINVAL;
    }

  borges_info_t *info = (borges_info_t*)Calloc(1, sizeof(borges_info_t));
  info->expid = strdup(expid);
  info->expver = expver;
  
  borges_err err;

  if (borges_context_init(&info->borges_ctx, &err) < 0)
  {
    log_err(&err, "Failed to init borges context");
    Free(info);
    return -1;
  }
  char borges_path[256];
  snprintf(borges_path, sizeof(borges_path), "tcp://%s:%s", host, port);
  if (borges_connection_init(&info->borges_conn, &info->borges_ctx, borges_path, borges_ev_handler, info, &err) < 0)
  { 
    Warning("Path is %s", path);
    log_err(&err, "Failed to init borges connection");
    borges_context_cleanup(&info->borges_ctx);
    Free(info);
    return -1;
  }
  if (borges_context_run(&info->borges_ctx, &err) < 0)
  {
    log_err(&err, "Failed to complete work queue");
    borges_connection_cleanup(&info->borges_conn);
    borges_context_cleanup(&info->borges_ctx);
    Free(info);
    return -1;
  }
  if (info->shutdown)
  {
    borges_connection_cleanup(&info->borges_conn);
    borges_context_cleanup(&info->borges_ctx);
    Free(info);
    return -1;
  }
  streamptr->protocolData = info;
  for(int idx = 0; idx < sizeof(handles) / sizeof(handles[0]); ++idx)
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

  char *p = strdup(path); // needed to adjust part strings
  int sock = borges_connect_int(p, filemode, streamptr);
  free(p);
  return sock;
}

void borges_disconnect(int sock)
{
  borges_info_t *info = handles[sock];
  borges_connection_cleanup(&info->borges_conn);
  borges_context_cleanup(&info->borges_ctx);
  Free(info);
}

int borges_ev_handler(borges_connection* conn, borges_event_type event_type, void* event_data, void* userp)
{
    borges_info_t *info = userp;
    switch (event_type) {
        case BORGES_EVENT_TYPE_CONNECT: {
            borges_connection_open_stream(conn, BORGES_STREAM_MODE_OUT,  info->expid);
            break;
        }
        case BORGES_EVENT_TYPE_DATA: {
            struct borges_event_data_out* out = event_data;
            if (info->buffer.size == 0)
              borges_connection_pause_stream(conn);
            size_t chunk_len =  out->len < info->buffer.size ? out->len : info->buffer.size;
            memcpy(out->data, info->buffer.ptr, chunk_len);
            info->buffer.ptr += chunk_len;
            info->buffer.size -= chunk_len;
            return chunk_len;
        }
        case BORGES_EVENT_TYPE_ERROR: {
            struct borges_event_error* err_event = event_data;
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
