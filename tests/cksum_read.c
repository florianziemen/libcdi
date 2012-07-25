#include <assert.h>
#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>

#include "cdi.h"
#include "cksum.h"
#include "ensure_array_size.h"
#include "pio_util.h"

struct cksum_table
{
  int code;
  uint32_t cksum;
};

struct varSize
{
  int x, y, z;
  size_t chars;
};

static struct cksum_table *
read_table(const char *table_fname, size_t *table_len)
{
  struct cksum_table *table = NULL;
  FILE *tablefp;
  size_t table_size = 0, table_used = 0;
  unsigned long cksum_temp;
  int code;

  if (!(tablefp = fopen(table_fname, "r")))
    {
      perror("failed to open table file");
      *table_len = -1;
      return NULL;
    }
  while (fscanf(tablefp, "%08lx %d\n", &cksum_temp, &code) == 2)
    {
      ENSURE_ARRAY_SIZE(table, table_size, table_used + 1);
      table[table_used].code = code;
      table[table_used].cksum = cksum_temp;
      ++table_used;
    }
  fclose(tablefp);
  *table_len = table_used;
  return table;
}


static int
compare_checksums(struct cksum_table a[], size_t a_size, const char *src_a,
                  struct cksum_table b[], size_t b_size, const char *src_b)
{
  int checked_a[a_size], checked_b[b_size];
  size_t i, j;
  int retcode = EXIT_SUCCESS;

  for (i = 0; i < a_size; ++i)
    checked_a[i] = 0;
  for (j = 0; j < b_size; ++j)
    checked_b[j] = 0;

  for (j = 0; j < b_size; ++j)
    for (i = 0; i < a_size; ++i)
      if (a[i].code == b[j].code)
      {
        if (a[i].cksum != b[j].cksum)
          {
            fprintf(stderr, "checksum error for varID %d, code %d!\n"
                    "%08lx != %08lx\n", i, a[i].code,
                    (unsigned long)a[i].cksum, b[j].cksum);
            retcode = EXIT_FAILURE;
          }
        checked_a[i] = 1;
        checked_b[j] = 1;
        break;
      }

  for (i = 0; i < a_size; ++i)
    if (!checked_a[i])
      {
        fprintf(stderr, "variable %d, code %d from %s not checked!\n",
                i, a[i].code, src_a);
        retcode = EXIT_FAILURE;
      }
  for (j = 0; j < b_size; ++j)
    if (!checked_b[j])
      {
        fprintf(stderr, "variable %d, code %d from %s not checked!\n",
                j, b[j].code, src_b);
        retcode = EXIT_FAILURE;
      }
  return retcode;
}

static struct cksum_table *
cksum_stream(const char *fname, size_t *table_len)
{
  int taxisID, vlistID, streamID, tsID;
  int vdate, vtime, nvars, ngrids, nzaxis;
  int i;
  uint32_t *checksum_state;
  struct varSize *varSize;

  double **var;

  // Open the dataset
  streamID = streamOpenRead(fname);
  if ( streamID < 0 )
    {
      fprintf(stderr, "Cannot open data input file %s: %s\n",
              fname, cdiStringError(streamID));
      return NULL;
    }

  // Get the variable list of the dataset
  vlistID = streamInqVlist(streamID);

  nvars = vlistNvars(vlistID);
  ngrids = vlistNgrids(vlistID);
  nzaxis = vlistNzaxis(vlistID);
  if (nzaxis < 0 || ngrids < 0)
    {
      fprintf(stderr, "Error in grid/zaxis count query %d:%d\n",
              ngrids, nzaxis);
      return NULL;
    }
  var = xmalloc(nvars * sizeof (var[0]));
  checksum_state = xcalloc(nvars, sizeof (checksum_state[0]));
  varSize = xmalloc(nvars * sizeof (varSize[0]));

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
      var[i] = xmalloc(varSize[i].chars);
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

  struct cksum_table *file_vars = xmalloc(nvars * sizeof (file_vars[0]));
  *table_len = nvars;
  for (i = 0; i < nvars; ++i)
    {
      file_vars[i].code = vlistInqVarCode(vlistID, i);
      file_vars[i].cksum = memcrc_finish(checksum_state + i,
                                         (off_t)varSize[i].chars * tsID);
    }
  // Close the input stream
  streamClose(streamID);

  return file_vars;
}

int main(int argc, char *argv[])
{
  char *fname = "example.grb", *table_fname = "example.cksum";

  if (argc > 1)
    fname = argv[1];
  if (argc > 2)
    table_fname = argv[2];

  // compute checksums from data file
  size_t nvars;
  struct cksum_table *file_vars = cksum_stream(fname, &nvars);
  if (!file_vars)
    exit(EXIT_FAILURE);
  // check checksums from table file
  int retcode;
  {
    size_t num_ref_entries;
    struct cksum_table *ref_var_table
      = read_table(table_fname, &num_ref_entries);
    if (num_ref_entries == -1)
      exit(EXIT_FAILURE);
    retcode = compare_checksums(file_vars, nvars, "file",
                                ref_var_table, num_ref_entries,
                                "reference table");
    free(ref_var_table);
  }

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
