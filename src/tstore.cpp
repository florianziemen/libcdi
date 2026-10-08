#include "tstore.h"
#include "tsio.h"

#include <array>

static std::array<tsHandle *, 32> tsHandleIDs{ nullptr };

static tsHandle *
get_tsHandle(int fileID)
{
  if (fileID < 0 || fileID >= (int) tsHandleIDs.size()) fprintf(stderr, "%s: fileID=%d undefined\n", __func__, fileID);
  tsHandle *handle = tsHandleIDs[fileID];
  if (handle == nullptr) fprintf(stderr, "%s: Undefined handle for fileID=%d\n", __func__, fileID);
  return handle;
}

const char *
ts_lib_version(void)
{
  return tsVersion();
}

int
ts_open(const char *fileName, const char *mode, int fileType)
{
  tsConfig config;
  tsHandle *handle = tsOpenFileWrite(fileName, config);
  for (size_t i = 0; i < (int) tsHandleIDs.size(); ++i)
  {
    if (tsHandleIDs[i] == nullptr)
    {
      tsHandleIDs[i] = handle;
      printf("%s: %s fileID=%d\n", __func__, fileName, (int) i);
      return static_cast<int>(i);
    }
  }
  return -1;
}

void
ts_close(int fileID)
{
  printf("%s: fileID=%d\n", __func__, fileID);
  tsCloseFile(get_tsHandle(fileID));
}
