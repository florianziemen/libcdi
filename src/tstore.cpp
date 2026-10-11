#include "tstore.h"
#include "tsio.h"

#include <array>
#include <set>
#include <string>

static std::array<tsHandle *, 32> tsHandleIDs{ nullptr };

static int
get_next_fileID(tsHandle *handle)
{
  if (!handle) return -1;

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
  int fileID = -1;
  int fmode = tolower(*mode);
  switch (fmode)
  {
    case 'r':
    {
      printf("open %s\n", fileName + 6);
      tsConfig cfg;
      cfg.path = fileName + 6;
      tsOverview ov;
      ov.ok = tsDatasetOverview(cfg, ov);
      if (!ov.ok) return -1;
      if (!ov.groups.empty())
      {
        printf("Found %zu groups in %s, select one of them:\n", ov.groups.size(), fileName + 6);
        for (const auto &group : ov.groups) printf(" %s", group.c_str());
        printf("\n");
        return -1;
      }
      printf("Found %zu arrays in %s:\n", ov.arrays.size(), fileName + 6);
      for (const auto &row : ov.arrays) printf(" %s", row.name.c_str());
      printf("\n");
      std::set<std::string> coord_names;
      for (const auto &row : ov.arrays)
        for (const auto &d : row.dim_names) coord_names.insert(d);
      for (const auto &row : ov.arrays)
        if (!row.grid_mapping.empty()) coord_names.insert(row.grid_mapping);
      cfg.arrays.clear();
      for (const auto &row : ov.arrays)
        if (!coord_names.count(row.name)) cfg.arrays.push_back(row.name);
      if (cfg.arrays.empty())
      {
        printf("No data arrays found in %s!", fileName + 6);
        return -1;
      };

      tsHandle *handle = tsOpenFileRead(fileName + 6, cfg);
      if (!handle) return -1;

      fileID = get_next_fileID(handle);
      break;
    }
    case 'w':
      tsConfig cfg;
      fileID = get_next_fileID(tsOpenFileWrite(fileName, cfg));
      break;
  }

  printf("%s: %s fileID=%d\n", __func__, fileName, fileID);
  if (fileID < 0) fprintf(stderr, "%s: fileID failed for %s!", __func__, fileName);
  return fileID;
}

void
ts_close(int fileID)
{
  printf("%s: fileID=%d\n", __func__, fileID);
  tsCloseFile(get_tsHandle(fileID));
}

int
ts_inq_contents(stream_t *streamP)
{
  printf("%s: fileID=%d\n", __func__, streamP->fileID);

  auto handle = get_tsHandle(streamP->fileID);
  tsMetadata const &md = tsGetReadMetadata(handle);
  printf("numSteps %lld\n", md.num_steps);
  printf("Found %zu data arrays:\n", md.arrayList.size());
  for (auto const &array : md.arrayList) printf(" %s", array.name.c_str());
  printf("\n");
  auto const &array = md.arrayList[0];
  printf("%s: num_steps=%lld, num_layers=%lld, num_cells=%lld\n", array.name.c_str(), array.num_steps, array.num_layers,
         array.num_cells);
  auto const &attributes = array.attributes;
  for (const auto &[key, value] : attributes) printf("[%s] = %s\n", key.c_str(), value.c_str());

  printf("Found %zu coordinate arrays:\n", md.coordList.size());
  for (auto const &array : md.coordList) printf("  %s: size=%lld\n", array.name.c_str(), array.shape[0]);

  return -1;
}
