#if defined (HAVE_CONFIG_H)
#  include "config.h"
#endif

#include "dmemory.h"

#include "cdi.h"
#include "stream_int.h"


static void tstepsInitEntry(int streamID, int tsID)
{
  streams[streamID].tsteps[tsID].curRecID     = CDI_UNDEFID;
  streams[streamID].tsteps[tsID].position     = 0;
  streams[streamID].tsteps[tsID].records      = NULL;
  streams[streamID].tsteps[tsID].recordSize   = 0;
  streams[streamID].tsteps[tsID].nallrecs     = 0;
  streams[streamID].tsteps[tsID].recIDs       = NULL;
  streams[streamID].tsteps[tsID].nrecs        = 0;
  streams[streamID].tsteps[tsID].next         = 0;

  ptaxisInit(&streams[streamID].tsteps[tsID].taxis);
}

int tstepsNewEntry(int streamID)
{
  static char func[] = "tstepsNewEntry";
  int tsID = 0;
  int tstepsTableSize;
  TSTEPS *tstepsTable;

  tsID            = streams[streamID].tstepsNextID++;
  tstepsTableSize = streams[streamID].tstepsTableSize;
  tstepsTable     = streams[streamID].tsteps;

  /*
    If the table overflows, double its size.
  */
  if ( tsID == tstepsTableSize )
    {
      if ( tstepsTableSize == 0 ) tstepsTableSize = 1;
      tstepsTableSize = 2*tstepsTableSize;
      tstepsTable = (TSTEPS *) realloc(tstepsTable, tstepsTableSize*sizeof(TSTEPS));
      if ( tstepsTable == NULL )
	{
          Message(func, "tstepsTableSize = %d", tstepsTableSize);
	  SysError(func, "Reallocation of TSTEPS failed");
	}
    }

  streams[streamID].tstepsTableSize = tstepsTableSize;
  streams[streamID].tsteps          = tstepsTable;

  tstepsInitEntry(streamID, tsID);

  streams[streamID].tsteps[tsID].taxis.used = TRUE;

  return (tsID);
}

void cdiCreateTimesteps(int streamID)
{
  static char func[] = "cdiCreateTimesteps";
  int ntsteps;
  int tsID;

  if ( streams[streamID].ntsteps < 0 || streams[streamID].tstepsTableSize > 0 )
    return;

  if ( streams[streamID].ntsteps == 0 ) ntsteps = 1;    /* <<<<<-------- */
  else ntsteps = streams[streamID].ntsteps;

  streams[streamID].tsteps = (TSTEPS *) malloc(ntsteps*sizeof(TSTEPS));
  if ( streams[streamID].tsteps == NULL )
    SysError(func, "Allocation of TSTEPS failed");

  streams[streamID].tstepsTableSize = ntsteps;
  streams[streamID].tstepsNextID    = ntsteps;

  for ( tsID = 0; tsID < ntsteps; tsID++ )
    {
      tstepsInitEntry(streamID, tsID);
      streams[streamID].tsteps[tsID].taxis.used = TRUE;
    }
}
