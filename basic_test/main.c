#include <stdlib.h>
#include <cdi.h>


int main(void) {
  printf("\ncdi-tiles example\n-----------------\n\n");

  /* Register additional GRIB keys which are read when file is opened. */
  cdiDefAdditionalKey("identificationNumberOfTile");
  cdiDefAdditionalKey("attribute");
  
  int streamID = streamOpenRead("NWP_tiles_test.grb");

  /* loop over variables */
  int vlistID  = streamInqVlist(streamID);
  int nvars    = streamInqNvars(streamID);

  int 
    identificationNumberOfTile = 4,
    attribute                  = 4;
  printf("identificationNumberOfTile = %d\n",   identificationNumberOfTile);
  printf("attribute                  = %d\n\n", attribute);

  for (int varID=0; varID<nvars; varID++) 
    {
      int subtypeID  = vlistInqVarSubtype(vlistID, varID);
      int tile_index = subtypeInqTile(subtypeID, identificationNumberOfTile, attribute);

      char name[CDI_MAX_NAME];
      vlistInqVarName(vlistID, varID, name);

      if (tile_index != CDI_UNDEFID) {

	subtypeDefActiveIndex(subtypeID, tile_index);
	printf("variable %-10.10s: subentry ID = %d\n", name, tile_index);
	printf("                     identificationNumberOfTile = %d\n", 
	       vlistInqVarIntKey(vlistID, varID, "identificationNumberOfTile"));
	printf("                     attribute = %d\n", 
	       vlistInqVarIntKey(vlistID, varID, "attribute"));
	
	/* query grid size, allocate buffer */
	int gridID = vlistInqVarGrid(vlistID, varID);
	int size = gridInqSize(gridID);
	
	double* data_vec = (double*) malloc(size*sizeof(double));
	int nmiss;
	streamReadVarSlice(streamID, varID, 0, data_vec, &nmiss);
	
	double avg = 0;
	for (int i=0; i<size; i++)
	  avg += data_vec[i];
	avg /= size;
	printf("                     first val=%e, average: %6.3f\n", data_vec[0], avg);
	free(data_vec);
      } else {
	printf("variable %-10.10s: no matching record found!\n", name);
      }
    }

  streamClose(streamID);
  printf("done.\n");

  return 0;
}
