#include <stdlib.h>
#include <cdi.h>


int main(void) {
  printf("\ncdi-tiles example\n-----------------\n\n");

  /* Register additional GRIB keys which are read when file is opened. */
  cdiDefAdditionalKey("identificationNumberOfTile");
  cdiDefAdditionalKey("attribute");
  
  int streamID = streamOpenRead("test.grb");

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
      char name[CDI_MAX_NAME];
      vlistInqVarName(vlistID, varID, name);
      
      printf("variable %-10.10s\n", name);
    }

  streamClose(streamID);
  printf("done.\n");

  return 0;
}
