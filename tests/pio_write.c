#if defined (HAVE_CONFIG_H)
#  include "config.h"
#endif

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifdef USE_MPI
#include <mpi.h>
#include "pio_c_temp.h"
#else
typedef int MPI_Comm;
#endif

#include "cdi.h"
#include "pio_util.h"
#include "cksum.h"

struct model_config
{
  int nlon, nlat, nts;
  int filetype, datatype;
  const char *suffix;
};

struct model_config default_setup
  = { .nlon = 12, .nts = 3, .nlat = 6,
      .filetype = FILETYPE_GRB, .datatype = DATATYPE_PACK24,
      .suffix = "grb"
};

static void
var_scale(int datatype, double *mscale, double *mrscale);

static inline double
sign_flat(double v)
{
  if (v == 0.0)
    return 0.0;
  return v;
}

enum {
  ntfiles     = 2,
  nVars       = 5,
  maxlev      = 5
};

static time_t
cditime2time_t(int date, int timeofday);
static void
time_t2cditime(time_t t, int *date, int *timeofday);




void modelRun (struct model_config setup, MPI_Comm comm)
{

  static int nlev[nVars]    = {1,1,5,5,2};
  static int varCodes[nVars] = {129, 130, 131, 132, 133};
  static char * name        = "example";

  int gridID, zaxisID[nVars], taxisID;
  int vlistID, varIDs[nVars], streamID, tsID, tfID = 0;
  int i, j, k, varID, nmiss = 0;
  double *lons, *lats;
  double levs[maxlev] = {101300, 92500, 85000, 50000, 20000};
  double *var, *varslice;
  double mscale, mrscale;
  time_t current_time;
  int vdate = 19850101, vtime = 120000;
  int start = CDI_UNDEFID, chunk = CDI_UNDEFID, rank = 0, comm_size = 1;
  char filename[1024];
  int nlon = setup.nlon, nlat = setup.nlat;
  uint32_t checksum_state[nVars];
  size_t varSize[nVars];
  int *chunks = NULL, *displs = NULL;

#if USE_MPI
  xmpi ( MPI_Comm_rank ( comm, &rank ));
  xmpi ( MPI_Comm_size ( comm, &comm_size ));
#endif
  if (rank == 0)
    {
      chunks = xmalloc(comm_size * sizeof (chunks[0]));
      displs = xmalloc(comm_size * sizeof (displs[0]));
    }

  var_scale(setup.datatype, &mscale, &mrscale);

  gridID = gridCreate ( GRID_LONLAT, nlon*nlat );
  gridDefXsize ( gridID, nlon );
  gridDefYsize ( gridID, nlat );
  lons = malloc(nlon * sizeof (lons[0]));
  for (i = 0; i < nlon; ++i)
    lons[i] = ((double)(i * 360))/nlon;
  lats = malloc(nlat * sizeof (lats[0]));
  for (i = 0; i < nlat; ++i)
    lats[i] = ((double)(i * 180))/nlat - 90.0;
  gridDefXvals ( gridID, lons );
  gridDefYvals ( gridID, lats );

  for ( i = 0; i < nVars; i++ )
    {
      zaxisID[i] = zaxisCreate ( ZAXIS_PRESSURE, nlev[i] );
      zaxisDefLevels ( zaxisID[i], levs );
    }

  var = malloc(nlon * nlat * maxlev * sizeof(var[0]));
  varslice = NULL;

  vlistID = vlistCreate ();

  for ( i = 0; i < nVars; i++ )
    {
      varIDs[i] = vlistDefVar ( vlistID, gridID, zaxisID[i], TIME_VARIABLE );
      varSize[i] = nlon * nlat * nlev[i];
      vlistDefVarCode(vlistID, varIDs[i], varCodes[i]);
      vlistDefVarDatatype(vlistID, varIDs[i], setup.datatype);
    }

  taxisID = taxisCreate ( TAXIS_ABSOLUTE );
  vlistDefTaxis ( vlistID, taxisID );

  sprintf ( &filename[0], "%s_%d.%s", name, tfID, setup.suffix );
  streamID = streamOpenWrite ( filename, setup.filetype );
  xassert ( streamID >= 0 );
  streamDefVlist ( streamID, vlistID);

  pioEndDef ();

  for ( tfID = 0; tfID < ntfiles; tfID++ )
    {
      memset(checksum_state, 0, sizeof(checksum_state));
      if ( tfID > 0 )
	{
	  streamClose ( streamID );
	  sprintf ( &filename[0], "%s_%d.%s", name, tfID, setup.suffix );
	  streamID = streamOpenWrite ( filename, setup.filetype );
	  xassert ( streamID >= 0 );
	  streamDefVlist ( streamID, vlistID );
	}
      vdate = 19850101;
      vtime = 120000;
      current_time = cditime2time_t(vdate, vtime);
      for ( tsID = 0; tsID < setup.nts; tsID++ )
	{
          time_t2cditime(current_time, &vdate, &vtime);
	  taxisDefVdate ( taxisID, vdate );
	  taxisDefVtime ( taxisID, vtime );
	  streamDefTimestep ( streamID, tsID );
	  for ( varID = 0; varID < nVars; varID++ )
	    {
              for (k = 0; k < nlev[varID]; ++k)
                for (j = 0; j < nlat; ++j)
                  for (i = 0; i < nlon; ++i)
                    var[i+j*nlon+k*nlon*nlat]
                      = sign_flat(round(
                      (cos(2.0 * M_PI * (lons[(i + tsID)%nlon] - lons[0])
                          / (lons[nlon-1] - lons[0]))
                      * sin(2.0 * M_PI * (lats[j] - lats[0])
                            / (lats[nlat-1] - lats[0]))
                      ) * mscale)) * mrscale;

              start = pioInqVarDecoOff(vlistID, varIDs[varID]);
              chunk = pioInqVarDecoChunk(vlistID, varIDs[varID]);
              varslice = realloc(varslice, chunk * sizeof (var[0]));
              memcpy(varslice, var + start, chunk * sizeof (var[0]));

#if USE_MPI
              xmpi(MPI_Gather(&chunk, 1, MPI_INT, chunks, 1, MPI_INT, 0, comm));
              if (rank == 0)
                {
                  displs[0] = 0;
                  for (i = 1; i < comm_size; ++i)
                    displs[i] = displs[i - 1] + chunks[i - 1];
                }
              xmpi(MPI_Gatherv(varslice, chunk, MPI_DOUBLE, var, chunks, displs,
                               MPI_DOUBLE, 0, comm));
#else
              memcpy(var, varslice, varSize[varID] * sizeof (var[0]));
#endif
              if (rank == 0)
                {
                  memcrc_r(&checksum_state[varID], (const unsigned char *)var,
                           varSize[varID] * sizeof (var[0]));
                }

	      streamWriteVar ( streamID, varIDs[varID], varslice, nmiss );
	      start = CDI_UNDEFID;
	      chunk = CDI_UNDEFID;
	    }
          current_time += 86400;
	  pioWriteTimestep ( tsID, vdate, vtime );
	}
      if (rank == 0)
        {
          FILE *tablefp;
          {
            sprintf(filename, "%s_%d.cksum", name, tfID);
            if (!(tablefp = fopen(filename, "w")))
              {
                perror("failed to open table file");
                exit(EXIT_FAILURE);
              }
            for (i = 0; i < nVars; ++i)
              {
                uint32_t cksum;
                int code;
                cksum = memcrc_finish(&checksum_state[i],
                                      (off_t)varSize[i]
                                      * sizeof (var[0]) * setup.nts);
                code = vlistInqVarCode(vlistID, varIDs[i]);
                if (fprintf(tablefp, "%08lx %d\n", (unsigned long)cksum,
                            code) < 0)
                  {
                    perror("failed to write table file");
                    exit(EXIT_FAILURE);
                  }
              }
            fclose(tablefp);
          }
        }
    }
  pioEndTimestepping ();
  streamClose ( streamID );
  vlistDestroy ( vlistID );
  taxisDestroy ( taxisID );
  for ( i = 0; i < nVars; i++ )
    zaxisDestroy ( zaxisID[i] );
  gridDestroy ( gridID );
  free(lats);
  free(lons);
}

struct {
  char *text;
  int mode;
} mode_map[] = {
  { "PIO_MPI", PIO_MPI },
  { "PIO_FPGUARD", PIO_FPGUARD },
  { "PIO_ASYNCH", PIO_ASYNCH },
  { "PIO_WRITER", PIO_WRITER }
};

static const struct {
  char suffix[4];
  int type, defaultDT, defaultGrid;
} suffix2type[] = {
  { "nc", FILETYPE_NC, DATATYPE_FLT64, GRID_LONLAT },
  { "grb",  FILETYPE_GRB, DATATYPE_PACK24, GRID_LONLAT },
  { "nc2", FILETYPE_NC2, DATATYPE_FLT64, GRID_LONLAT },
  { "nc4", FILETYPE_NC4, DATATYPE_FLT64, GRID_LONLAT },
  { "ext", FILETYPE_EXT, DATATYPE_FLT64, GRID_GENERIC, },
  { "svc", FILETYPE_SRV, DATATYPE_FLT64, GRID_GENERIC, },
  { "ieg", FILETYPE_IEG, DATATYPE_FLT64, GRID_LONLAT },
};

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
  {
    fprintf(stderr, "range error: %ld\n", temp);
    exit(EXIT_FAILURE);
  }
  return (int)temp;
}

static void
var_scale(int datatype, double *mscale, double *mrscale)
{
  int mant_bits;
  switch (datatype)
    {
    case DATATYPE_PACK8:
      mant_bits = 7;
      break;
    case DATATYPE_PACK16:
      mant_bits = 15;
      break;
    case DATATYPE_PACK24:
      mant_bits = 23;
      break;
    case DATATYPE_FLT32:
      mant_bits = 24;
      break;
    case DATATYPE_FLT64:
      mant_bits = 53;
      break;
    case DATATYPE_INT8:
    case DATATYPE_INT16:
    case DATATYPE_INT32:
    default:
      fprintf(stderr, "Unexpected or unusable content format: %d\n",
              datatype);
      exit(EXIT_FAILURE);
    }
  *mscale = INT64_C(1) << mant_bits;
  *mrscale = 1.0 / *mscale;
}

static inline int
search_iomode_str(const char *modestr)
{
  int i, retval = -1;
  for (i = 0;
       i < sizeof (mode_map) / sizeof (mode_map[0]);
       ++i)
    if (!strcmp(modestr, mode_map[i].text))
      {
        retval = i;
        break;
      }
  return retval;
}

int main (int argc, char *argv[])
{
#ifdef USE_MPI
  enum {
    nNamespaces = 2 };
  static int hasLocalFile[nNamespaces] = { 0, 1 };
#endif

  struct model_config setup = default_setup;

  MPI_Comm commModel;
#ifdef USE_MPI
  MPI_Comm commGlob;
  int sizeGlob;
  int rankGlob;
  int IOMode = PIO_MPI;
  int nProcsIO = 2;

  xmpi ( MPI_Init ( &argc, &argv));
  xmpi ( MPI_Comm_dup ( MPI_COMM_WORLD, &commGlob ));
  xmpi ( MPI_Comm_set_errhandler ( commGlob, MPI_ERRORS_RETURN ));
  xmpi ( MPI_Comm_size ( commGlob, &sizeGlob ));
  xmpi ( MPI_Comm_rank ( commGlob, &rankGlob ));
#endif

  {
    int opt;
    while ((opt = getopt(argc, argv, "f:m:n:t:"
#ifdef USE_MPI
                         "p:w:"
#endif
                         )) != -1)
      switch (opt) {
#ifdef USE_MPI
      case 'p':
        {
          int entry = search_iomode_str(optarg);
          if (entry < 0)
            {
              fprintf(stderr, "Unsupported PIO mode requested: %s\n", optarg);
              exit(EXIT_FAILURE);
            }
          IOMode = mode_map[entry].mode;
        }
        break;
      case 'w':
        nProcsIO = strtol(optarg, NULL, 0);
        break;
#endif
      case 'f':
        {
          int i, found = 0;
          for (i = 0;
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
      case 't':
        setup.nts = parse_intarg("error parsing number of timesteps");
        break;
      default: /* '?' */
        fprintf(stderr, "Usage: %s "
                "[-m nlon] [-n nlat] [-o nlev] [-t nts]"
#ifdef USE_MPI
                " [-p PIO_MODE] [-w NIOSERVERS]"
#endif
                "\n", argv[0]);
        exit(EXIT_FAILURE);
      }

  }

#ifdef USE_MPI
  commModel = pioInit_c(commGlob, nProcsIO, IOMode,
                        nNamespaces, hasLocalFile);
#else
  commModel = -1;
#endif

  modelRun (setup, commModel);

#ifdef USE_MPI
  pioFinalize ();
  MPI_Finalize ();
#endif
  return 0;
}

static time_t
cditime2time_t(int date, int timeofday)
{
  struct tm t_s;
  time_t t;
  t_s.tm_year = date / 10000;
  t_s.tm_mon = (date - t_s.tm_year * 10000)/100;
  t_s.tm_mday = date % 100;
  t_s.tm_year -= 1900;
  t_s.tm_hour = timeofday/10000;
  t_s.tm_min = (timeofday%10000)/100;
  t_s.tm_sec = timeofday%100;
  t_s.tm_isdst = 0;
  t = mktime(&t_s);
  /* 
   * fprintf(stderr, "converted %d,%d to %s to %lld.\n", date, timeofday,
   *         asctime(&t_s), (long long)t);
   */
  return t;
}

static void
time_t2cditime(time_t t, int *date, int *timeofday)
{
  struct tm *t_s;
  t_s = localtime(&t);
  /* fprintf(stderr, "converted %lld to %s.\n", (long long)t, asctime(t_s)); */
  *date = (t_s->tm_year + 1900) * 10000 + t_s->tm_mon * 100 + t_s->tm_mday;
  *timeofday = t_s->tm_hour * 10000 + t_s->tm_min * 100 + t_s->tm_sec;
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
