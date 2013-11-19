#if defined (HAVE_CONFIG_H)
#  include "config.h"
#endif

#include <math.h>
#include <stdlib.h>
#include <time.h>

#ifdef USE_MPI
#include <mpi.h>
#include <yaxt.h>
#else
typedef int MPI_Comm;
#endif

#include "cdi.h"

#include "cksum.h"
#include "pio_util.h"
#include "pio_write.h"

enum {
  ntfiles     = 2,
  nVars       = 5,
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

static time_t
cditime2time_t(int date, int timeofday);
static void
time_t2cditime(time_t t, int *date, int *timeofday);

static void
modelRegionCompute(double region[], size_t offset, size_t len,
                   int nlev, int nlat, int nlon,
                   int tsID, const double lons[], const double lats[],
                   double mscale, double mrscale)
{
  size_t local_pos;
  for (local_pos = 0; local_pos < len; ++local_pos)
    {
      size_t global_pos = offset + local_pos;
      int k = global_pos / (nlon * nlat);
      int j = (global_pos % (nlon * nlat))/ nlon;
      int i = global_pos % nlon;
      region[local_pos]
        = sign_flat(round((cos(2.0 * M_PI * (lons[(i + tsID)%nlon] - lons[0])
                               / (lons[nlon-1] - lons[0]))
                           * sin(2.0 * M_PI * (lats[(j + k)%nlat] - lats[0])
                                 / (lats[nlat-1] - lats[0]))
                           ) * mscale)) * mrscale;
    }
}

#ifdef USE_MPI
static int
uniform_partition_start(int set_interval[2], int nparts, int part_idx);
#endif

void
modelRun(struct model_config setup, MPI_Comm comm)
{
  static const char * const fname_prefix        = "example";

  int gridID, *zaxisID, taxisID;
  int vlistID, *varIDs, streamID, tsID, tfID = 0;
  int i, nmiss = 0;
  double *lons, *lats;
  double *var = NULL, *varslice = NULL;
  double mscale, mrscale;
  time_t current_time;
  int vdate = 19850101, vtime = 120000;
  int rank = 0;
  char filename[1024];
  int nlon = setup.nlon, nlat = setup.nlat;
  uint32_t checksum_state[nVars];
  size_t varSize[nVars], varslice_size = 0;
  int *nlev;
  double *levs;
#if USE_MPI
  int *chunks = NULL, *displs = NULL, comm_size = 1;
  struct var1DDeco {
    int chunkSize, start;
    Xt_idxlist partDesc;
  } varDeco[nVars];
#endif

#if USE_MPI
  xmpi ( MPI_Comm_rank ( comm, &rank ));
  xmpi ( MPI_Comm_size ( comm, &comm_size ));
  if (rank == 0 && setup.compute_checksum)
    {
      chunks = xmalloc(comm_size * sizeof (chunks[0]));
      displs = xmalloc(comm_size * sizeof (displs[0]));
      var = xmalloc((size_t)nlon * (size_t)nlat
                    * (size_t)setup.max_nlev * sizeof(var[0]));
    }
#endif

  var_scale(setup.datatype, &mscale, &mrscale);

  gridID = gridCreate ( GRID_LONLAT, nlon*nlat );
  gridDefXsize ( gridID, nlon );
  gridDefYsize ( gridID, nlat );
  lons = xmalloc(nlon * sizeof (lons[0]));
  for (i = 0; i < nlon; ++i)
    lons[i] = ((double)(i * 360))/nlon;
  lats = xmalloc(nlat * sizeof (lats[0]));
  for (i = 0; i < nlat; ++i)
    lats[i] = ((double)(i * 180))/nlat - 90.0;
  gridDefXvals ( gridID, lons );
  gridDefYvals ( gridID, lats );

  levs = xmalloc(setup.max_nlev * sizeof (levs[0]));
  for (i = 0; i < setup.max_nlev; ++i)
    levs[i] = 101300.0
      - 3940.3 * (exp(1.3579 * (double)(i)/(setup.max_nlev - 1)) - 1.0);

  nlev = xmalloc(4 * nVars * sizeof (nlev[0]));
  zaxisID = nlev + 3 * nVars;
  for (int varIdx = 0; varIdx < nVars; varIdx++ )
    {
      int varLevs = random()%4;
      switch (varLevs)
        {
        case 1:
          varLevs = setup.max_nlev / 3;
          break;
        case 2:
          varLevs = setup.max_nlev >= 11 ? 11 : setup.max_nlev / 2;
          break;
        case 3:
          varLevs = setup.max_nlev - 1;
          break;
        }
      ++varLevs;
      nlev[varIdx] = varLevs;
      for (i = 0; i < varIdx; ++i)
        if (nlev[i] == varLevs)
          {
            zaxisID[varIdx] = zaxisID[i];
            goto zaxisIDset;
          }
      zaxisID[varIdx] = zaxisCreate ( ZAXIS_PRESSURE, nlev[varIdx] );
      zaxisDefLevels ( zaxisID[varIdx], levs );
      zaxisIDset:
      ;
    }

  int *varCodes = nlev + nVars;
  for (int varIdx = 0; varIdx < nVars; ++varIdx)
    varCodes[varIdx] = 129 + varIdx;

  vlistID = vlistCreate ();

  varIDs = nlev + 2 * nVars;
  for ( i = 0; i < nVars; i++ )
    {
      varIDs[i] = vlistDefVar ( vlistID, gridID, zaxisID[i], TIME_VARIABLE );
      varSize[i] = nlon * nlat * nlev[i];
#ifdef USE_MPI
      {
         int start = uniform_partition_start((int [2]){ 0, varSize[i] - 1 },
                                             comm_size, rank),
           chunkSize = uniform_partition_start((int [2]){ 0, varSize[i] - 1 },
                                               comm_size, rank + 1) - start;
         fprintf(stderr, "%d: start=%d, chunkSize = %d\n", rank,
                 start, chunkSize);
         Xt_idxlist idxlist
           = xt_idxstripes_new(&(struct Xt_stripe){ .start = start,
                   .nstrides = chunkSize, .stride = 1 }, 1);
         varDeco[i] = (struct var1DDeco){
           .start = start,
           .chunkSize = chunkSize,
           .partDesc = idxlist
         };
      }
#endif
      vlistDefVarCode(vlistID, varIDs[i], varCodes[i]);
      vlistDefVarDatatype(vlistID, varIDs[i], setup.datatype);
    }

  taxisID = taxisCreate ( TAXIS_ABSOLUTE );
  vlistDefTaxis ( vlistID, taxisID );

  sprintf ( &filename[0], "%s_%d.%s", fname_prefix, tfID, setup.suffix );
  streamID = streamOpenWrite ( filename, setup.filetype );
  xassert ( streamID >= 0 );
  streamDefVlist ( streamID, vlistID);

#ifdef USE_MPI
  pioEndDef ();
#endif

  for ( tfID = 0; tfID < ntfiles; tfID++ )
    {
      memset(checksum_state, 0, sizeof(checksum_state));
      if ( tfID > 0 )
	{
	  streamClose ( streamID );
	  sprintf ( &filename[0], "%s_%d.%s", fname_prefix, tfID, setup.suffix );
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
	  for (int varID = 0; varID < nVars; ++varID)
	    {
#ifdef USE_MPI
              int start = varDeco[varID].start;
              int chunk = varDeco[varID].chunkSize;
#else
              int chunk = varSize[varID];
              int start = 0;
#endif
              if (varslice_size < chunk)
                {
                  varslice = xrealloc(varslice, chunk * sizeof (var[0]));
                  varslice_size = chunk;
                }
              modelRegionCompute(varslice, start, chunk,
                                 nlev[varID], nlat, nlon,
                                 tsID, lons, lats,
                                 mscale, mrscale);
              if (setup.compute_checksum)
                {
#if USE_MPI
                  xmpi(MPI_Gather(&chunk, 1, MPI_INT,
                                  chunks, 1, MPI_INT, 0, comm));
                  if (rank == 0)
                    {
                      displs[0] = 0;
                      for (i = 1; i < comm_size; ++i)
                        displs[i] = displs[i - 1] + chunks[i - 1];
                    }
                  xmpi(MPI_Gatherv(varslice, chunk, MPI_DOUBLE,
                                   var, chunks, displs, MPI_DOUBLE, 0, comm));
#else
                  var = varslice;
#endif
                }
              if (rank == 0 && setup.compute_checksum)
                {
                  memcrc_r(&checksum_state[varID], (const unsigned char *)var,
                           varSize[varID] * sizeof (var[0]));
                }

#ifdef USE_MPI
	      streamWriteVarPart(streamID, varIDs[varID], varslice, nmiss,
                                 varDeco[varID].partDesc);
#else
	      streamWriteVar(streamID, varIDs[varID], varslice, nmiss);
#endif
	      start = CDI_UNDEFID;
	      chunk = CDI_UNDEFID;
	    }
          current_time += 86400;
#ifdef USE_MPI
	  pioWriteTimestep ( tsID, vdate, vtime );
#endif
	}
      if (rank == 0 && setup.compute_checksum)
        {
          FILE *tablefp;
          {
            sprintf(filename, "%s_%d.cksum", fname_prefix, tfID);
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
  free(varslice);
#ifdef USE_MPI
  pioEndTimestepping ();
#endif
  streamClose ( streamID );
  vlistDestroy ( vlistID );
  taxisDestroy ( taxisID );
  for ( i = 0; i < nVars; i++ )
    {
      int zID = zaxisID[i];
      if (zID != CDI_UNDEFID)
        {
          zaxisDestroy(zID);
          for (int j = i + 1; j < nVars; ++j)
            if (zID == zaxisID[j])
              zaxisID[j] = CDI_UNDEFID;
        }
    }
  gridDestroy ( gridID );
#if USE_MPI
  for (int varID = 0; varID < nVars; ++varID)
    xt_idxlist_delete(varDeco[varID].partDesc);
  free(displs);
  free(chunks);
  free(var);
#endif
  free(nlev);
  free(levs);
  free(lats);
  free(lons);
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

static void
time_t2cditime(time_t t, int *date, int *timeofday)
{
  struct tm *t_s;
  t_s = localtime(&t);
  /* fprintf(stderr, "converted %lld to %s.\n", (long long)t, asctime(t_s)); */
  *date = (t_s->tm_year + 1900) * 10000 + t_s->tm_mon * 100 + t_s->tm_mday;
  *timeofday = t_s->tm_hour * 10000 + t_s->tm_min * 100 + t_s->tm_sec;
}

#ifdef USE_MPI
static int
uniform_partition_start(int set_interval[2], int nparts, int part_idx)
{
  int part_offset
    = (((long long)set_interval[1] - (long long)set_interval[0] + 1LL)
       * (long long)part_idx) / (long long)nparts;
  int start = set_interval[0] + part_offset;
  return start;
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
