#include "cdi_across.h"

#ifdef HAVE_ACROSS

#include <string.h>

#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

#include "cdi_int.h"
#include "dmemory.h"
#include "error.h"

int across_connect_int(char *path, char filemode, stream_t *streamptr)
{
  // parse ACROSS address format: across://<host>[:<port>]/<expid>[@<expver>]

  const char *host = path;
  const char *port = ACROSS_DEFAULT_PORT;
  const char *expid = NULL;
  int expver = 1;

  char *c = path;
  while (*c != '\0')
    {
      switch (*c)
        {
        case ':':
          if (expid != NULL)
            {
              Warning("Experiment id must not contain ':'");
              return CDI_EINVAL;
            }
          port = c + 1;
          *c = '\0';
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

        default:
          ++c;
          break;
        }
    }

  if (expid == NULL || expid[0] == '\0')
    {
      Warning("No experiment version given");
      return CDI_EINVAL;
    }

  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = 0;
  hints.ai_protocol = 0;

  struct addrinfo *addr;
  int res = getaddrinfo(host, port, &hints, &addr);
  if (res != 0)
    {
      Warning("Could not find %s: %s", host, gai_strerror(res));
      return CDI_ESYSTEM;
    }

  int sock;
  struct addrinfo *rp;
  for (rp = addr; rp != NULL; rp = rp->ai_next)
    {
      sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
      if (sock < 0) continue;
      if (connect(sock, rp->ai_addr, rp->ai_addrlen) != -1) break;
      close(sock);
    }
  freeaddrinfo(addr);

  if (rp == NULL)
    {
      Warning("Could not connect to %s:%s", host, port);
      return CDI_ESYSTEM;
    }

  across_info_t *info = (across_info_t*)Malloc(sizeof(across_info_t));
  info->expid = strdup(expid);
  info->expver = expver;
  streamptr->protocolData = info;

  return sock;
}

int across_connect(const char *path, char filemode, stream_t *streamptr)
{
  if (filemode != 'w')
    {
      Warning("Reading from ACROSS not implemented yet"); // TODO read from ACROSS
      return CDI_EINVAL;
    }

  char *p = strdup(path); // needed to adjust part strings
  int sock = across_connect_int(p, filemode, streamptr);
  free(p);
  return sock;
}

void across_disconnect(int sock)
{
  close(sock);
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
