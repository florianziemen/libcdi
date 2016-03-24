#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#ifdef USE_MPI
#include <mpi.h>
#include <yaxt.h>
#endif

#include "cdi.h"
#include "dmemory.h"
#include "pio_write.h"
#ifdef USE_MPI
#include "cdipio.h"
#include "pio_util.h"
#endif

static const struct model_config default_setup
#ifdef __cplusplus
    = { 12, 6, 3, 5, 5, FILETYPE_GRB, CDI_DATATYPE_PACK24, 1, 0, "grb" };
#else
  = { .nlon = 12, .nlat = 6, .nts = 3, .max_nlev = 5, .nvars = 5,
      .filetype = CDI_FILETYPE_GRB, .datatype = CDI_DATATYPE_PACK24,
      .compute_checksum = 1, .create_uuid = 0,
      .suffix = "grb",
};
#endif

static const struct {
  char suffix[4];
  int type, defaultDT, defaultGrid;
} suffix2type[] = {
  { "nc",  CDI_FILETYPE_NC,  CDI_DATATYPE_FLT64, GRID_LONLAT },
  { "grb", CDI_FILETYPE_GRB, CDI_DATATYPE_PACK24, GRID_LONLAT },
  { "nc2", CDI_FILETYPE_NC2, CDI_DATATYPE_FLT64, GRID_LONLAT },
  { "nc4", CDI_FILETYPE_NC4, CDI_DATATYPE_FLT64, GRID_LONLAT },
  { "ext", CDI_FILETYPE_EXT, CDI_DATATYPE_FLT64, GRID_GENERIC, },
  { "srv", CDI_FILETYPE_SRV, CDI_DATATYPE_FLT64, GRID_GENERIC, },
  { "ieg", CDI_FILETYPE_IEG, CDI_DATATYPE_FLT64, GRID_LONLAT },
};

static void
invalidOptionDie(const char *format, ...)
{
  va_list ap;
  va_start(ap, format);
  vfprintf(stderr, format, ap);
  exit(EXIT_FAILURE);
}

static int
parse_intarg(const char msg[])
{
  char *end;
  long temp = strtol(optarg, &end, 0);
  if ((errno == ERANGE && (temp == LONG_MAX || temp == LONG_MIN))
      || (errno != 0 && temp == 0)) {
    perror(msg);
    exit(EXIT_FAILURE);
  }
  if (temp > INT_MAX || temp < INT_MIN)
    invalidOptionDie("range error: %ld\n", temp);
  return (int)temp;
}

static unsigned
parse_unsignedarg(const char msg[])
{
  char *end;
  unsigned long temp = strtoul(optarg, &end, 0);
  if ((errno == ERANGE && (temp == ULONG_MAX))
      || (errno != 0 && temp == 0))
    {
      perror(msg);
      exit(EXIT_FAILURE);
    }
  if (temp > UINT_MAX)
    invalidOptionDie("range error: %ld\n", temp);
  return (unsigned)temp;
}

typedef int (*pioRoleFunc)(MPI_Comm commSuper, int IOMode, int nProcsIO);

struct boolOptionParse
{
  bool matched, value, invert;
};

static struct boolOptionParse
parseBooleanLongOption(size_t optionStrSize, const char *optionStr, const char *argStr)
{
  bool invert;
  struct boolOptionParse result;
  if ((invert = !strncmp(argStr, optionStr, optionStrSize - 1)) || !strncmp(argStr, optionStr + 3, optionStrSize - 4))
    {
      result.matched = true;
      result.invert = invert;
      size_t valOfs = optionStrSize - (invert ? 1 : 4);
      if (argStr[valOfs] == '\0' || !strcmp(argStr + valOfs, "=true"))
        result.value = !invert;
      else if (!strcmp(argStr + valOfs, "=false"))
        result.value = invert;
      else
        invalidOptionDie("invalid option argument to -q%s: %s\n", optionStr + (invert ? 0 : 3), argStr + valOfs);
    }
  else
    result = (struct boolOptionParse){ .matched = false, .value = false };
  return result;
}

static void
parse_long_option(struct model_config *restrict setup,
                  int pioConfHandle, pioRoleFunc *pioRoleAssign,
                  const char *str)
{
#ifndef USE_MPI
  (void)pioConfHandle;
  (void)pioRoleAssign;
#endif
  static const char cacheRedistStr[] = "no-cache-redists", pioRoleSchemeOptionStr[] = "pio-role-scheme",
                    uuidCreateOptionStr[] = "no-create-uuid";
  struct boolOptionParse bop;
  if ((bop = parseBooleanLongOption(sizeof(cacheRedistStr), cacheRedistStr, str)).matched)
    {
#ifdef USE_MPI
      cdiPioConfSetRedistCache(pioConfHandle, bop.value);
#else
      invalidOptionDie("CDI-PIO option -q%s unavailable in non-MPI mode\n", cacheRedistStr + (bop.invert ? 0 : 3));
#endif
    }
  else if (!strncmp(str, pioRoleSchemeOptionStr,
                    sizeof (pioRoleSchemeOptionStr) - 1))
    {
#ifdef USE_MPI
      static const char pioRoleSchemeLastN[]="last",
        pioRoleSchemeFirstN[]="first", pioRoleSchemeBalanced[]="balanced";
      if (str[sizeof (pioRoleSchemeOptionStr) - 1] == '=')
        {
          const char *optargArg = str + sizeof(pioRoleSchemeOptionStr);
          if (!strcmp(optargArg, pioRoleSchemeLastN))
            *pioRoleAssign = cdiPioCSRLastN;
          else if (!strcmp(optargArg, pioRoleSchemeFirstN))
            *pioRoleAssign = cdiPioCSRFirstN;
          else if (!strcmp(optargArg, pioRoleSchemeBalanced))
            *pioRoleAssign = cdiPioCSRBalanced;
          else
            invalidOptionDie("unknown scheme argument \"%s\" to -q%s", optargArg, pioRoleSchemeOptionStr);
        }
      else
        invalidOptionDie("long option %s needs argument\n",
                         pioRoleSchemeOptionStr);
#else
      invalidOptionDie("CDI-PIO option -q%s ignored in non-MPI mode\n",
                       pioRoleSchemeOptionStr);
#endif
    }
  else if ((bop = parseBooleanLongOption(sizeof(uuidCreateOptionStr), uuidCreateOptionStr, str)).matched)
    {
      setup->create_uuid = bop.value;
    }
  else
    invalidOptionDie("unknown long option: %s\n", str);
}


int main(int argc, char *argv[])
{
  struct model_config setup = default_setup;

  MPI_Comm commModel;
  int pioConfHandle = 0;
  pioRoleFunc pioRoleAssign = 0;
#ifdef USE_MPI
  MPI_Comm commGlob;
  int sizeGlob;
  int rankGlob;
  int IOMode = PIO_MPI;
  int nProcsIO = 2;

  xmpi ( MPI_Init ( &argc, &argv));
  commGlob = MPI_COMM_WORLD;
  xt_initialize(commGlob);
  xmpi ( MPI_Comm_set_errhandler ( commGlob, MPI_ERRORS_RETURN ));
  xmpi ( MPI_Comm_size ( commGlob, &sizeGlob ));
  xmpi ( MPI_Comm_rank ( commGlob, &rankGlob ));

  pioConfHandle = cdiPioConfCreate();
  pioRoleAssign = cdiPioCSRLastN;
#endif

  /* seed random generator */
  unsigned randSeed;
  {
#ifdef USE_MPI
    if (rankGlob == 0)
#endif
      {
        struct timeval tv;
        int status = gettimeofday(&tv, NULL);
        if (status != 0)
          {
            perror("failed seed generation!");
            exit(1);
          }
        randSeed = (unsigned)(tv.tv_sec ^ tv.tv_usec);
      }
  }

  {
    int opt;
    while ((opt = getopt(argc, argv, "f:m:n:z:t:y:cs:q:"
#ifdef USE_MPI
                         "p:w:"
#endif
                         )) != -1)
      switch (opt) {
#ifdef USE_MPI
      case 'p':
        IOMode = cdiPioStr2IOMode(optarg);
        if (IOMode < 0)
          {
            fprintf(stderr, "Unsupported PIO mode requested: %s\n", optarg);
            exit(EXIT_FAILURE);
          }
        break;
      case 'w':
        {
          long temp = strtol(optarg, NULL, 0);
          if (temp < 0 || temp > INT_MAX/2)
            {
              fprintf(stderr, "Unsupported number of I/O servers: %ld\n", temp);
              exit(EXIT_FAILURE);
            }
          nProcsIO = (int)temp;
        }
        break;
#endif
      case 'q':
        parse_long_option(&setup, pioConfHandle, &pioRoleAssign, optarg);
        break;
      case 'f':
        {
          int found = 0;
          for (size_t i = 0;
               i < sizeof (suffix2type) / sizeof (suffix2type[0]);
               ++i)
            if (!strcmp(optarg, suffix2type[i].suffix))
              {
                found = 1;
                setup.filetype = suffix2type[i].type;
                setup.suffix = suffix2type[i].suffix;
                setup.datatype = suffix2type[i].defaultDT;
                break;
              }
          if (!found)
            {
              fprintf(stderr, "Unsupported format requested: %s\n", optarg);
              exit(EXIT_FAILURE);
            }
        }
        break;
      case 'm':
        setup.nlon = parse_intarg("error parsing number of longitudes");
        break;
      case 'n':
        setup.nlat = parse_intarg("error parsing number of latitudes");
        break;
      case 'y':
        setup.nvars = parse_intarg("error parsing number of variables");
        if (setup.nvars < 1)
          {
            fputs("number of levels must be greater than zero!\n",
                  stderr);
            exit(EXIT_FAILURE);
          }
        if (setup.nvars > 127)
          {
            fputs("number of variables must not exceed 127!\n",
                  stderr);
            exit(EXIT_FAILURE);
          }
        break;
      case 'z':
        setup.max_nlev = parse_intarg("error parsing number of levels");
        if (setup.max_nlev < 1)
          {
            fputs("number of levels must be greater than zero!\n",
                  stderr);
            exit(EXIT_FAILURE);
          }
        break;
      case 't':
        setup.nts = parse_intarg("error parsing number of timesteps");
        break;
      case 'c':
        setup.compute_checksum = 0;
        break;
      case 's':
        randSeed = parse_unsignedarg("error parsing random seed");
        break;
      default: /* '?' */
        fprintf(stderr, "Usage: %s "
                "[-m nlon] [-n nlat] [-z nlev] [-t nts] [-y num_vars]"
#ifdef USE_MPI
                " [-p PIO_MODE] [-w NIOSERVERS] [-c]"
#endif
                "\n", argv[0]);
        exit(EXIT_FAILURE);
      }

  }

#ifdef USE_MPI
  MPI_Bcast(&randSeed, 1, MPI_UNSIGNED, 0, MPI_COMM_WORLD);
#endif
  srandom(randSeed);
#ifdef USE_MPI
  if (rankGlob == 0)
#endif
    fprintf(stderr, "random seed=%u\n", randSeed);

#ifdef USE_MPI
  int pioNamespace;
  cdiPioConfSetIOMode(pioConfHandle, IOMode);
  cdiPioConfSetCSRole(pioConfHandle, pioRoleAssign(commGlob, IOMode,
                                                   nProcsIO));
  cdiPioConfSetPartInflate(pioConfHandle, 1.0f);
  int initNamespace = namespaceGetActive();
  commModel = cdiPioInit(commGlob, pioConfHandle, &pioNamespace);
  if (commModel != MPI_COMM_NULL)
    {
      namespaceSetActive(pioNamespace);
#else
      commModel = -1;
#endif

      modelRun (setup, commModel);

#ifdef USE_MPI
    }
  pioFinalize();
  namespaceSetActive(initNamespace);
  cdiPioConfDestroy(pioConfHandle);
  xt_finalize();
  MPI_Finalize ();
#endif
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
