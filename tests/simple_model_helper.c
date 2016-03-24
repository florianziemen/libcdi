#if defined (HAVE_CONFIG_H)
#  include "config.h"
#endif

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cdi.h"
#include "dmemory.h"

#include "simple_model_helper.h"

void
var_scale(int datatype, double *mscale, double *mrscale)
{
  int mant_bits;
  switch (datatype)
    {
    case CDI_DATATYPE_PACK8:
      mant_bits = 7;
      break;
    case CDI_DATATYPE_PACK16:
      mant_bits = 15;
      break;
    case CDI_DATATYPE_PACK24:
      mant_bits = 23;
      break;
    case CDI_DATATYPE_FLT32:
      mant_bits = 24;
      break;
    case CDI_DATATYPE_FLT64:
      mant_bits = 53;
      break;
    case CDI_DATATYPE_INT8:
    case CDI_DATATYPE_INT16:
    case CDI_DATATYPE_INT32:
    default:
      fprintf(stderr, "Unexpected or unusable content format: %d\n",
              datatype);
      exit(EXIT_FAILURE);
    }
  *mscale = (double)(INT64_C(1) << mant_bits);
  *mrscale = 1.0 / *mscale;
}

/**
 * Compute UNIX epoch-based time_t from CDI's decimal encoding of date.
 */
time_t
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
  return t;
}

/**
 * Build decimal encoding of date from UNIX epoch-based time_t.
 */
void
time_t2cditime(time_t t, int *date, int *timeofday)
{
  struct tm *t_s;
  t_s = localtime(&t);
  *date = (t_s->tm_year + 1900) * 10000 + t_s->tm_mon * 100 + t_s->tm_mday;
  *timeofday = t_s->tm_hour * 10000 + t_s->tm_min * 100 + t_s->tm_sec;
}

void
composeFilename(char **buf, const char *fname_prefix, int tfID, const char *suffix)
{
  size_t fnSize = strlen(fname_prefix) + 1 + sizeof(int) * 3 + 1 + strlen(suffix) + 1;
  char *filename = Realloc(*buf, fnSize);
  int plen;
  if ((size_t) (plen = snprintf(filename, fnSize, "%s_%d.%s", fname_prefix, tfID, suffix)) >= fnSize)
    {
      fprintf(stderr,
              "unexpected error: printing to string of size %zu"
              " results in %d chars to write\n",
              fnSize, plen);
      abort();
    }
  filename[fnSize - 1] = 0;
  *buf = filename;
}

int
composeStream(char **buf, const char *fname_prefix, int tfID, const char *suffix, int filetype)
{
  composeFilename(buf, fname_prefix, tfID, suffix);
  int streamID = streamOpenWrite(*buf, filetype);
  if (streamID < 0)
    {
      fprintf(stderr, "Failed to open stream: %s\n", cdiStringError(streamID));
      abort();
    }
  return streamID;
}

int
createGlobalLatLonGrid(int nlon, int nlat)
{
  int gridID = gridCreate(GRID_LONLAT, nlon * nlat);
  gridDefXsize(gridID, nlon);
  gridDefYsize(gridID, nlat);
  size_t maxAxisSize = (size_t) (nlon > nlat ? nlon : nlat);
  double *restrict coords = (double *) Malloc(maxAxisSize * sizeof(coords[0]));
  {
    for (size_t i = 0; i < (size_t) nlon; ++i) coords[i] = ((double) (i * 360)) / nlon;
    gridDefXvals(gridID, coords);
  }
  {
    for (size_t i = 0; i < (size_t) nlat; ++i) coords[i] = ((double) (i * 180)) / nlat - 90.0;
    gridDefYvals(gridID, coords);
  }
  Free(coords);
  return gridID;
}

#if defined USE_MPI && ! defined HAVE_PPM_CORE

static int32_t
uniform_partition_start(struct PPM_extent set_interval, int nparts,
                        int part_idx);

struct PPM_extent
PPM_uniform_partition(struct PPM_extent set_interval, int nparts,
                      int part_idx)
{
  struct PPM_extent range;
  range.first = uniform_partition_start(set_interval, nparts, part_idx);
  range.size = uniform_partition_start(set_interval, nparts, part_idx + 1)
    - range.first;
  return range;
}

static int32_t
uniform_partition_start(struct PPM_extent set_interval, int nparts,
                        int part_idx)
{
  int32_t part_offset
    = ((int64_t)set_interval.size * (int64_t)part_idx) / (int64_t)nparts;
  int32_t start = set_interval.first + part_offset;
  return start;
}

int
PPM_prime_factorization_32(uint32_t n, uint32_t **factors)
{
  if (n <= 1) return 0;
  uint32_t * restrict pfactors = realloc(*factors, 32 * sizeof (pfactors[0]));
  size_t numFactors = 0;
  uint32_t unfactored = n;
  while (!(unfactored & 1))
  {
    pfactors[numFactors] = 2;
    ++numFactors;
    unfactored >>= 1;
  }
  uint32_t divisor = 3;
  while (unfactored > 1)
  {
    while (unfactored % divisor == 0)
    {
      unfactored /= divisor;
      pfactors[numFactors] = divisor;
      ++numFactors;
    }
    divisor += 1;
  }
  *factors = pfactors;
  return numFactors;
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
