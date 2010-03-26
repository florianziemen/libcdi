#include <stdio.h>
#include "cdi.h"

#define NOPEN 8096
#define FILENAME "example.grb"

int main(void)
{
  int iopen;
  int taxisID, vlistID, streamID;
  int streamIDs[NOPEN];

  for ( iopen = 0; iopen < NOPEN; ++iopen )
    {
      fprintf(stderr, "sequential open %d\n", iopen+1);
      /* Open the dataset */
      streamID = streamOpenRead(FILENAME);
      if ( streamID < 0 )
	{
	  fprintf(stderr, "%s\n", cdiStringError(streamID));
	  return(1);
	}

      /* Get the variable list of the dataset */
      vlistID = streamInqVlist(streamID);

      /* Get the Time axis from the variable list */
      taxisID = vlistInqTaxis(vlistID);
      
      /* Close the input stream */
      streamClose(streamID);
    }

  for ( iopen = 0; iopen < NOPEN; ++iopen )
    {
      fprintf(stderr, "simultaneous open %d\n", iopen+1);
      /* Open the dataset */
      streamID = streamOpenRead(FILENAME);
      if ( streamID < 0 )
	{
	  fprintf(stderr, "%s\n", cdiStringError(streamID));
	  return(1);
	}

      /* Get the variable list of the dataset */
      vlistID = streamInqVlist(streamID);

      /* Get the Time axis from the variable list */
      taxisID = vlistInqTaxis(vlistID);

      streamIDs[iopen] = streamID;
    }
      
  for ( iopen = 0; iopen < NOPEN; ++iopen )
    {
      /* Close the input stream */
      streamClose(streamIDs[iopen]);
    }

  return 0;
}
