#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cdi.h"

#include "cksum.h"

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

static inline double
sign_flat(double v)
{
  if (v == 0.0)
    return 0.0;
  return v;
}

static char *
fname_create(const char *prefix, const char *suffix)
{
  size_t prefix_len, suffix_len;
  char *fname;
  if (!(fname =malloc((prefix_len = strlen(prefix)) + 1
                      + (suffix_len = strlen(suffix)) + 1)))
    {
      perror("cannot create string");
      exit(EXIT_FAILURE);
    }
  strcpy(fname, prefix);
  fname[prefix_len] = '.';
  strcpy(fname + prefix_len + 1, suffix);
  return fname;
}

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

enum {
  nvars = 2,
};

static const int varCodes[nvars] = { 42, 55 };

int
main(int argc, char *argv[])
{
  int gridID, zaxisID[nvars], taxisID;
  int vlistID, varID[nvars], streamID, tsID;
  int nlon = 12, //!< Number of longitudes
    nlat = 6, //!< Number of latitudes
    nlev = 5, //!< Number of levels
    nts = 3; //!< Number of time steps
  int i, j, k, nmiss = 0;
  double *lons, *lats, *var[nvars], *levs, mscale, mrscale;
  size_t varSize[nvars];
  char *varName[nvars] = { "varname1", "varname2" };
  const char *suffix = "grb", *prefix = "example";
  int grid = GRID_LONLAT;
  int filetype = FILETYPE_GRB, datatype = DATATYPE_PACK24;
  {
    int opt;
    while ((opt = getopt(argc, argv, "f:m:n:o:t:")) != -1)
      switch (opt) {
      case 'f':
        {
          int found = 0;
          for (i = 0;
               i < sizeof (suffix2type) / sizeof (suffix2type[0]);
               ++i)
            if (!strcmp(optarg, suffix2type[i].suffix))
              {
                found = 1;
                filetype = suffix2type[i].type;
                suffix = suffix2type[i].suffix;
                datatype = suffix2type[i].defaultDT;
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
        nlon = parse_intarg("error parsing number of longitudes");
        break;
      case 'n':
        nlat = parse_intarg("error parsing number of latitudes");
        break;
      case 'o':
        nlev = parse_intarg("error parsing number of levels");
        break;
      case 't':
        nts = parse_intarg("error parsing number of timesteps");
        break;
      default: /* '?' */
        fprintf(stderr, "Usage: %s [-m nlon] [-n nlat] [-o nlev] [-t nts]\n",
                argv[0]);
        exit(EXIT_FAILURE);
      }
  }

  lons = malloc(nlon * sizeof (lons[0]));
  for (i = 0; i < nlon; ++i)
    lons[i] = ((double)(i * 360))/nlon;
  lats = malloc(nlat * sizeof (lats[0]));
  for (i = 0; i < nlat; ++i)
    lats[i] = ((double)(i * 180))/nlat - 90.0;
  levs = malloc(nlev * sizeof (levs[0]));
  for (i = 0; i < nlev; ++i)
    levs[i] = 101300 - 3940.3 * (exp(1.3579 * (double)(i)/(nlev - 1)) - 1.0);

  varSize[0] = nlon * nlat;
  varSize[1] = nlon * nlat * nlev;

  // Create a regular lon/lat grid
  gridID = gridCreate(grid, nlon*nlat);
  gridDefXsize(gridID, nlon);
  gridDefYsize(gridID, nlat);
  gridDefXvals(gridID, lons);
  gridDefYvals(gridID, lats);

  // Create a surface level Z-axis
  zaxisID[0] = zaxisCreate(ZAXIS_SURFACE, 1);

  // Create a pressure level Z-axis
  zaxisID[1] = zaxisCreate(ZAXIS_PRESSURE, nlev);
  zaxisDefLevels(zaxisID[1], levs);

  // Create a Time axis
  taxisID = taxisCreate(TAXIS_ABSOLUTE);

  // Create a variable list
  vlistID = vlistCreate();

  for (i = 0; i < nvars; ++i)
    {
      // Define the variables
      varID[i] = vlistDefVar(vlistID, gridID, zaxisID[i], TIME_VARIABLE);
      // Define the variable names,
      vlistDefVarName(vlistID, varID[i], varName[i]);
      // the codes
      vlistDefVarCode(vlistID, varID[i], varCodes[i]);
      // and set the data type
      vlistDefVarDatatype(vlistID, varID[i], datatype);
      // create memory for variables
      var[i] = malloc(varSize[i] * sizeof (var[i][0]));

    }

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
    mscale = INT64_C(1) << mant_bits;
    mrscale = 1.0/mscale;
  }

  // Assign the Time axis to the variable list
  vlistDefTaxis(vlistID, taxisID);

  // Create a dataset in netCDF fromat
  {
    char *fname = fname_create(prefix, suffix);
    if ((streamID = streamOpenWrite(fname, filetype)) < 0)
      {
        fprintf(stderr, "error opening output file %s: %s\n",
                fname, cdiStringError(streamID));
        exit(EXIT_FAILURE);
      }
    free(fname);
  }

  // Assign the variable list to the dataset
  streamDefVlist(streamID, vlistID);

  {
    uint32_t checksum_state[nvars] = { 0, 0 };
    // Loop over the number of time steps
    for ( tsID = 0; tsID < nts; tsID++ )
      {
        // Set the verification date to 1985-01-01 + tsID
        taxisDefVdate(taxisID, 19850101+tsID);
        // Set the verification time to 12:00:00
        taxisDefVtime(taxisID, 120000);
        // Define the time step
        streamDefTimestep(streamID, tsID);

        // Init var1 and var2
        for (j = 0; j < nlat; j++)
          for (i = 0; i < nlon; i++)
            var[0][i+j*nlon]
              = sign_flat(round(
                   (sin(2.0 * M_PI * (lons[(i + tsID)%nlon] - lons[0])
                        / (lons[nlon-1] - lons[0]))
                    * cos(2.0 * M_PI * (lats[j] - lats[0])
                          / (lons[nlat-1] - lats[0]))
                    ) * mscale)) * mrscale;
        for (k = 0; k < nlev; ++k)
          for (j = 0; j < nlat; j++)
            for (i = 0; i < nlon; i++)
              var[1][i+j*nlon+k*nlon*nlat]
                = sign_flat(round(
                     (cos(2.0 * M_PI * (lons[(i + tsID)%nlon] - lons[0])
                          / (lons[nlon-1] - lons[0]))
                      * sin(2.0 * M_PI * (lats[j] - lats[0])
                            / (lons[nlat-1] - lats[0]))
                      ) * mscale)) * mrscale;

        memcrc_r(&checksum_state[0], (const unsigned char *)var[0],
                 varSize[0] * sizeof (var[0][0]));
        memcrc_r(&checksum_state[1], (const unsigned char *)var[1],
                 varSize[1] * sizeof (var[1][0]));

        // Write var1 and var2
        streamWriteVar(streamID, varID[0], var[0], nmiss);
        streamWriteVar(streamID, varID[1], var[1], nmiss);
      }
    // write checksums to table file
    {
      FILE *tablefp;
      {
        char *fname = fname_create(prefix, "cksum");
        if (!(tablefp = fopen(fname, "w")))
          {
            perror("failed to open table file");
            exit(EXIT_FAILURE);
          }
        free(fname);
      }
      for (i = 0; i < nvars; ++i)
        {
          uint32_t cksum;
          int code;
          cksum = memcrc_finish(&checksum_state[i],
                                (off_t)varSize[i] * sizeof (var[i][0]) * nts);
          code = vlistInqVarCode(vlistID, varID[i]);
          if (fprintf(tablefp, "%08lx %d\n", (unsigned long)cksum, code) < 0)
            {
              perror("failed to write table file");
              exit(EXIT_FAILURE);
            }
        }
      fclose(tablefp);
    }
  }

  // Close the output stream
  streamClose(streamID);

  // Destroy the objects
  vlistDestroy(vlistID);
  taxisDestroy(taxisID);
  zaxisDestroy(zaxisID[0]);
  zaxisDestroy(zaxisID[1]);
  gridDestroy(gridID);

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
