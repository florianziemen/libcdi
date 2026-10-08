#include "tstore.h"
#include "tsio.h"

#include <array>

static std::array<tsHandle *, 32> tsHandleIDs{ nullptr };

const char *
ts_lib_version(void)
{
  return tsVersion();
}

int
ts_open(const char *fileName, const char *mode, int fileType)
{
  printf("%s: %s\n", __func__, fileName);
  tsConfig config;
  tsHandle *handle = tsOpenFileWrite(fileName, config);
  for (size_t i = 0; i < (int) tsHandleIDs.size(); ++i)
  {
    if (tsHandleIDs[i] == nullptr)
    {
      tsHandleIDs[i] = handle;
      return static_cast<int>(i);
    }
  }
  return -1;
}
