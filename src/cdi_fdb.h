#ifndef CDI_FDB_H
#define CDI_FDB_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

extern int cdi_fdb_dummy;

#ifdef HAVE_LIBFDB5

#include <fdb5/api/fdb_c.h>

typedef struct
{
  char *item;
  char *keys[32];
  char *values[32];
  int numKeys;
} KeyValueItem;

typedef struct
{
  char *keys[32];
  char *values[32];
  int numKeys;
} fdbKeyValueEntry;

typedef struct
{
  int date;
  int time;
  int param;
  int levtype;
  int ilevel;
} RecordInfoEntry;

void check_fdb_error(int errorNum);
void fdb_delete_kvlist(int numItems, fdbKeyValueEntry *keyValueList);
void decode_fdbitem(const char *fdbItem, KeyValueItem *keyValue);
int fdb_fill_kvlist(fdb_handle_t *fdb, fdb_request_t *request, fdbKeyValueEntry **keyValueList);
long fdb_read_record(fdb_handle_t *fdb, const fdbKeyValueEntry *keyValue, size_t *buffersize, void **gribbuffer);
int check_keyvalueList(int numItems, fdbKeyValueEntry *keyValueList);
void record_info_entry_init(RecordInfoEntry *recordInfo);
int get_num_records(int numItems, RecordInfoEntry *recordInfoList);
void decode_keyvalue(fdbKeyValueEntry *keyValue, RecordInfoEntry *recordInfo);
int remove_duplicate_timesteps(RecordInfoEntry *recordInfoList, int numRecords, int numTimesteps, int *timestepRecordOffset);
fdb_request_t *create_fdb_request(const char *filename);

#endif

#endif /* CDI_FDB_H */
