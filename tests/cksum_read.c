#include <assert.h>
#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>

#include "cdi.h"
#include "cksum.h"

int main(int argc, char *argv[])
{
  int taxisID, vlistID, streamID, tsID;
  int vdate, vtime, nvars, ngrids, nzaxis;
  int i;
  uint32_t *checksum_state;
  struct {
    int x, y, z;
    size_t chars;
  } *varSize;

  double **var;
  char *fname = "example.grb", *table_fname = "example.cksum";

  if (argc > 1)
    fname = argv[1];
  if (argc > 2)
    table_fname = argv[2];

  // Open the dataset
  streamID = streamOpenRead(fname);
  if ( streamID < 0 )
    {
      fprintf(stderr, "Cannot open data input file %s: %s\n",
              fname, cdiStringError(streamID));
      return(1);
    }

  // Get the variable list of the dataset
  vlistID = streamInqVlist(streamID);

  nvars = vlistNvars(vlistID);
  var = malloc(nvars * sizeof (var[0]));
  checksum_state = calloc(nvars, sizeof (checksum_state[0]));
  ngrids = vlistNgrids(vlistID);
  nzaxis = vlistNzaxis(vlistID);
  if (nzaxis < 0 || ngrids < 0)
    {
      fprintf(stderr, "Error in grid/zaxis count query %d:%d\n",
              ngrids, nzaxis);
    }
  varSize = malloc(nvars * sizeof (varSize[0]));

  for (i = 0; i < nvars; ++i)
    {
      int grid = vlistInqVarGrid(vlistID, i), gridType;
      int zaxis = vlistInqVarZaxis(vlistID, i);
      if (grid == CDI_UNDEFID || zaxis == CDI_UNDEFID)
        {
          fputs("error in axis/grid inquiry\n", stderr);
          exit(EXIT_FAILURE);
        }
      if ((varSize[i].z = zaxisInqSize(zaxis)) <= 0)
        {
          fputs("invalid Z-axis found\n", stderr);
          exit(EXIT_FAILURE);
        }
      if ((gridType = gridInqType(grid)) != GRID_LONLAT
          && gridType != GRID_GENERIC)
        {
          fprintf(stderr, "unexpected non-lonlat grid found: %d\n",
                  gridType);
          exit(EXIT_FAILURE);
        }
      if ((varSize[i].x = gridInqXsize(grid)) < 0)
        {
          fprintf(stderr, "invalid X-size found: %d\n", varSize[i].x);
          exit(EXIT_FAILURE);
        }
      if (varSize[i].x == 0) varSize[i].x = 1;
      if ((varSize[i].y = gridInqYsize(grid)) < 0)
        {
          fprintf(stderr, "invalid Y-size found: %d\n", varSize[i].y);
          exit(EXIT_FAILURE);
        }
      if (varSize[i].y == 0) varSize[i].y = 1;
      varSize[i].chars = (size_t)varSize[i].x * varSize[i].y * varSize[i].z
        * sizeof (var[0][0]);
      var[i] = malloc(varSize[i].chars);
    }

  // Get the Time axis from the variable list
  taxisID = vlistInqTaxis(vlistID);

  tsID = 0;
  // Inquire the time step
  while (streamInqTimestep(streamID, tsID))
    {
      // Get the verification date and time
      vdate = taxisInqVdate(taxisID);
      vtime = taxisInqVtime(taxisID);

      // Read var1 and var2
      for (i = 0; i < nvars; ++i)
        {
          int nmiss;
          streamReadVar(streamID, i, var[i], &nmiss);
          memcrc_r(checksum_state + i, (const unsigned char *)var[i],
                   varSize[i].chars);
        }
      ++tsID;
    }

  // check checksums from table file
  int retcode = EXIT_SUCCESS;
  {
    uint32_t cksum[nvars];
    unsigned long cksum_temp;
    int code, varCodes[nvars], checked[nvars];
    FILE *tablefp;
    for (i = 0; i < nvars; ++i)
      {
        cksum[i] = memcrc_finish(checksum_state + i,
                                 (off_t)varSize[i].chars * tsID);
        varCodes[i] = vlistInqVarCode(vlistID, i);
        checked[i] = 0;
      }
    if (!(tablefp = fopen(table_fname, "r")))
      {
        fputs("bla\n", stderr);
        perror("failed to open table file");
        exit(EXIT_FAILURE);
      }
    while (fscanf(tablefp, "%08lx %d\n", &cksum_temp, &code) == 2)
      {
        for (i = 0; i < nvars; ++i)
          if (varCodes[i] == code)
            {
              if (cksum[i] != cksum_temp)
                {
                  fprintf(stderr, "checksum error for varID %d, code %d!\n"
                          "%08lx != %08lx\n", i, code,
                          (unsigned long)cksum[i], cksum_temp);
                  retcode = EXIT_FAILURE;
                }
              checked[i] = 1;
              break;
            }
      }
    fclose(tablefp);
    for (i = 0; i < nvars; ++i)
      if (!checked[i])
        {
          fprintf(stderr, "variable %d, code %d in file not checked!\n",
                  i, varCodes[i]);
          exit(EXIT_FAILURE);
        }
  }
  // Close the input stream
  streamClose(streamID);

  return retcode;
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
