#include "tsio.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <nlohmann/json.hpp>

#include "absl/strings/cord.h"
#include "tensorstore/context.h"
#include "tensorstore/internal/version/version.h"
#include "tensorstore/data_type.h"
#include "tensorstore/index_space/dim_expression.h"
#include "tensorstore/kvstore/kvstore.h"
#include "tensorstore/kvstore/operations.h"
#include "tensorstore/open.h"
#include "tensorstore/tensorstore.h"

using tensorstore::DimensionIndex;
using tensorstore::dynamic_rank;
using tensorstore::Index;
using tensorstore::SharedArray;
using tensorstore::TensorStore;
using AnyStoreType = tensorstore::TensorStore<void, tensorstore::dynamic_rank>;

namespace
{
void
resolve_kvstore(const tsConfig &cfg, std::string &kvstore, std::string &path, std::optional<std::string> &endpoint)
{
  kvstore = cfg.kvstore;
  path = cfg.path;
  endpoint = cfg.endpoint;

  if (kvstore.empty()) { kvstore = (path.find("://") == std::string::npos) ? "file" : "http"; }

  if (kvstore == "s3" && path.rfind("https://", 0) == 0)
  {
    const std::size_t slash = path.find('/', 8);
    const std::string derived_endpoint = (slash == std::string::npos) ? path : path.substr(0, slash);
    path = (slash == std::string::npos) ? "" : path.substr(slash + 1);
    if (!endpoint) endpoint = derived_endpoint;
  }
}

std::string
sanitize_invalid_json(const std::string &src)
{
  static const std::array<std::string_view, 3> kTokens = { "NaN", "Infinity", "-Infinity" };

  auto is_ident_char = [](char c) { return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_' || c == '-' || c == '.'; };

  std::string out;
  out.reserve(src.size());
  bool in_string = false;
  bool escape = false;
  for (std::size_t i = 0; i < src.size();)
  {
    const char c = src[i];
    if (in_string)
    {
      out.push_back(c);
      ++i;
      if (escape) { escape = false; }
      else if (c == '\\') { escape = true; }
      else if (c == '"') { in_string = false; }
      continue;
    }
    if (c == '"')
    {
      in_string = true;
      out.push_back(c);
      ++i;
      continue;
    }

    bool replaced = false;
    for (const std::string_view token : kTokens)
    {
      if (src.compare(i, token.size(), token) != 0) continue;
      const bool prev_ok = (i == 0) || !is_ident_char(src[i - 1]);
      const bool next_ok = (i + token.size() >= src.size()) || !is_ident_char(src[i + token.size()]);
      if (!prev_ok || !next_ok) continue;
      out.push_back('"');
      out.append(token);
      out.push_back('"');
      i += token.size();
      replaced = true;
      break;
    }
    if (!replaced)
    {
      out.push_back(c);
      ++i;
    }
  }
  return out;
}

struct KvstoreResult
{
  nlohmann::json spec;
  // True if `spec` already locates the individual array (its root is the
  // array directory); in that case no `"path"` may be set on the zarr spec.
  bool path_embedded = false;
  // Array path relative to the kvstore root, used when `!path_embedded`.
  // Empty means the plain array name is used.
  std::string path;
};

std::string
kvstore_mem_url(const std::string &array_name, const std::string &tag)
{
  static std::atomic<std::size_t> counter{ 0 };
  return "memory://ts_zarr_" + tag + "_" + array_name + "_" + std::to_string(counter.fetch_add(1));
}

nlohmann::json
make_s3_kvstore(const std::string &bucket, const std::string &prefix, const std::optional<std::string> &endpoint)
{
  nlohmann::json kv{
    { "driver", "s3" }, { "bucket", bucket }, { "path", prefix }, { "aws_credentials", { { "type", "anonymous" } } }
  };
  if (endpoint) kv["endpoint"] = *endpoint;
  return kv;
}

nlohmann::json
build_root_kvstore(const tsConfig &cfg, tensorstore::Context &context)
{
  (void) context;
  using namespace tensorstore;

  std::string kvstore, path;
  std::optional<std::string> endpoint;
  resolve_kvstore(cfg, kvstore, path, endpoint);

  if (kvstore == "file") return { { "driver", "file" }, { "path", cfg.path + "/" } };

  if (kvstore == "http")
  {
    const std::size_t scheme = path.find("://");
    const std::size_t slash = path.find('/', scheme + 3);
    const std::string base_url = path.substr(0, slash);
    const std::string remainder = (slash == std::string::npos) ? std::string() : path.substr(slash + 1);
    return { { "driver", "http" }, { "base_url", base_url }, { "path", "/" + remainder + "/" } };
  }

  // s3
  const auto sl = path.find('/');
  const std::string bucket = (sl == std::string::npos) ? path : path.substr(0, sl);
  const std::string prefix = (sl == std::string::npos) ? "" : path.substr(sl + 1);
  return make_s3_kvstore(bucket, prefix + "/", endpoint);
}

std::map<std::string, std::string>
read_consolidated_metadata(const nlohmann::json &group_kvstore, tensorstore::Context &context, const std::string &array_name)
{
  auto group_store = tensorstore::kvstore::Open(group_kvstore, context).result();
  if (!group_store) { throw std::runtime_error("Failed to open group kvstore: " + group_store.status().ToString()); }

  auto read_result = tensorstore::kvstore::Read(*group_store, ".zmetadata").result();
  if (!read_result) { throw std::runtime_error("Failed to read '.zmetadata': " + read_result.status().ToString()); }
  if (!read_result->has_value()) { throw std::runtime_error("No '.zmetadata' found (consolidated metadata missing)"); }

  std::string raw;
  for (const absl::string_view chunk : read_result->value.Chunks()) raw.append(chunk.data(), chunk.size());

  nlohmann::json root;
  try
  {
    root = nlohmann::json::parse(raw);
  }
  catch (const std::exception &e)
  {
    throw std::runtime_error("Invalid JSON in '.zmetadata': " + std::string(e.what()));
  }
  if (!root.contains("metadata") || !root["metadata"].is_object())
  {
    throw std::runtime_error("'.zmetadata' has no 'metadata' object");
  }

  static const std::array<std::string_view, 3> kMetadataKeys = { ".zarray", ".zattrs", ".zgroup" };
  const nlohmann::json &metadata = root["metadata"];
  std::map<std::string, std::string> out;
  for (const std::string_view suffix : kMetadataKeys)
  {
    const std::string key = array_name + "/" + std::string(suffix);
    if (metadata.contains(key) && !metadata[key].is_null()) { out[std::string(suffix)] = metadata[key].dump(); }
  }
  return out;
}

std::map<std::string, std::string>
read_zarr3_consolidated(const nlohmann::json &root_kvstore, tensorstore::Context &context)
{
  using namespace tensorstore;

  auto store = kvstore::Open(root_kvstore, context).result();
  if (!store) return {};

  auto read_result = kvstore::Read(*store, "zarr.json").result();
  if (!read_result || !read_result->has_value()) return {};

  std::string raw;
  for (const absl::string_view chunk : read_result->value.Chunks()) raw.append(chunk.data(), chunk.size());

  nlohmann::json root;
  try
  {
    root = nlohmann::json::parse(raw);
  }
  catch (...)
  {
    return {};
  }
  if (!root.contains("consolidated_metadata") || !root["consolidated_metadata"].contains("metadata")
      || !root["consolidated_metadata"]["metadata"].is_object())
  {
    return {};
  }

  std::map<std::string, std::string> out;
  for (const auto &[key, value] : root["consolidated_metadata"]["metadata"].items()) { out[key] = value.dump(); }
  return out;
}

std::map<std::string, std::string>
read_zarr2_consolidated(const nlohmann::json &root_kvstore, tensorstore::Context &context)
{
  using namespace tensorstore;

  auto store = kvstore::Open(root_kvstore, context).result();
  if (!store) return {};

  auto read_result = kvstore::Read(*store, ".zmetadata").result();
  if (!read_result || !read_result->has_value()) return {};

  std::string raw;
  for (const absl::string_view chunk : read_result->value.Chunks()) raw.append(chunk.data(), chunk.size());

  nlohmann::json root;
  try
  {
    root = nlohmann::json::parse(raw);
  }
  catch (...)
  {
    return {};
  }
  if (!root.contains("metadata") || !root["metadata"].is_object()) return {};

  std::map<std::string, std::string> out;
  for (const auto &[key, value] : root["metadata"].items()) { out[key] = value.dump(); }
  return out;
}

std::vector<std::string>
zarr2_children(const std::map<std::string, std::string> &meta, const std::string &prefix, bool want_groups)
{
  const std::string pfx = prefix.empty() ? "" : prefix + "/";
  const std::string marker = want_groups ? ".zgroup" : ".zarray";
  std::set<std::string> names;
  for (const auto &[key, _] : meta)
  {
    if (key.rfind(pfx, 0) != 0) continue;
    std::string rest = key.substr(pfx.size());
    const std::size_t slash = rest.find('/');
    if (slash == std::string::npos) continue;  // e.g. ".zgroup", ".zattrs"
    const std::string head = rest.substr(0, slash);
    const std::string tail = rest.substr(slash + 1);
    if (tail != marker) continue;
    names.insert(head);
  }
  return std::vector<std::string>(names.begin(), names.end());
}

std::vector<std::string>
consolidated_children(const std::map<std::string, std::string> &meta, const std::string &prefix, bool want_groups)
{
  std::vector<std::string> names;
  for (const auto &[key, raw] : meta)
  {
    std::string base = key;
    if (!prefix.empty())
    {
      const std::string pfx = prefix + "/";
      if (base.rfind(pfx, 0) != 0) continue;
      base = base.substr(pfx.size());
    }
    const std::size_t slash = base.find('/');
    if (slash != std::string::npos) continue;  // not a direct child
    if (base.empty()) continue;

    nlohmann::json j;
    try
    {
      j = nlohmann::json::parse(raw);
    }
    catch (...)
    {
      continue;
    }
    const std::string node_type = j.value("node_type", "");
    const bool is_group = (node_type == "group");
    if (want_groups && !is_group) continue;
    if (!want_groups && is_group) continue;
    names.push_back(base);
  }
  std::sort(names.begin(), names.end());
  return names;
}

KvstoreResult
build_consolidated_kvstore(tensorstore::Context &context, const std::string &array_name, const nlohmann::json &group_kv,
                           const nlohmann::json &array_kv)
{
  const std::map<std::string, std::string> overlay = read_consolidated_metadata(group_kv, context, array_name);
  if (!overlay.count(".zarray"))
  {
    throw std::runtime_error("Consolidated metadata '.zmetadata' contains no '" + array_name + "/.zarray' entry");
  }

  const std::string mem_url = kvstore_mem_url(array_name, "consolidated");
  auto mem_store = tensorstore::kvstore::Open(mem_url, context).result();
  if (!mem_store) { throw std::runtime_error("Failed to open memory kvstore: " + mem_store.status().ToString()); }

  nlohmann::json layers = nlohmann::json::array();
  layers.push_back({ { "base", array_kv } });
  for (const auto &[key, value] : overlay)
  {
    auto write_result
        = tensorstore::kvstore::Write(*mem_store, key, std::optional<tensorstore::kvstore::Value>{ absl::Cord(value) }).result();
    if (!write_result)
    {
      throw std::runtime_error("Failed to write consolidated metadata '" + key + "': " + write_result.status().ToString());
    }
    layers.push_back({ { "base", mem_url }, { "exact", key }, { "strip_prefix", 0 } });
  }

  return { nlohmann::json{ { "driver", "kvstack" }, { "layers", std::move(layers) } }, /*path_embedded=*/true };
}

KvstoreResult
build_kvstack_overlay(tensorstore::Context &context, const nlohmann::json &root_kv, const std::string &array_name,
                      const std::string &tag, const std::vector<std::pair<std::string, std::string>> &key_json,
                      const std::string &path)
{
  using namespace tensorstore;

  const std::string mem_url = kvstore_mem_url(array_name, tag);
  auto mem_store = kvstore::Open(mem_url, context).result();
  if (!mem_store) { throw std::runtime_error("Failed to open memory kvstore: " + mem_store.status().ToString()); }

  nlohmann::json layers = nlohmann::json::array();
  layers.push_back({ { "base", root_kv } });
  for (const auto &[key, value] : key_json)
  {
    auto write_result
        = tensorstore::kvstore::Write(*mem_store, key, std::optional<tensorstore::kvstore::Value>{ absl::Cord(value) }).result();
    if (!write_result)
    {
      throw std::runtime_error("Failed to write consolidated metadata '" + key + "': " + write_result.status().ToString());
    }
    layers.push_back({ { "base", mem_url }, { "exact", key }, { "strip_prefix", 0 } });
  }

  return { nlohmann::json{ { "driver", "kvstack" }, { "layers", std::move(layers) } },
           /*path_embedded=*/false,
           /*path=*/path };
}

KvstoreResult
build_consolidated_group_kvstore(tensorstore::Context &context, const std::string &array_name, const std::string &group,
                                 const nlohmann::json &root_kv)
{
  using namespace tensorstore;

  // Zarr 3: consolidated metadata in the root `zarr.json` (`node` = path key).
  const std::map<std::string, std::string> meta3 = read_zarr3_consolidated(root_kv, context);
  if (!meta3.empty())
  {
    const std::string group_node = group;                     // "group"
    const std::string array_node = group + "/" + array_name;  // "group/array"
    if (!meta3.count(group_node))
    {
      throw std::runtime_error("No consolidated Zarr 3 metadata for group '" + group + "' (missing '" + group_node + "')");
    }
    if (!meta3.count(array_node))
    {
      throw std::runtime_error("No consolidated Zarr 3 metadata for array '" + array_name + "' (missing '" + array_node + "')");
    }
    return build_kvstack_overlay(
        context, root_kv, array_name, "group",
        { { group + "/zarr.json", meta3.at(group_node) }, { group + "/" + array_name + "/zarr.json", meta3.at(array_node) } },
        group + "/" + array_name);
  }

  // Zarr 2: consolidated metadata in the root `.zmetadata`.
  const std::map<std::string, std::string> meta2 = read_zarr2_consolidated(root_kv, context);
  if (!meta2.empty())
  {
    const std::string group_key = group + "/.zgroup";
    const std::string array_key = group + "/" + array_name + "/.zarray";
    if (!meta2.count(group_key))
    {
      throw std::runtime_error("No consolidated Zarr 2 metadata for group '" + group + "' (missing '" + group_key + "')");
    }
    if (!meta2.count(array_key))
    {
      throw std::runtime_error("No consolidated Zarr 2 metadata for array '" + array_name + "' (missing '" + array_key + "')");
    }
    std::vector<std::pair<std::string, std::string>> overlay
        = { { group_key, meta2.at(group_key) }, { array_key, meta2.at(array_key) } };
    const std::string attrs_key = group + "/" + array_name + "/.zattrs";
    if (meta2.count(attrs_key)) overlay.emplace_back(attrs_key, meta2.at(attrs_key));
    return build_kvstack_overlay(context, root_kv, array_name, "group", overlay, group + "/" + array_name);
  }

  throw std::runtime_error("No consolidated Zarr metadata (displayed 'zarr.json' / '.zmetadata') found at store root '"
                           + std::string(root_kv.dump()) + "'");
}

KvstoreResult
build_kvstore(const tsConfig &cfg, tensorstore::Context &context, const std::string &array_name)
{
  using namespace tensorstore;

  std::string kvstore, path;
  std::optional<std::string> endpoint;
  resolve_kvstore(cfg, kvstore, path, endpoint);

  if (cfg.group) { return build_consolidated_group_kvstore(context, array_name, *cfg.group, build_root_kvstore(cfg, context)); }

  if (kvstore == "s3")
  {
    const auto slash = path.find('/');
    std::string bucket = (slash == std::string::npos) ? path : path.substr(0, slash);
    std::string prefix = (slash == std::string::npos) ? "" : path.substr(slash + 1);

    if (cfg.consolidated)
    {
      nlohmann::json group_kv = make_s3_kvstore(bucket, prefix + "/", endpoint);
      nlohmann::json array_kv = make_s3_kvstore(bucket, prefix + "/" + array_name + "/", endpoint);
      return build_consolidated_kvstore(context, array_name, group_kv, array_kv);
    }

    nlohmann::json kv = make_s3_kvstore(bucket, prefix, endpoint);
    return { std::move(kv), /*path_embedded=*/false };
  }

  if (kvstore == "http")
  {
    const std::size_t scheme = path.find("://");
    if (scheme == std::string::npos)
    {
      throw std::runtime_error("http kvstore requires a full 'http://' or 'https://' URL as --path");
    }
    const std::string base_url = path.substr(0, path.find('/', scheme + 3));
    const std::string remainder
        = (path.find('/', scheme + 3) == std::string::npos) ? std::string() : path.substr(path.find('/', scheme + 3) + 1);

    nlohmann::json group_kv{ { "driver", "http" }, { "base_url", base_url }, { "path", "/" + remainder + "/" } };
    nlohmann::json array_kv{ { "driver", "http" }, { "base_url", base_url }, { "path", "/" + remainder + "/" + array_name + "/" } };

    if (!cfg.consolidated)
    {
      // Overlay store fed with sanitized metadata.
      const std::string mem_url = kvstore_mem_url(array_name, "sanitize");

      nlohmann::json layers = nlohmann::json::array();
      layers.push_back({ { "base", array_kv } });

      auto base_store = tensorstore::kvstore::Open(array_kv, context).result();
      if (!base_store) { throw std::runtime_error("Failed to open http kvstore: " + base_store.status().ToString()); }
      auto mem_store = tensorstore::kvstore::Open(mem_url, context).result();
      if (!mem_store) { throw std::runtime_error("Failed to open memory kvstore: " + mem_store.status().ToString()); }

      static const std::array<std::string_view, 4> kMetadataKeys = { ".zarray", ".zattrs", ".zgroup", "zarr.json" };
      for (const std::string_view key : kMetadataKeys)
      {
        auto read_result = tensorstore::kvstore::Read(*base_store, key).result();
        if (!read_result)
        {
          throw std::runtime_error("Failed to read remote metadata '" + std::string(key) + "': " + read_result.status().ToString());
        }
        if (!read_result->has_value()) continue;

        std::string raw;
        for (const absl::string_view chunk : read_result->value.Chunks()) raw.append(chunk.data(), chunk.size());
        raw = sanitize_invalid_json(raw);

        auto write_result
            = tensorstore::kvstore::Write(*mem_store, key, std::optional<tensorstore::kvstore::Value>{ absl::Cord(std::move(raw)) })
                  .result();
        if (!write_result)
        {
          throw std::runtime_error("Failed to write sanitized metadata '" + std::string(key)
                                   + "': " + write_result.status().ToString());
        }

        // `strip_prefix: 0` keeps the metadata key itself as the memory key, so
        // multiple overlaid keys do not collide in the single overlay store.
        layers.push_back({ { "base", mem_url }, { "exact", key }, { "strip_prefix", 0 } });
      }

      return { nlohmann::json{ { "driver", "kvstack" }, { "layers", std::move(layers) } }, /*path_embedded=*/true };
    }

    return build_consolidated_kvstore(context, array_name, group_kv, array_kv);
  }

  // --- file ---
  if (cfg.consolidated)
  {
    const nlohmann::json group_kv{ { "driver", "file" }, { "path", cfg.path + "/" } };
    const nlohmann::json array_kv{ { "driver", "file" }, { "path", cfg.path + "/" + array_name + "/" } };
    return build_consolidated_kvstore(context, array_name, group_kv, array_kv);
  }

  return { nlohmann::json{ { "driver", "file" }, { "path", cfg.path } }, /*path_embedded=*/false };
}

void
resolve_selection(const tsConfig &cfg, const std::vector<Index> &shape, std::vector<Index> &bstart, std::vector<Index> &bcount)
{
  bstart.assign(shape.size(), 0);
  bcount = shape;
  const auto apply = [&bstart, &bcount](const tsRange &range, std::size_t d)
  {
    bstart[d] = static_cast<Index>(range.start - 1);
    if (range.end) bcount[d] = static_cast<Index>(*range.end - range.start + 1);
  };
  apply(cfg.step, 0);
  if (shape.size() >= 3) apply(cfg.layer, 1);
  if (shape.size() >= 2) apply(cfg.cell, shape.size() - 1);
}

// Auswahl eines zeitkonstanten Arrays: es gibt keine Zeitdimension, die erste
// Dimension ist (bei rank >= 2) die Layer-Dimension.  `--step` wird ignoriert.
void
resolve_selection_static(const tsConfig &cfg, const std::vector<Index> &shape, std::vector<Index> &bstart,
                         std::vector<Index> &bcount)
{
  bstart.assign(shape.size(), 0);
  bcount = shape;
  const auto apply = [&bstart, &bcount](const tsRange &range, std::size_t d)
  {
    bstart[d] = static_cast<Index>(range.start - 1);
    if (range.end) bcount[d] = static_cast<Index>(*range.end - range.start + 1);
  };
  if (shape.size() >= 2) apply(cfg.layer, 0);
  if (shape.size() >= 1) apply(cfg.cell, shape.size() - 1);
}

// Zeitkonstant = Dimensionsnamen sind bekannt und enthalten kein "time".
bool
is_time_constant(const std::vector<std::string> &dim_names)
{
  if (dim_names.empty()) return false;
  return std::find(dim_names.begin(), dim_names.end(), "time") == dim_names.end();
}

template <typename T, tensorstore::DimensionIndex R>
std::string
read_store_key(const tensorstore::TensorStore<T, R> *store, const std::string &key)
{
  using namespace tensorstore;
  const KvStore kv = store->kvstore();
  if (!kv.valid()) return {};
  auto res = kvstore::Read(kv, key).result();
  if (!res || !res->has_value()) return {};
  std::string raw;
  for (const absl::string_view chunk : res->value.Chunks()) raw.append(chunk.data(), chunk.size());
  return raw;
}

template <typename T, tensorstore::DimensionIndex R>
std::vector<std::string>
read_array_dim_names(const tensorstore::TensorStore<T, R> *store, int format)
{
  std::vector<std::string> dims;
  const std::string meta = read_store_key(store, (format == 3) ? "zarr.json" : ".zarray");
  const std::string attrs = read_store_key(store, ".zattrs");
  try
  {
    if (!meta.empty())
    {
      const nlohmann::json j = nlohmann::json::parse(meta);
      if (j.contains("dimension_names") && j["dimension_names"].is_array())
      {
        for (const auto &d : j["dimension_names"]) dims.push_back(d.get<std::string>());
        if (!dims.empty()) return dims;
      }
    }
    if (!attrs.empty())
    {
      const nlohmann::json j = nlohmann::json::parse(attrs);
      if (j.contains("_ARRAY_DIMENSIONS") && j["_ARRAY_DIMENSIONS"].is_array())
      {
        for (const auto &d : j["_ARRAY_DIMENSIONS"]) dims.push_back(d.get<std::string>());
      }
    }
  }
  catch (...)
  {
    dims.clear();
  }
  return dims;
}

template <typename T, tensorstore::DimensionIndex R>
std::map<std::string, std::string>
read_array_attrs(const tensorstore::TensorStore<T, R> *store, int format)
{
  std::map<std::string, std::string> attrs;
  const std::string raw = read_store_key(store, (format == 3) ? "zarr.json" : ".zattrs");
  if (raw.empty()) return attrs;
  try
  {
    nlohmann::json j = nlohmann::json::parse(raw);
    if (j.contains("attributes") && j["attributes"].is_object()) j = j["attributes"];
    if (!j.is_object()) return attrs;
    for (auto it = j.begin(); it != j.end(); ++it)
    {
      const nlohmann::json &v = it.value();
      if (v.is_string())
        attrs[it.key()] = v.get<std::string>();
      else if (v.is_boolean())
        attrs[it.key()] = v.get<bool>() ? "true" : "false";
      else if (v.is_number_integer())
        attrs[it.key()] = std::to_string(v.get<long long>());
      else if (v.is_number_unsigned())
        attrs[it.key()] = std::to_string(v.get<unsigned long long>());
      else if (v.is_number_float())
        attrs[it.key()] = v.dump();
      else
        attrs[it.key()] = v.dump();
    }
  }
  catch (...)
  {
    attrs.clear();
  }
  return attrs;
}

template <typename T, tensorstore::DimensionIndex R>
void
read_source_metadata(const tensorstore::TensorStore<T, R> *store, int format, nlohmann::json &attributes,
                     nlohmann::json &dimension_names)
{
  attributes = nlohmann::json::object();
  dimension_names = nlohmann::json::array();
  const std::string raw = read_store_key(store, (format == 3) ? "zarr.json" : ".zattrs");
  if (raw.empty()) return;
  nlohmann::json j;
  try
  {
    j = nlohmann::json::parse(raw);
  }
  catch (...)
  {
    return;
  }
  if (!j.is_object()) return;
  if (format == 3)
  {
    if (j.contains("attributes") && j["attributes"].is_object()) attributes = j["attributes"];
    if (j.contains("dimension_names") && j["dimension_names"].is_array()) dimension_names = j["dimension_names"];
  }
  else
  {
    for (auto it = j.begin(); it != j.end(); ++it)
    {
      if (it.key() == "_ARRAY_DIMENSIONS" && it.value().is_array())
        dimension_names = it.value();
      else
        attributes[it.key()] = it.value();
    }
  }
}

template <typename Element>
std::optional<tensorstore::TensorStore<Element, tensorstore::dynamic_rank>>
open_array_impl(const tsConfig &cfg, tensorstore::Context &context, const std::string &array_name, std::string &kvstore_out,
                int &detected_format, bool quiet = false)
{
  using namespace tensorstore;

  std::string resolved_path;
  std::optional<std::string> resolved_endpoint;
  resolve_kvstore(cfg, kvstore_out, resolved_path, resolved_endpoint);

  KvstoreResult kv;
  try
  {
    kv = build_kvstore(cfg, context, array_name);
  }
  catch (const std::runtime_error &e)
  {
    if (!quiet) std::cerr << "Failed to open '" << array_name << "': " << e.what() << "\n";
    return std::nullopt;
  }

  // Try the zarr3 driver first, then zarr2 (or only the forced one).
  std::vector<std::string> candidates;
  if (cfg.format) { candidates = { (cfg.format == 3) ? "zarr3" : "zarr" }; }
  else
  {
    candidates = { "zarr3", "zarr" };
  }

  std::string last_error;
  for (const auto &driver : candidates)
  {
    nlohmann::json spec{ { "driver", driver }, { "kvstore", kv.spec } };
    if (!kv.path_embedded) spec["path"] = kv.path.empty() ? array_name : kv.path;

    auto store = Open<Element, dynamic_rank>(spec, context, OpenMode::open, ReadWriteMode::read).result();
    if (!store)
    {
      last_error = store.status().ToString();
      continue;
    }
    detected_format = (driver == "zarr3") ? 3 : 2;
    return *store;
  }

  if (!quiet) std::cerr << "Failed to open '" << array_name << "':\n" << last_error << "\n";
  return std::nullopt;
}

std::optional<AnyStoreType>
open_array_any(const tsConfig &cfg, tensorstore::Context &context, const std::string &array_name, std::string &kvstore_out,
               int &detected_format, bool quiet = false)
{
  return open_array_impl<void>(cfg, context, array_name, kvstore_out, detected_format, quiet);
}

bool
write_v2_zattrs_if_needed(const std::string &dir, const nlohmann::json &src_attrs, const nlohmann::json &src_dims)
{
  if (src_attrs.empty() && src_dims.empty()) return true;
  nlohmann::json zattrs = src_attrs;
  if (!src_dims.empty()) zattrs["_ARRAY_DIMENSIONS"] = src_dims;
  std::ofstream zf(dir + ".zattrs");
  if (!zf)
  {
    std::cerr << "Failed to write " << dir << ".zattrs\n";
    return false;
  }
  zf << zattrs.dump();
  return true;
}

std::string
zarr2_shuffle_to_v3(int shuffle)
{
  if (shuffle == 1) return "shuffle";
  if (shuffle == 2) return "bitshuffle";
  return "noshuffle";
}

int
zarr3_shuffle_to_v2(const std::string &shuffle)
{
  if (shuffle == "shuffle") return 1;
  if (shuffle == "bitshuffle") return 2;
  return 0;
}

std::optional<nlohmann::json>
v2_compressor_to_v3(const nlohmann::json &comp, int dtype_size)
{
  if (comp.is_null() || !comp.is_object()) return std::nullopt;
  const std::string id = comp.value("id", std::string());
  if (id == "zlib") return nlohmann::json{ { "name", "zlib" }, { "configuration", { { "level", comp.value("level", -1) } } } };
  if (id == "gzip") return nlohmann::json{ { "name", "gzip" }, { "configuration", { { "level", comp.value("level", -1) } } } };
  if (id == "blosc")
    return nlohmann::json{ { "name", "blosc" },
                           { "configuration",
                             { { "typesize", dtype_size },
                               { "cname", comp.value("cname", "lz4") },
                               { "clevel", comp.value("clevel", 5) },
                               { "shuffle", zarr2_shuffle_to_v3(comp.value("shuffle", 0)) },
                               { "blocksize", comp.value("blocksize", 0) } } } };
  return std::nullopt;
}

nlohmann::json
v3_codec_to_v2(const nlohmann::json &codec)
{
  const std::string name = (codec.is_object()) ? codec.value("name", std::string()) : std::string();
  const nlohmann::json conf = codec.value("configuration", nlohmann::json::object());
  if (name == "blosc")
    return nlohmann::json{ { "id", "blosc" },
                           { "cname", conf.value("cname", "lz4") },
                           { "clevel", conf.value("clevel", 5) },
                           { "shuffle", zarr3_shuffle_to_v2(conf.value("shuffle", "shuffle")) },
                           { "blocksize", conf.value("blocksize", 0) } };
  if (name == "zlib") return nlohmann::json{ { "id", "zlib" }, { "level", conf.value("level", -1) } };
  if (name == "gzip") return nlohmann::json{ { "id", "gzip" }, { "level", conf.value("level", -1) } };
  if (name == "zstd") return nlohmann::json{ { "id", "zstd" }, { "level", conf.value("level", 0) } };
  return nlohmann::json();
}

nlohmann::json
first_v3_compressor(const nlohmann::json &codecs)
{
  static const std::set<std::string> structural
      = { "bytes", "crc32c", "crc32c_no_separate_header", "transpose", "bitshuffle", "delta", "pcodec", "sharding_indexed" };
  if (!codecs.is_array()) return nlohmann::json();
  for (const auto &c : codecs)
  {
    const std::string name = c.is_object() ? c.value("name", std::string()) : std::string();
    if (name == "sharding_indexed")
    {
      const nlohmann::json inner = first_v3_compressor(c["configuration"].value("codecs", nlohmann::json::array()));
      if (!inner.is_null()) return inner;
      continue;
    }
    if (name.empty() || structural.count(name)) continue;
    return v3_codec_to_v2(c);
  }
  return nlohmann::json();
}

nlohmann::json
v2_compressor_to_n5(const nlohmann::json &comp)
{
  if (!comp.is_object()) return nlohmann::json{ { "type", "raw" } };
  const std::string id = comp.value("id", std::string());
  if (id == "gzip") return nlohmann::json{ { "type", "gzip" }, { "level", comp.value("level", -1) }, { "useZlib", false } };
  if (id == "zlib") return nlohmann::json{ { "type", "gzip" }, { "level", comp.value("level", -1) }, { "useZlib", true } };
  if (id == "zstd") return nlohmann::json{ { "type", "zstd" }, { "level", comp.value("level", 0) } };
  if (id == "blosc")
    return nlohmann::json{ { "type", "blosc" },
                           { "cname", comp.value("cname", "lz4") },
                           { "clevel", comp.value("clevel", 5) },
                           { "shuffle", comp.value("shuffle", 0) },
                           { "blocksize", comp.value("blocksize", 0) } };
  return nlohmann::json{ { "type", "raw" } };
}

// Zusaetzliche N5-Attribute fuer attributes.json: die Quell-Attribute, aber
// ohne die von N5 belegten Schluessel (die eine andere ZARR-Form erwarten),
// plus `_ARRAY_DIMENSIONS`.
nlohmann::json
n5_extra_attrs(const nlohmann::json &attrs, const nlohmann::json &dims)
{
  static const std::set<std::string> reserved
      = { "dimensions", "blockSize", "dataType", "compression", "axes", "units", "resolution" };
  nlohmann::json out = nlohmann::json::object();
  if (attrs.is_object())
  {
    for (auto it = attrs.begin(); it != attrs.end(); ++it)
    {
      if (!reserved.count(it.key())) out[it.key()] = it.value();
    }
  }
  if (!dims.empty()) out["_ARRAY_DIMENSIONS"] = dims;
  return out;
}

// Name -> zarr2-Kompressor-Dict fuer --compressor (kanonische Form, wie sie auch
// numcodecs im zarr2-Store versteht).
nlohmann::json
compressor_v2_by_name(const std::string &name)
{
  if (name == "blosc")
    return nlohmann::json{ { "id", "blosc" }, { "cname", "zstd" }, { "clevel", 5 }, { "shuffle", 1 }, { "blocksize", 0 } };
  if (name == "gzip") return nlohmann::json{ { "id", "gzip" }, { "level", 5 } };
  if (name == "zstd") return nlohmann::json{ { "id", "zstd" }, { "level", 5 } };
  return nlohmann::json();
}

// Name -> zarr3-Kompressor-Codec (ohne den bytes-Praeambel-Codec).
nlohmann::json
compressor_v3_by_name(const std::string &name, int dtype_size)
{
  if (name == "blosc")
    return nlohmann::json{
      { "name", "blosc" },
      { "configuration",
        { { "typesize", dtype_size }, { "cname", "zstd" }, { "clevel", 5 }, { "shuffle", "shuffle" }, { "blocksize", 0 } } }
    };
  if (name == "gzip") return nlohmann::json{ { "name", "gzip" }, { "configuration", { { "level", 5 } } } };
  if (name == "zstd") return nlohmann::json{ { "name", "zstd" }, { "configuration", { { "level", 5 }, { "checksum", false } } } };
  return nlohmann::json();
}

// Baut die Ausgabe-Kompression direkt nach `name` statt aus der Quelle.
void
apply_compressor_by_name(const std::string &name, const std::string &out_format, int dtype_size, nlohmann::json &md)
{
  if (out_format == "zarr3")
  {
    nlohmann::json codecs = nlohmann::json::array();
    codecs.push_back(nlohmann::json{ { "name", "bytes" }, { "configuration", { { "endian", "little" } } } });
    if (name != "none")
    {
      const nlohmann::json c = compressor_v3_by_name(name, dtype_size);
      if (!c.empty()) codecs.push_back(c);
    }
    md["codecs"] = std::move(codecs);
  }
  else if (out_format == "n5")
  {
    md["compression"] = (name == "none") ? nlohmann::json{ { "type", "raw" } } : v2_compressor_to_n5(compressor_v2_by_name(name));
  }
  else
  {
    md["compressor"] = (name == "none") ? nlohmann::json() : compressor_v2_by_name(name);
  }
}

// Wendet die Quell-Kompression (bereits geparstes Roh-.zarray/zarr.json der
// Quelle) auf die Ausgabe-Metadaten an; benoetigt keinen Store.
void
apply_source_compression_json(const nlohmann::json &j, int source_format, const std::string &out_format, int dtype_size,
                              const std::vector<Index> &out_chunks, nlohmann::json &md)
{
  if (!j.is_object()) return;

  const auto adapt_sharding = [&out_chunks](nlohmann::json &codecs)
  {
    if (!codecs.is_array()) return;
    for (auto &c : codecs)
    {
      if (!c.is_object() || c.value("name", std::string()) != "sharding_indexed") continue;
      auto &conf = c["configuration"];
      if (!conf.contains("chunk_shape") || !conf["chunk_shape"].is_array()) continue;
      for (std::size_t d = 0; d < conf["chunk_shape"].size() && d < out_chunks.size(); ++d)
      {
        Index outer = out_chunks[d];
        if (outer < 1) continue;
        Index inner = conf["chunk_shape"][d].get<Index>();
        if (outer % inner != 0)
        {
          if (inner > outer) inner = outer;
          while (outer % inner != 0) --inner;
          if (inner < 1) inner = 1;
          conf["chunk_shape"][d] = inner;
        }
      }
    }
  };

  if (out_format == "zarr3")
  {
    if (source_format == 3)
    {
      if (j.contains("codecs") && j["codecs"].is_array() && !j["codecs"].empty())
      {
        md["codecs"] = j["codecs"];
        adapt_sharding(md["codecs"]);
      }
    }
    else
    {
      // Zarr 2 -> Zarr 3: bytes endian codec plus the compressor as a codec.
      nlohmann::json arr = nlohmann::json::array();
      arr.push_back(nlohmann::json{ { "name", "bytes" }, { "configuration", { { "endian", "little" } } } });
      if (j.contains("compressor"))
      {
        const auto mapped = v2_compressor_to_v3(j["compressor"], dtype_size);
        if (mapped) arr.push_back(*mapped);
      }
      md["codecs"] = arr;
    }
  }
  else if (out_format == "n5")
  {
    // N5: compression object (N5 has no codecs pipeline).  Unknown or missing
    // compressors fall back to raw, like the other cross-format conversions.
    nlohmann::json comp;
    if (source_format == 2)
      comp = j.value("compressor", nlohmann::json());
    else
      comp = first_v3_compressor(j.value("codecs", nlohmann::json::array()));
    md["compression"] = v2_compressor_to_n5(comp);
  }
  else
  {
    if (source_format == 2)
    {
      if (j.contains("compressor")) md["compressor"] = j["compressor"];
    }
    else
    {
      md["compressor"] = first_v3_compressor(j.value("codecs", nlohmann::json::array()));
    }
  }
}

std::optional<std::string>
coord_dtype_encoded(const tensorstore::DataType &dtype, const std::string &zarr_format)
{
  using namespace tensorstore;
  if (zarr_format == "zarr3") return std::string(dtype.name());
  if (zarr_format == "n5")
  {
    // N5 dataType: plain type name, only for the types N5 supports.
    switch (dtype.id())
    {
      case DataTypeId::int8_t:
      case DataTypeId::int16_t:
      case DataTypeId::int32_t:
      case DataTypeId::int64_t:
      case DataTypeId::uint8_t:
      case DataTypeId::uint16_t:
      case DataTypeId::uint32_t:
      case DataTypeId::uint64_t:
      case DataTypeId::float32_t:
      case DataTypeId::float64_t: return std::string(dtype.name());
      default: return std::nullopt;
    }
  }
  switch (dtype.id())
  {
    case DataTypeId::bool_t: return "|b1";
    case DataTypeId::char_t: return "|S1";
    case DataTypeId::int8_t: return "|i1";
    case DataTypeId::int16_t: return "<i2";
    case DataTypeId::int32_t: return "<i4";
    case DataTypeId::int64_t: return "<i8";
    case DataTypeId::uint8_t: return "|u1";
    case DataTypeId::uint16_t: return "<u2";
    case DataTypeId::uint32_t: return "<u4";
    case DataTypeId::uint64_t: return "<u8";
    case DataTypeId::float16_t: return "<f2";
    case DataTypeId::float32_t: return "<f4";
    case DataTypeId::float64_t: return "<f8";
    case DataTypeId::complex64_t: return "<c8";
    case DataTypeId::complex128_t: return "<c16";
    default: return std::nullopt;
  }
}

std::string
zarr3_codec_names(const nlohmann::json &codecs)
{
  static const std::set<std::string> kStructural
      = { "bytes", "crc32c", "crc32c_no_separate_header", "transpose", "bitshuffle", "delta", "pcodec", "sharding_indexed" };
  std::vector<std::string> found;
  std::function<void(const nlohmann::json &)> walk = [&](const nlohmann::json &list)
  {
    if (!list.is_array()) return;
    for (const auto &codec : list)
    {
      if (!codec.is_object()) continue;
      const std::string name = codec.value("name", "");
      if (!name.empty() && !kStructural.count(name)) found.push_back(name);
      if (codec.contains("configuration") && codec["configuration"].is_object())
      {
        const auto &conf = codec["configuration"];
        for (const char *key : { "codecs", "index_codecs" })
        {
          if (conf.contains(key)) walk(conf[key]);
        }
      }
    }
  };
  walk(codecs);

  std::set<std::string> uniq(found.begin(), found.end());
  std::string out;
  for (const auto &c : uniq)
  {
    if (!out.empty()) out += "+";
    out += c;
  }
  return out;
}

// Basisname des Quell-Kompressors eines Arrays ("-" wenn keiner/unbekannt).
// Zarr 2: "id" des compressor-Objekts; Zarr 3: Codec-Namen nach
// zarr3_codec_names (rekursiv durch sharding_indexed).
template <typename T, tensorstore::DimensionIndex R>
std::string
array_compressor(const tensorstore::TensorStore<T, R> *store, int format)
{
  const std::string raw = read_store_key(store, (format == 3) ? "zarr.json" : ".zarray");
  if (raw.empty()) return "-";
  try
  {
    const nlohmann::json j = nlohmann::json::parse(raw);
    if (format == 3)
    {
      const std::string names = zarr3_codec_names(j.value("codecs", nlohmann::json::array()));
      if (!names.empty()) return names;
    }
    else if (j.contains("compressor") && j["compressor"].is_object())
    {
      const std::string id = j["compressor"].value("id", "");
      if (!id.empty()) return id;
    }
  }
  catch (...)
  {
  }
  return "-";
}

std::string
json_fill_value_text(const nlohmann::json &v)
{
  if (v.is_null()) return "-";
  if (v.is_string()) return v.get<std::string>();
  if (v.is_boolean()) return v.get<bool>() ? "true" : "false";
  if (v.is_number_integer()) return std::to_string(v.get<long long>());
  if (v.is_number_unsigned()) return std::to_string(v.get<unsigned long long>());
  if (v.is_number_float())
  {
    const double d = v.get<double>();
    if (std::isnan(d)) return "NaN";
    if (std::isinf(d)) return d > 0 ? "Infinity" : "-Infinity";
  }
  return v.dump();
}

// Quell-fill_value eines Arrays als Text ("-" wenn nicht gesetzt).  Gelesen
// wird der Roh-Wert aus `.zarray` / `zarr.json`, damit der Wert nicht durch
// den Datentyp normalisiert wird (wie beim Kompressor).
template <typename T, tensorstore::DimensionIndex R>
std::string
array_fill_value(const tensorstore::TensorStore<T, R> *store, int format)
{
  const std::string raw = read_store_key(store, (format == 3) ? "zarr.json" : ".zarray");
  if (raw.empty()) return "-";
  try
  {
    const nlohmann::json j = nlohmann::json::parse(sanitize_invalid_json(raw));
    if (j.contains("fill_value")) return json_fill_value_text(j["fill_value"]);
  }
  catch (...)
  {
  }
  return "-";
}

std::string
format_dim_list(const nlohmann::json &dims)
{
  std::string out = "[";
  bool first = true;
  if (dims.is_array())
  {
    for (const auto &d : dims)
    {
      if (!first) out += ",";
      first = false;
      out += std::to_string(d.get<std::int64_t>());
    }
  }
  out += "]";
  return out;
}

std::string
zarr3_meta_chunk_shape(const nlohmann::json &j)
{
  if (j.contains("codecs") && j["codecs"].is_array())
  {
    for (const auto &codec : j["codecs"])
    {
      if (codec.is_object() && codec.value("name", "") == "sharding_indexed" && codec.contains("configuration")
          && codec["configuration"].is_object() && codec["configuration"].contains("chunk_shape"))
      {
        return format_dim_list(codec["configuration"]["chunk_shape"]);
      }
    }
  }
  if (j.contains("chunk_grid") && j["chunk_grid"].is_object() && j["chunk_grid"].contains("configuration")
      && j["chunk_grid"]["configuration"].is_object() && j["chunk_grid"]["configuration"].contains("chunk_shape"))
  {
    return format_dim_list(j["chunk_grid"]["configuration"]["chunk_shape"]);
  }
  return "[-]";
}

std::string
zarr2_dtype_name(const std::string &dtype)
{
  static const std::map<char, std::string> kBase
      = { { 'f', "float" }, { 'i', "int" }, { 'u', "uint" }, { 'c', "complex" }, { 'b', "bool" }, { 'V', "void" } };
  if (dtype.size() < 2) return dtype;
  const auto it = kBase.find(dtype[1]);
  if (it == kBase.end()) return dtype;
  if (it->second == "bool") return "bool";
  const int bytes = std::atoi(dtype.c_str() + 2);
  if (bytes <= 0 || it->second == "void") return dtype;
  return it->second + std::to_string(bytes * 8);
}

std::vector<std::string>
list_array_names(const tsConfig &cfg, tensorstore::Context &context)
{
  using namespace tensorstore;

  std::string kvstore, path;
  std::optional<std::string> endpoint;
  resolve_kvstore(cfg, kvstore, path, endpoint);

  nlohmann::json group_spec;
  if (kvstore == "file") { group_spec = { { "driver", "file" }, { "path", cfg.path } }; }
  else if (kvstore == "http")
  {
    const std::size_t scheme = path.find("://");
    if (scheme == std::string::npos)
    {
      std::cerr << "http kvstore requires a full 'http://' or 'https://' URL as --path\n";
      return {};
    }
    const std::size_t slash = path.find('/', scheme + 3);
    const std::string base_url = path.substr(0, slash);
    const std::string remainder = (slash == std::string::npos) ? std::string() : path.substr(slash + 1);
    group_spec = { { "driver", "http" }, { "base_url", base_url }, { "path", "/" + remainder + "/" } };
  }
  else  // s3
  {
    const auto sl = path.find('/');
    const std::string bucket = (sl == std::string::npos) ? path : path.substr(0, sl);
    const std::string prefix = (sl == std::string::npos) ? "" : path.substr(sl + 1);
    group_spec = make_s3_kvstore(bucket, prefix + "/", endpoint);
  }

  auto group_store = kvstore::Open(group_spec, context).result();
  if (!group_store)
  {
    std::cerr << "Failed to open group kvstore: " << group_store.status().ToString() << "\n";
    return {};
  }

  std::set<std::string> names;

  // When consolidated metadata is requested (or available), the `.zmetadata`
  // keys enumerate every array / group in the store, which also works for
  // stores (e.g. http) that do not support listing.
  if (auto read_result = kvstore::Read(*group_store, ".zmetadata").result(); read_result)
  {
    if (read_result->has_value())
    {
      std::string raw;
      for (const absl::string_view chunk : read_result->value.Chunks()) raw.append(chunk.data(), chunk.size());
      try
      {
        const auto root = nlohmann::json::parse(raw);
        if (root.contains("metadata") && root["metadata"].is_object())
        {
          for (const auto &[key, _] : root["metadata"].items())
          {
            const std::size_t slash = key.find('/');
            if (slash == std::string::npos) continue;
            const std::string head = key.substr(0, slash);
            const std::string tail = key.substr(slash + 1);
            if (tail == ".zarray" || tail == "zarr.json") names.insert(head);
          }
        }
      }
      catch (...)
      {
      }
    }
  }
  if (!names.empty()) return { names.begin(), names.end() };

  auto entries = kvstore::ListFuture(*group_store).result();
  if (!entries)
  {
    std::cerr << "Failed to list group (and no usable '.zmetadata'): " << entries.status().ToString() << "\n";
    return {};
  }

  for (const auto &e : *entries)
  {
    std::string key = e.key;
    key.erase(0, key.find_first_not_of('/'));
    const std::size_t slash = key.find('/');
    if (slash == std::string::npos) continue;
    const std::string head = key.substr(0, slash);
    const std::string tail = key.substr(slash + 1);
    if (tail == ".zarray" || tail == "zarr.json") names.insert(head);
  }
  return { names.begin(), names.end() };
}
// Haengt `n` Elemente ab `src` elementweise als float32 an `out` an.  Der
// Lese-Cache hält den nativen Zarr-Datentyp (bfloat16/float16/float64/...);
// erst beim Ausschneiden in das Auswertungs-Feld wird nach float32
// konvertiert (bf16/f16 weiten exakt, f64->f32 ist verlustbehaftet, float32
// wird kopiert).  Dadurch hat der Cache denselben Datentyp wie die Quelle.
template <typename S>
void
append_as_float(const S *src, std::size_t n, float *out, std::size_t &pos)
{
  for (std::size_t i = 0; i < n; ++i) out[pos++] = static_cast<float>(src[i]);
}

struct TsEngine
{
  const AnyStoreType &store;
  const DimensionIndex rank;
  std::vector<Index> bs;  // clamped box start (0-based)
  std::vector<Index> bc;  // clamped box count
  std::vector<Index> chunk_shape;
  std::vector<Index> brange_lo, brange_hi;  // inclusive chunk-block range per trailing dim
  std::vector<Index> lb_n;                  // # layer blocks in the box per layer dim
  std::size_t nb_grid = 0;
  Index time_chunk = 0;
  bool layered = false;
  int fill_threads = 1;

  // Intersection block: one spatial chunk read only in the part that overlaps
  // the hypercube.  Sorted as layer-block combos (dims 1 .. rank-2, last
  // fastest) outer, grid blocks (dim rank-1) inner ascending - this keeps the
  // double accumulation order equal to a single contiguous pass over the
  // selected rows, and hence byte-identical to the Python reference.
  struct IBlock
  {
    std::vector<Index> off;  // absolute start per trailing dim (box intersection)
    std::vector<Index> ext;  // extent per trailing dim (box intersection)
    std::size_t seg = 1;     // per-timestep element count
  };
  std::vector<IBlock> ibs;
  std::size_t nblocks_total = 0;

  // One time-window (up to `time_chunk` timesteps) of decoded chunks.  Der
  // Cache haelt den nativen Quell-Datentyp (SharedArray<void> traegt den Typ
  // der gelesenen Werte); keine Vor-Konvertierung.
  struct WindowChunks
  {
    std::mutex mutex;
    std::vector<std::optional<SharedArray<void, dynamic_rank>>> chunks;
  };

  // Chunk-aligned time windows over [bs[0], bs[0] + bc[0]): a window never
  // spans two chunks, so a read returns exactly the timesteps of one chunk
  // that are needed (e.g. --nt 2 --t-start 4 crossing a chunk boundary yields
  // two 1-timestep windows and the cache holds one timestep per window).
  std::vector<std::pair<Index, Index>> windows;

  TsEngine(const AnyStoreType &s, const std::vector<Index> &box_start, const std::vector<Index> &box_count, int threads)
      : store(s), rank(static_cast<DimensionIndex>(s.domain().rank()))
  {
    const auto domain = store.domain();

    // Hypercube to process, clamped to the domain.
    bs.assign(static_cast<std::size_t>(rank), 0);
    bc.assign(static_cast<std::size_t>(rank), 0);
    for (DimensionIndex d = 0; d < rank; ++d)
    {
      bs[d] = std::max<Index>(0, std::min(box_start[d], domain.shape()[d] - 1));
      bc[d] = std::max<Index>(1, std::min(box_count[d], domain.shape()[d] - bs[d]));
    }
    layered = rank >= 3;

    // Physical (outer) chunk shape = shard shape for sharded Zarr 3
    // (write_chunk_shape); 0/-1 = unspecified -> full domain extent.
    chunk_shape.assign(static_cast<std::size_t>(rank), 0);
    for (DimensionIndex d = 0; d < rank; ++d) chunk_shape[d] = domain.shape()[d];
    if (auto chunk_layout = store.chunk_layout())
    {
      for (DimensionIndex d = 0; d < rank; ++d)
      {
        const Index s = chunk_layout->write_chunk_shape()[d];
        if (s > 0) chunk_shape[d] = s;
      }
    }
    time_chunk = chunk_shape[0];

    brange_lo.assign(static_cast<std::size_t>(rank), 0);
    brange_hi.assign(static_cast<std::size_t>(rank), 0);
    for (DimensionIndex d = 1; d < rank; ++d)
    {
      brange_lo[d] = bs[d] / chunk_shape[d];
      brange_hi[d] = (bs[d] + bc[d] - 1) / chunk_shape[d];
    }

    lb_n.assign(static_cast<std::size_t>(rank), 1);
    std::size_t lb_combos = 1;
    if (layered)
    {
      for (DimensionIndex d = 1; d <= rank - 2; ++d)
      {
        lb_n[d] = brange_hi[d] - brange_lo[d] + 1;
        lb_combos *= static_cast<std::size_t>(lb_n[d]);
      }
    }
    nb_grid = static_cast<std::size_t>(brange_hi[rank - 1] - brange_lo[rank - 1] + 1);
    for (std::size_t clc = 0; clc < lb_combos; ++clc)
    {
      std::vector<Index> cd(rank, 0);
      std::size_t rem = clc;
      for (DimensionIndex d = rank - 2; d >= 1; --d)
      {
        cd[d] = static_cast<Index>(rem % static_cast<std::size_t>(lb_n[d]));
        rem /= static_cast<std::size_t>(lb_n[d]);
      }
      for (std::size_t g = 0; g < nb_grid; ++g)
      {
        IBlock ib;
        ib.off.assign(static_cast<std::size_t>(rank - 1), 0);
        ib.ext.assign(static_cast<std::size_t>(rank - 1), 0);
        for (DimensionIndex d = 1; d <= rank - 2; ++d)
        {
          const Index o = (brange_lo[d] + cd[d]) * chunk_shape[d];
          ib.off[d - 1] = std::max(o, bs[d]);
          ib.ext[d - 1] = std::min(o + chunk_shape[d], bs[d] + bc[d]) - ib.off[d - 1];
        }
        {
          const DimensionIndex d = rank - 1;
          if (rank >= 2)
          {
            const Index o = (brange_lo[d] + static_cast<Index>(g)) * chunk_shape[d];
            ib.off[d - 1] = std::max(o, bs[d]);
            ib.ext[d - 1] = std::min(o + chunk_shape[d], bs[d] + bc[d]) - ib.off[d - 1];
          }
        }
        for (DimensionIndex d = 1; d < rank; ++d) ib.seg *= static_cast<std::size_t>(ib.ext[d - 1]);
        ibs.push_back(std::move(ib));
      }
    }
    nblocks_total = ibs.size();
    fill_threads = std::max(1, std::min(threads, static_cast<int>(nblocks_total)));

    // Chunk-aligned time windows over the selected range.
    {
      Index tcur = bs[0];
      Index remaining = bc[0];
      while (remaining > 0)
      {
        const Index chunk_base = (tcur / time_chunk) * time_chunk;
        const Index n = std::min(remaining, chunk_base + time_chunk - tcur);
        windows.emplace_back(tcur, n);
        tcur += n;
        remaining -= n;
      }
    }
  }

  // Fill `buf` with the chunks of window [t0, t0 + n) using at most
  // `fill_threads` concurrent reader threads.
  void
  fill(WindowChunks &buf, Index t0, Index n) const
  {
    using namespace tensorstore;
    buf.chunks.assign(nblocks_total, std::nullopt);
    std::atomic<std::size_t> next{ 0 };
    std::vector<std::thread> pool;
    pool.reserve(static_cast<std::size_t>(fill_threads));
    auto worker = [&]()
    {
      for (;;)
      {
        const std::size_t b = next.fetch_add(1);
        if (b >= nblocks_total) break;
        auto sliced = store | Dims(0).HalfOpenInterval(t0, t0 + n);
        for (DimensionIndex d = 1; d < rank; ++d)
        {
          const Index start = ibs[b].off[d - 1];
          const Index end = start + ibs[b].ext[d - 1];
          sliced = sliced | Dims(d).HalfOpenInterval(start, end);
        }
        auto r = Read<zero_origin>(sliced).result();
        std::lock_guard<std::mutex> lock(buf.mutex);
        buf.chunks[b].emplace(std::move(*r));
      }
    };
    for (int w = 0; w < fill_threads; ++w) pool.emplace_back(worker);
    for (auto &w : pool) w.join();
  }

  // Schneidet das Feld (Schritt `t_abs`, Layer `lay`) aus dem gefuellten
  // Fenster in `out` aus.  Reihenfolge und Anzahl der Werte entsprechen genau
  // der bisherigen Auswertung dieses Feldes.
  void
  extract(const WindowChunks &buf, Index win_t0, Index t_abs, Index lay, float *out, std::size_t n) const
  {
    using namespace tensorstore;
    const Index lt = t_abs - win_t0;
    std::size_t pos = 0;
    // Schneidet `n` Elemente ab Elementindex `off` des Chunks als float32 aus
    // (Laufzeit-Dispatch auf den nativen Cache-Datentyp).
    auto append = [&out, &pos](const SharedArray<void, dynamic_rank> &chunk, std::size_t off, std::size_t cnt)
    {
      const void *base = chunk.data();
      const DataType dt = chunk.dtype();
      if (dt == dtype_v<float>)
      {
        const float *p = static_cast<const float *>(base) + off;
        std::copy(p, p + static_cast<std::ptrdiff_t>(cnt), out + pos);
        pos += cnt;
      }
      else if (dt == dtype_v<double>)
        append_as_float(static_cast<const double *>(base) + off, cnt, out, pos);
      else if (dt == dtype_v<tensorstore::dtypes::float16_t>)
        append_as_float(static_cast<const tensorstore::dtypes::float16_t *>(base) + off, cnt, out, pos);
      else if (dt == dtype_v<tensorstore::dtypes::bfloat16_t>)
        append_as_float(static_cast<const tensorstore::dtypes::bfloat16_t *>(base) + off, cnt, out, pos);
      else
      {
        std::cerr << "Unsupported source data type for statistics: " << dt << "\n";
        std::fill_n(out + pos, cnt, 0.0f);
        pos += cnt;
      }
    };
    if (!layered)
    {
      // Rank-1: dim 0 is both the time and the (single) cell dim.  The field of
      // one step is a single value; the remaining `n - 1` slots are NaN so the
      // reported count/missing match the Python reference (count = extent along
      // dim 0, one valid value per step).
      if (rank == 1) std::fill_n(out, n, std::numeric_limits<float>::quiet_NaN());
      for (std::size_t b = 0; b < nblocks_total; ++b)
      {
        const SharedArray<void, dynamic_rank> &chunk = *buf.chunks[b];
        append(chunk, static_cast<std::size_t>(lt) * ibs[b].seg, ibs[b].seg);
      }
      return;
    }
    // Kombinierte Layer-Block-Koordinate -> Basis-Chunk dieses Layers.
    Index clc = 0;
    Index mult = 1;
    for (DimensionIndex d = rank - 2; d >= 1; --d)
    {
      const Index cd = lay / chunk_shape[d] - brange_lo[d];
      clc += cd * mult;
      mult *= lb_n[d];
    }
    const std::size_t base_ib = static_cast<std::size_t>(clc) * nb_grid;

    // `lo` = zeilenweise Position des Layers im (geschnittenen) Chunk.
    const IBlock &ib0 = ibs[base_ib];
    Index lo = lay - ib0.off[0];
    for (DimensionIndex d = 2; d <= rank - 2; ++d) lo = lo * ib0.ext[d - 1] + (lay - ib0.off[d - 1]);
    Index layer_seg = 1;
    for (DimensionIndex d = 1; d <= rank - 2; ++d) layer_seg *= ib0.ext[d - 1];

    for (std::size_t g = 0; g < nb_grid; ++g)
    {
      const std::size_t b = base_ib + g;
      const SharedArray<void, dynamic_rank> &chunk = *buf.chunks[b];
      const std::size_t egrid = ibs[b].ext[rank - 2];
      const std::size_t off
          = (static_cast<std::size_t>(lt) * static_cast<std::size_t>(layer_seg) + static_cast<std::size_t>(lo)) * egrid;
      append(chunk, off, egrid);
    }
  }
};  // ---------------------------------------------------------------------------
// Lesezustand eines Arrays: Engine (Auswahl + Zeitfenster) und zwei
// Fensterpuffer mit optionalem Prefetch des naechsten Fensters.  Die Felder
// werden daraus der Reihe nach ausgeschnitten; die Werte eines Feldes kommen in
// genau der Reihenfolge, in der sie bisher ausgewertet wurden.
// ---------------------------------------------------------------------------

struct Reader
{
  TsEngine eng;
  std::unique_ptr<TsEngine::WindowChunks> buf[2];
  int fwin = 0;     // Fenster, auf dem wir stehen
  int ready = -1;   // Fenster liegt fertig in buf[ready % 2]
  int launch = -1;  // Fenster, das der Prefetch-Thread fuellt
  std::thread filler;

  bool direct_read = false;  // direkt pro Feld lesen (kein Window-Cache)

  Reader(AnyStoreType &store, const std::vector<Index> &bstart, const std::vector<Index> &bcount, int threads, bool double_buffer)
      : eng(store, bstart, bcount, threads)
  {
    buf[0] = std::make_unique<TsEngine::WindowChunks>();
    if (double_buffer) buf[1] = std::make_unique<TsEngine::WindowChunks>();
    // Direkter Lesemodus: rank-2, kein Layering und Zeit-Chunk == 1.  Nur dann
    // entspricht das Zugriffsmuster (ein Schritt = eine Zellzeile) exakt der
    // Chunkgroesse, d.h. ein Read dekodiert genau einen Chunk ohne Redundanz.
    if (eng.rank == 2 && !eng.layered && eng.time_chunk == 1) direct_read = true;
  }

  ~Reader()
  {
    if (filler.joinable()) filler.join();
  }

  void
  fill(int w)
  {
    const int bi = buf[1] ? (w % 2) : 0;
    const auto &win = eng.windows[static_cast<std::size_t>(w)];
    eng.fill(*buf[bi], win.first, win.second);
  }

  // Stellt sicher, dass das Fenster mit Schritt `at` fertig in einem Puffer
  // liegt.
  void
  ensure(Index at)
  {
    if (direct_read) return;  // keine Fensterverwaltung im Direktmodus
    auto &ws = eng.windows;
    while (at >= ws[static_cast<std::size_t>(fwin)].first + ws[static_cast<std::size_t>(fwin)].second) ++fwin;
    if (ready == fwin) return;

    if (launch == fwin)
    {
      // Der Hintergrund-Thread hat dieses Fenster bereits gefuellt.
      filler.join();
      ready = fwin;
      launch = -1;
    }
    else
    {
      if (filler.joinable()) filler.join();
      fill(fwin);
      ready = fwin;
    }

    if (buf[1] && ready + 1 < static_cast<int>(ws.size()) && launch == -1)
    {
      launch = ready + 1;
      filler = std::thread([this, w = launch]() { fill(w); });
    }
  }

  void
  extract(Index t_abs, Index lay, float *out, std::size_t n)
  {
    if (direct_read && eng.rank == 2 && !eng.layered)
    {
      using namespace tensorstore;
      auto sliced = eng.store | tensorstore::Dims(0).ClosedInterval(t_abs, t_abs)
                    | tensorstore::Dims(1).HalfOpenInterval(eng.bs[1], eng.bs[1] + eng.bc[1]);
      auto r = Read<zero_origin>(sliced).result();
      if (!r)
      {
        std::cerr << "Read failed in direct mode\n";
        std::fill_n(out, n, 0.0f);
        return;
      }
      const void *base = r->data();
      const tensorstore::DataType dt = r->dtype();
      const std::size_t cnt = std::min<std::size_t>(static_cast<std::size_t>(r->num_elements()), n);
      std::size_t pos = 0;
      if (dt == dtype_v<float>)
      {
        const float *src = static_cast<const float *>(base);
        std::copy(src, src + static_cast<std::ptrdiff_t>(cnt), out);
      }
      else if (dt == dtype_v<double>)
        append_as_float(static_cast<const double *>(base), cnt, out, pos);
      else if (dt == dtype_v<tensorstore::dtypes::float16_t>)
        append_as_float(static_cast<const tensorstore::dtypes::float16_t *>(base), cnt, out, pos);
      else if (dt == dtype_v<tensorstore::dtypes::bfloat16_t>)
        append_as_float(static_cast<const tensorstore::dtypes::bfloat16_t *>(base), cnt, out, pos);
      else
      {
        std::cerr << "Unsupported source data type in direct read: " << dt << "\n";
        std::fill_n(out, n, 0.0f);
      }
      return;
    }
    eng.extract(*buf[buf[1] ? (fwin % 2) : 0], eng.windows[static_cast<std::size_t>(fwin)].first, t_abs, lay, out, n);
  }
};

// ---------------------------------------------------------------------------
// Schreibmodus: ein Ausgabe-Array mit dem Sammelpuffer des aktuellen
// Zeitfensters.  Die Felder werden Fenster fuer Fenster gesammelt und mit einem
// einzigen Write abgelegt; die Fenstergrenzen sind chunkweise wie beim Lesen.
// ---------------------------------------------------------------------------

struct Writer
{
  std::string name;
  tensorstore::TensorStore<void, dynamic_rank> store;
  std::vector<Index> bstart, bcount, chunks;
  bool static_array = false;  // zeitkonstant: der ganze Hypercube wird einmal geschrieben
  Index win_t0 = -1;  // erster Schritt des aktuellen Fensters
  Index win_n = 0;    // Schritte im aktuellen Fensters
  std::shared_ptr<SharedArray<float, dynamic_rank>> buf;
  Index filled = 0;  // bereits abgelegte Schritte in `buf`
};

// Schreibt `buf` (Form `shp`) an die Position `ori` des Ausgabestores, mit
// elementweiser Konvertierung fuer verlustbehaftete Ziele (float16/bfloat16).
bool
write_shared_block(Writer &w, const SharedArray<float, dynamic_rank> &buf, const std::vector<Index> &ori,
                   const std::vector<Index> &shp)
{
  using namespace tensorstore;
  const auto target = w.store | Box(ori, shp);
  const DataType dt = w.store.dtype();
  WriteFutures wr;
  if (dt == dtype_v<float>)
    wr = Write(buf, target);
  else if (dt == dtype_v<tensorstore::dtypes::float16_t>)
  {
    const auto conv = AllocateArray<tensorstore::dtypes::float16_t>(shp);
    const float *ip = buf.data();
    tensorstore::dtypes::float16_t *op = conv.data();
    const std::size_t n = conv.num_elements();
    for (std::size_t i = 0; i < n; ++i) op[i] = static_cast<tensorstore::dtypes::float16_t>(ip[i]);
    wr = Write(conv, target);
  }
  else if (dt == dtype_v<tensorstore::dtypes::bfloat16_t>)
  {
    const auto conv = AllocateArray<tensorstore::dtypes::bfloat16_t>(shp);
    const float *ip = buf.data();
    tensorstore::dtypes::bfloat16_t *op = conv.data();
    const std::size_t n = conv.num_elements();
    for (std::size_t i = 0; i < n; ++i) op[i] = static_cast<tensorstore::dtypes::bfloat16_t>(ip[i]);
    wr = Write(conv, target);
  }
  else
    wr = Write(buf, target);
  const auto st = wr.result();
  if (!st)
  {
    std::cerr << "Failed to write '" << w.name << "': " << st.status().message() << "\n";
    return false;
  }
  return true;
}

bool
flush_window(Writer &w)
{
  using namespace tensorstore;
  if (!w.buf || w.filled == 0) return true;
  const std::size_t rank = static_cast<std::size_t>(w.store.domain().rank());
  std::vector<Index> ori(rank, 0), shp = w.bcount;
  ori[0] = w.win_t0 - w.bstart[0];
  shp[0] = w.filled;
  if (!write_shared_block(w, *w.buf, ori, shp)) return false;
  w.buf.reset();
  w.filled = 0;
  return true;
}

// Legt den Sammelpuffer des Zeitfensters an, das `at` enthaelt (chunkweise
// Ausrichtung ueber die Auswahl, dieselben Fenster wie beim Lesen).
bool
start_window(Writer &w, Index at)
{
  using namespace tensorstore;
  const DimensionIndex rank = w.store.domain().rank();
  const Index time_chunk = w.chunks[0];
  const Index base = (at / time_chunk) * time_chunk;
  const Index t0 = std::max(w.bstart[0], base);
  const Index n = std::max<Index>(1, std::min(w.bstart[0] + w.bcount[0], base + time_chunk) - t0);
  std::vector<Index> region(static_cast<std::size_t>(rank));
  region[0] = n;
  for (DimensionIndex d = 1; d < rank; ++d) region[d] = w.bcount[static_cast<std::size_t>(d)];
  w.buf = std::make_shared<SharedArray<float, dynamic_rank>>(AllocateArray<float>(region, c_order, value_init));
  w.win_t0 = t0;
  w.win_n = n;
  w.filled = 0;
  return true;
}

// ---------------------------------------------------------------------------
// Alles, was fuer einen Datensatz (Lesen oder Schreiben) noetig ist.
// ---------------------------------------------------------------------------

void
make_context(const tsConfig &cfg, tensorstore::Context &context, std::optional<int> &concurrency)
{
  if (!concurrency)
  {
    // Der voreingestellte "unbounded"-Wert legt einen enormen Worker-Pool an
    // (hardware_concurrency * 16) und ueberlastet die Maschine, ~2x langsamer
    // als ein explizites Limit.  Default: Anzahl der Kerne.
    const unsigned int cores = std::thread::hardware_concurrency();
    concurrency = static_cast<int>(cores > 0 ? cores : 1);
  }
  nlohmann::json spec_json{ { "data_copy_concurrency", { { "limit", *concurrency } } } };
  // TensorStore-Caching ist deaktiviert (cache_pool fehlt: total_bytes_limit
  // ist dann 0, also kein LRU-Cache); die dekodierten Chunks werden statt
  // dessen im Reader zwischengespeichert.
  auto spec_result = tensorstore::Context::Spec::FromJson(std::move(spec_json));
  if (!spec_result)
  {
    std::cerr << "Invalid data_copy_concurrency context spec: " << spec_result.status() << "\n";
    return;
  }
  context = tensorstore::Context(spec_result.value());
}

// ---------------------------------------------------------------------------
// Uebersicht: eine Zeile pro Array.
// ---------------------------------------------------------------------------

// Alle String-Eintraege eines JSON-Arrays (nicht-Strings werden uebersprungen).
std::vector<std::string>
json_string_list(const nlohmann::json &j)
{
  std::vector<std::string> out;
  if (j.is_array())
    for (const auto &v : j)
      if (v.is_string()) out.push_back(v.get<std::string>());
  return out;
}

// Dimensionsnamen eines Array-Metadaten-Objekts: Zarr 3 `dimension_names`,
// sonst das `_ARRAY_DIMENSIONS`-Attribut (auch direkt fuer Zarr-2-`.zattrs`).
std::vector<std::string>
json_dim_names(const nlohmann::json &j)
{
  if (j.contains("dimension_names"))
  {
    std::vector<std::string> dims = json_string_list(j["dimension_names"]);
    if (!dims.empty()) return dims;
  }
  if (j.contains("attributes") && j["attributes"].is_object() && j["attributes"].contains("_ARRAY_DIMENSIONS"))
    return json_string_list(j["attributes"]["_ARRAY_DIMENSIONS"]);
  if (j.contains("_ARRAY_DIMENSIONS")) return json_string_list(j["_ARRAY_DIMENSIONS"]);
  return {};
}

// `grid_mapping`-Attribut eines Array-Metadaten-Objekts ("" wenn nicht gesetzt).
std::string
json_grid_mapping(const nlohmann::json &j)
{
  if (j.contains("attributes") && j["attributes"].is_object() && j["attributes"].contains("grid_mapping")
      && j["attributes"]["grid_mapping"].is_string())
  {
    return j["attributes"]["grid_mapping"].get<std::string>();
  }
  if (j.contains("grid_mapping") && j["grid_mapping"].is_string()) return j["grid_mapping"].get<std::string>();
  return {};
}

tsInfo
overview_line_from_zarr3(const std::string &name, const nlohmann::json &j)
{
  tsInfo row;
  row.name = name;
  row.data_type = j.value("data_type", "-");
  row.shape = j.contains("shape") ? format_dim_list(j["shape"]) : "[-]";
  row.chunks = zarr3_meta_chunk_shape(j);
  row.compressor = j.contains("codecs") ? zarr3_codec_names(j["codecs"]) : "";
  if (row.compressor.empty()) row.compressor = "-";
  row.units = "-";
  if (j.contains("units") && j["units"].is_string()) { row.units = j["units"].get<std::string>(); }
  else if (j.contains("attributes") && j["attributes"].is_object() && j["attributes"].contains("units")
           && j["attributes"]["units"].is_string())
  {
    row.units = j["attributes"]["units"].get<std::string>();
  }
  row.dim_names = json_dim_names(j);
  row.grid_mapping = json_grid_mapping(j);
  return row;
}

bool
overview_rows_from_consolidated(const std::map<std::string, std::string> &meta, const std::string &prefix,
                                const std::vector<std::string> &names, std::vector<tsInfo> &rows)
{
  bool ok = true;
  for (const auto &name : names)
  {
    const std::string key = prefix.empty() ? name : (prefix + "/" + name);
    const auto it = meta.find(key);
    if (it == meta.end())
    {
      std::cerr << "Failed to read metadata for array '" << name << "' (missing '" << key << "')\n";
      ok = false;
      continue;
    }
    try
    {
      rows.push_back(overview_line_from_zarr3(name, nlohmann::json::parse(it->second)));
    }
    catch (...)
    {
      std::cerr << "Failed to parse metadata for array '" << name << "'\n";
      ok = false;
    }
  }
  return ok;
}

bool
overview_rows_from_zarr2_consolidated(const std::map<std::string, std::string> &meta, const std::string &prefix,
                                      const std::vector<std::string> &names, std::vector<tsInfo> &rows)
{
  bool ok = true;
  for (const auto &name : names)
  {
    const std::string key = prefix.empty() ? name : (prefix + "/" + name);
    const auto za = meta.find(key + "/.zarray");
    if (za == meta.end())
    {
      std::cerr << "Failed to read metadata for array '" << name << "' (missing '" << key << "/.zarray')\n";
      ok = false;
      continue;
    }
    nlohmann::json z;
    try
    {
      z = nlohmann::json::parse(za->second);
    }
    catch (...)
    {
      std::cerr << "Failed to parse metadata for array '" << name << "'\n";
      ok = false;
      continue;
    }
    tsInfo row;
    row.name = name;
    row.shape = format_dim_list(z.value("shape", nlohmann::json::array()));
    row.chunks = format_dim_list(z.value("chunks", nlohmann::json::array()));
    row.data_type = zarr2_dtype_name(z.value("dtype", "<f8"));
    row.compressor = "-";
    if (z.contains("compressor") && z["compressor"].is_object() && z["compressor"].contains("id"))
    {
      row.compressor = z["compressor"]["id"].get<std::string>();
    }
    row.units = "-";
    const auto attrs = meta.find(key + "/.zattrs");
    if (attrs != meta.end())
    {
      try
      {
        const nlohmann::json a = nlohmann::json::parse(attrs->second);
        if (a.contains("units") && a["units"].is_string()) row.units = a["units"].get<std::string>();
        row.dim_names = json_dim_names(a);
        row.grid_mapping = json_grid_mapping(a);
      }
      catch (...)
      {
      }
    }
    rows.push_back(std::move(row));
  }
  return ok;
}

// Uebersicht direkt aus den Stores (ohne konsolidierte Metadaten).
bool
overview_rows_from_stores(const tsConfig &cfg, tensorstore::Context &context, const std::vector<std::string> &names,
                          std::vector<tsInfo> &rows)
{
  using namespace tensorstore;
  bool ok = true;
  for (const auto &name : names)
  {
    std::string kvstore;
    int fmt = 0;
    auto store = open_array_any(cfg, context, name, kvstore, fmt);
    if (!store)
    {
      ok = false;
      continue;
    }
    const auto domain = store->domain();

    tsInfo row;
    row.name = name;
    std::ostringstream shape_ss, chunk_ss;
    shape_ss << "[";
    for (DimensionIndex d = 0; d < domain.rank(); ++d)
    {
      if (d) shape_ss << ",";
      shape_ss << domain.shape()[d];
    }
    shape_ss << "]";
    chunk_ss << "[";
    if (const auto cl = store->chunk_layout())
    {
      const auto cshape = cl->read_chunk_shape();
      for (std::size_t d = 0; d < cshape.size(); ++d)
      {
        if (d) chunk_ss << ",";
        chunk_ss << cshape[d];
      }
    }
    chunk_ss << "]";
    row.shape = shape_ss.str();
    row.chunks = chunk_ss.str();
    row.data_type = std::string(store->dtype().name());

    row.compressor = "-";
    row.units = "-";
    const std::string meta = read_store_key(&*store, (fmt == 3) ? "zarr.json" : ".zarray");
    if (!meta.empty())
    {
      try
      {
        const nlohmann::json j = nlohmann::json::parse(meta);
        if (fmt == 3)
        {
          if (j.contains("codecs")) row.compressor = zarr3_codec_names(j["codecs"]);
          if (row.compressor.empty()) row.compressor = "-";
          if (j.contains("units") && j["units"].is_string()) { row.units = j["units"].get<std::string>(); }
          else if (j.contains("attributes") && j["attributes"].contains("units") && j["attributes"]["units"].is_string())
          {
            row.units = j["attributes"]["units"].get<std::string>();
          }
          row.dim_names = json_dim_names(j);
          row.grid_mapping = json_grid_mapping(j);
        }
        else if (j.contains("compressor") && j["compressor"].is_object() && j["compressor"].contains("id"))
        {
          row.compressor = j["compressor"]["id"].get<std::string>();
        }
      }
      catch (...)
      {
      }
    }
    if (fmt == 2)
    {
      const std::string attrs = read_store_key(&*store, ".zattrs");
      if (!attrs.empty())
      {
        try
        {
          const nlohmann::json j = nlohmann::json::parse(attrs);
          if (j.contains("units") && j["units"].is_string()) row.units = j["units"].get<std::string>();
          row.dim_names = json_dim_names(j);
          row.grid_mapping = json_grid_mapping(j);
        }
        catch (...)
        {
        }
      }
    }
    rows.push_back(std::move(row));
  }
  return ok;
}

}  // namespace

// TensorStore-spezifischer Teil des Handles; die TensorStore-freien Felder
// stehen in tsHandle (tsio.h).
struct tsHandleImpl : tsHandle
{
  tensorstore::Context context;

  // Quelle elementunabhaengig (Metadaten, Kompression, Koordinaten).
  std::vector<AnyStoreType> stores;
  std::vector<AnyStoreType> coord_stores;
  // Quelle dtype-neutral geoeffnet (float32 oder per to_float konvertiert).
  std::vector<AnyStoreType> fstores;
  std::vector<std::unique_ptr<Reader>> readers;
  // Zeitkonstante Arrays: der ganze ausgewaehlte Hypercube, einmal gelesen.
  std::vector<std::vector<float>> static_data;
  std::vector<char> static_loaded;
  std::vector<Writer> writers;
};

// ---------------------------------------------------------------------------
// Oeffentliche API
// ---------------------------------------------------------------------------

const char *
tsVersion()
{
  return TENSORSTORE_VERSION;
}

tsMetadata &
tsGetReadMetadata(tsHandle *handle)
{
  static tsMetadata empty;
  return handle ? handle->metadata : empty;
}

// Oeffnet einen Datensatz zum Lesen: alle ausgewaehlten Arrays element-
// unabhaengig (fuer Metadaten und Koordinatenerkennung) und die erkannten
// Koordinatenarrays.
tsHandle *
tsOpenFileRead(const std::string &path, const tsConfig &config)
{
  using namespace tensorstore;
  auto *h = new tsHandleImpl();
  h->cfg = config;
  h->cfg.path = path;
  make_context(h->cfg, h->context, h->cfg.data_copy_concurrency);
  if (!h->cfg.data_copy_concurrency)
  {
    delete h;
    return nullptr;
  }
  return h;
}

tsHandle *
tsApplyConfig(tsHandle *handle)
{
  tsHandleImpl *h = static_cast<tsHandleImpl *>(handle);
  using namespace tensorstore;

  bool debug_reported = false;  // das Format ist im ganzen Datensatz gleich: einmal melden
  std::sort(h->cfg.arrays.begin(), h->cfg.arrays.end());
  h->stores.reserve(h->cfg.arrays.size());
  h->metadata.arrayList.reserve(h->cfg.arrays.size());
  for (const auto &name : h->cfg.arrays)
  {
    std::string kvstore;
    int detected_format = 0;
    auto store = open_array_any(h->cfg, h->context, name, kvstore, detected_format);
    if (!store)
    {
      delete h;
      return nullptr;
    }
    tsArray a;
    a.name = name;
    a.kvstore = kvstore;
    a.format = detected_format;
    a.dataType = std::string(store->dtype().name());
    a.compressor = array_compressor(&*store, detected_format);
    a.fill_value = array_fill_value(&*store, detected_format);
    const auto domain = store->domain();
    a.shape.assign(domain.shape().begin(), domain.shape().end());
    a.dim_names = read_array_dim_names(&*store, detected_format);
    a.attributes = read_array_attrs(&*store, detected_format);
    a.time_constant = is_time_constant(a.dim_names);
    if (h->cfg.debug && !debug_reported)
    {
      std::cerr << "debug: zarr format " << detected_format << "\n";
      if (h->cfg.consolidated || h->cfg.group) { std::cerr << "debug: consolidated metadata\n"; }
      debug_reported = true;
    }
    h->stores.push_back(std::move(*store));
    h->metadata.arrayList.push_back(std::move(a));
  }

  // Koordinatenarrays: die Dimensionsnamen eines Datenarrays verweisen auf
  // Koordinaten; zusaetzlich kommt der `grid_mapping`-Wert (z. B. "crs")
  // hinzu.  Existiert ein Name nicht als Array, ist es keine Koordinate.
  std::set<std::string> coord_names;
  for (const auto &a : h->metadata.arrayList)
    for (const auto &d : a.dim_names) coord_names.insert(d);
  for (const auto &a : h->metadata.arrayList)
  {
    const auto it = a.attributes.find("grid_mapping");
    if (it != a.attributes.end()) coord_names.insert(it->second);
  }
  for (const auto &cname : coord_names)
  {
    std::string kvstore;
    int detected_format = 0;
    auto store = open_array_any(h->cfg, h->context, cname, kvstore, detected_format, /*quiet=*/true);
    if (!store) continue;
    tsArray c;
    c.name = cname;
    c.kvstore = kvstore;
    c.format = detected_format;
    c.dataType = coord_dtype_encoded(store->dtype(), (detected_format == 3) ? "zarr3" : "zarr2").value_or("-");
    const auto domain = store->domain();
    c.shape.assign(domain.shape().begin(), domain.shape().end());
    c.dim_names = read_array_dim_names(&*store, detected_format);
    c.attributes = read_array_attrs(&*store, detected_format);
    h->coord_stores.push_back(std::move(*store));
    h->metadata.coordList.push_back(std::move(c));
  }
  return h;
}

// Loest die Auswahl (--step/--layer/--cell) eines Arrays auf und prueft sie
// gegen die Ausdehnung.  Fuellt `bstart`, `bcount`, `chunks` und `out_chunks`.
bool
tsResolveArray(tsHandle *handle, std::size_t index)
{
  using namespace tensorstore;
  if (!handle || index >= handle->metadata.arrayList.size()) return false;
  tsArray &a = handle->metadata.arrayList[index];
  const DimensionIndex rank = static_cast<DimensionIndex>(a.shape.size());
  std::vector<Index> shape(a.shape.begin(), a.shape.end());

  std::vector<Index> bstart, bcount;
  if (a.time_constant)
    resolve_selection_static(handle->cfg, shape, bstart, bcount);
  else
    resolve_selection(handle->cfg, shape, bstart, bcount);
  for (DimensionIndex d = 0; d < rank; ++d)
  {
    if (bstart[d] >= shape[d])
    {
      std::cerr << "Start index beyond array extent (dim " << d << ")\n";
      return false;
    }
  }

  a.bstart.assign(static_cast<std::size_t>(rank), 0);
  a.bcount.assign(static_cast<std::size_t>(rank), 1);
  for (DimensionIndex d = 0; d < rank; ++d)
  {
    a.bstart[d] = std::max<Index>(0, std::min(bstart[d], shape[d] - 1));
    a.bcount[d] = std::max<Index>(1, std::min(bcount[d], shape[d] - a.bstart[d]));
  }

  // Physische (aeussere) Chunkform = Shardform bei geshardetem Zarr 3
  // (write_chunk_shape); 0/-1 = unspezifiziert -> volle Ausdehnung.
  a.chunks.assign(shape.begin(), shape.end());
  if (auto cl = static_cast<tsHandleImpl *>(handle)->stores[index].chunk_layout())
  {
    for (DimensionIndex d = 0; d < rank; ++d)
    {
      const Index s = cl->write_chunk_shape()[d];
      if (s > 0) a.chunks[d] = s;
    }
  }

  // Die Ausgabe-Chunkform (Quellform auf die Auswahl begrenzt, genau so werden
  // die Ausgabe-Stores angelegt).  --step-chunk/--cell-chunk ueberschreiben die
  // Laenge der step- (Dim 0) bzw. cell- (letzte Dim) Chunks, auf die Auswahl
  // geklemmt.
  a.out_chunks.assign(static_cast<std::size_t>(rank), 1);
  for (DimensionIndex d = 0; d < rank; ++d) { a.out_chunks[d] = std::max<Index>(1, std::min(a.chunks[d], a.bcount[d])); }
  if (handle->cfg.step_chunk && rank > 0 && !a.time_constant)
  {
    a.out_chunks[0] = std::max<Index>(1, std::min<Index>(*handle->cfg.step_chunk, a.bcount[0]));
  }
  if (handle->cfg.cell_chunk && rank > 0)
  {
    a.out_chunks[static_cast<std::size_t>(rank) - 1]
        = std::max<Index>(1, std::min<Index>(*handle->cfg.cell_chunk, a.bcount[static_cast<std::size_t>(rank) - 1]));
  }

  a.num_values = 1;
  for (DimensionIndex d = 0; d < rank; ++d) a.num_values *= a.bcount[d];
  a.num_cells = a.bcount[static_cast<std::size_t>(rank) - 1];
  if (a.time_constant)
  {
    // Keine Zeitdimension: genau ein (impliziter) Zeitschritt, die erste
    // Dimension ist die Layer-Dimension.
    a.num_steps = 1;
    a.layered = rank >= 2;
    a.num_layers = rank >= 2 ? a.num_values / a.num_cells : 1;
  }
  else
  {
    a.num_steps = a.bcount[0];
    a.layered = rank >= 3;
    a.num_layers = rank >= 3 ? a.bcount[1] : 1;
  }
  return true;
}

// Oeffnet alle ausgewaehlten Datenarrays als float32 und legt die Leser an.
bool
tsOpenFields(tsHandle *handle)
{
  using namespace tensorstore;
  if (!handle || handle->fields_open) return false;
  auto *h = static_cast<tsHandleImpl *>(handle);
  const std::size_t n = handle->metadata.arrayList.size();
  h->fstores.reserve(n);
  h->readers.reserve(n);
  h->static_data.resize(n);
  h->static_loaded.assign(n, 0);
  for (std::size_t i = 0; i < n; ++i)
  {
    tsArray &a = handle->metadata.arrayList[i];
    if (a.bcount.size() != a.shape.size())
    {
      std::cerr << "Array '" << a.name << "' has not been resolved\n";
      return false;
    }
    std::string kvstore = a.kvstore;
    int detected_format = a.format;
    auto store = open_array_any(handle->cfg, h->context, a.name, kvstore, detected_format);
    if (!store) return false;
    h->fstores.push_back(std::move(*store));
    if (a.time_constant)
    {
      // Zeitkonstante Arrays werden als Feld des ersten Schritts geliefert;
      // gelesen wird der ganze Hypercube einmal in tsReadField.
      h->readers.push_back(nullptr);
      continue;
    }
    h->readers.push_back(std::make_unique<Reader>(h->fstores.back(), a.bstart, a.bcount, *handle->cfg.data_copy_concurrency,
                                                  handle->cfg.double_buffer));
  }
  handle->metadata.num_steps = 0;
  handle->start_t = std::numeric_limits<std::int64_t>::max();
  bool have_time_array = false;
  bool have_static_array = false;
  for (const auto &a : handle->metadata.arrayList)
  {
    if (a.time_constant)
    {
      have_static_array = true;
      continue;
    }
    have_time_array = true;
    handle->metadata.num_steps = std::max(handle->metadata.num_steps, a.bstart[0] + a.num_steps);
    handle->start_t = std::min(handle->start_t, a.bstart[0]);
  }
  if (!have_time_array)
  {
    // Nur zeitkonstante Arrays: genau ein (impliziter) Zeitschritt.
    handle->start_t = 0;
    if (have_static_array) handle->metadata.num_steps = 1;
  }
  // Feldliste: zuerst die zeitlich varianten Arrays (in jedem Schritt), danach
  // die zeitlich konstanten (nur im ersten Schritt).  Dadurch haben die
  // varianten Arrays in allen Schritten denselben Feldindex.
  handle->fields.clear();
  for (std::size_t i = 0; i < handle->metadata.arrayList.size(); ++i)
  {
    const tsArray &a = handle->metadata.arrayList[i];
    if (a.time_constant) continue;
    const std::int64_t nlay = a.num_layers;
    for (std::int64_t l = 0; l < nlay; ++l)
    {
      const std::int64_t lay = a.bcount.size() >= 3 ? a.bstart[1] + l : 0;
      handle->fields.emplace_back(i, lay);
    }
  }
  handle->dynamic_field_count = handle->fields.size();
  for (std::size_t i = 0; i < handle->metadata.arrayList.size(); ++i)
  {
    const tsArray &a = handle->metadata.arrayList[i];
    if (!a.time_constant) continue;
    const std::int64_t nlay = a.num_layers;
    for (std::int64_t l = 0; l < nlay; ++l)
    {
      const std::int64_t lay = a.layered ? a.bstart[0] + l : 0;
      handle->fields.emplace_back(i, lay);
    }
  }
  handle->cur_t = handle->start_t - 1;
  handle->next_field = 0;
  handle->have_field = false;
  handle->done = false;
  handle->fields_open = true;
  return true;
}

// Anzahl der Felder des aktuellen Zeitschritts: im ersten Schritt auch die
// zeitlich konstanten Arrays, sonst nur die zeitlich varianten.
std::size_t
tsGetNumFields(tsHandle *handle)
{
  if (!handle || !handle->fields_open) return 0;
  if (handle->cur_t == handle->start_t) return handle->fields.size();
  return handle->dynamic_field_count;
}

// Rueckt auf den naechsten Zeitschritt vor und gibt ihn zurueck (absolute
// Nummer); -1 am Ende des Datensatzes.
std::int64_t
tsGetNextStep(tsHandle *handle)
{
  if (!handle || !handle->fields_open) return -1;
  if (handle->cur_t < handle->start_t)
    handle->cur_t = handle->start_t;
  else
    ++handle->cur_t;
  if (handle->cur_t >= handle->metadata.num_steps)
  {
    handle->done = true;
    return -1;
  }
  handle->next_field = 0;
  handle->have_field = false;
  return handle->cur_t;
}

// Liefert das naechste Feld des aktuellen Schritts: Array-Liste aufsteigend,
// darin Layer aufsteigend.  Arrays ohne Daten beim aktuellen Schritt werden
// uebersprungen (ungleich lange Zeitraeume).
bool
tsGetField(tsHandle *handle, tsField &field)
{
  if (!handle || !handle->fields_open || handle->done) return false;
  const auto &arrays = handle->metadata.arrayList;
  while (handle->next_field < handle->fields.size())
  {
    const auto [j, lay] = handle->fields[handle->next_field++];
    const tsArray &a = arrays[j];
    if (a.time_constant)
    {
      // Zeitkonstant: nur im ersten Schritt vorhanden.
      if (handle->cur_t != handle->start_t) continue;
    }
    else if (handle->cur_t < a.bstart[0] || handle->cur_t >= a.bstart[0] + a.num_steps)
      continue;
    field.array = j;
    field.layer = lay;
    handle->last_field = field;
    handle->have_field = true;
    return true;
  }
  return false;
}

// Liest den gesamten ausgewaehlten Hypercube eines zeitkonstanten Arrays in
// `out` (Laenge num_values, C-Reihenfolge).
static bool
read_static_hypercube(tsHandleImpl *h, std::size_t index, std::vector<float> &out)
{
  using namespace tensorstore;
  const tsArray &a = h->metadata.arrayList[index];
  if (index >= h->fstores.size()) return false;
  const AnyStoreType &store = h->fstores[index];
  auto read = Read<zero_origin>(store | Box(a.bstart, a.bcount)).result();
  if (!read)
  {
    std::cerr << "Failed to read '" << a.name << "': " << read.status().ToString() << "\n";
    return false;
  }
  const void *base = read->data();
  const DataType dt = read->dtype();
  const std::size_t cnt = static_cast<std::size_t>(read->num_elements());
  out.assign(cnt, 0.0f);
  std::size_t pos = 0;
  if (dt == dtype_v<float>)
  {
    const float *src = static_cast<const float *>(base);
    std::copy(src, src + static_cast<std::ptrdiff_t>(cnt), out.data());
  }
  else if (dt == dtype_v<double>)
    append_as_float(static_cast<const double *>(base), cnt, out.data(), pos);
  else if (dt == dtype_v<tensorstore::dtypes::float16_t>)
    append_as_float(static_cast<const tensorstore::dtypes::float16_t *>(base), cnt, out.data(), pos);
  else if (dt == dtype_v<tensorstore::dtypes::bfloat16_t>)
    append_as_float(static_cast<const tensorstore::dtypes::bfloat16_t *>(base), cnt, out.data(), pos);
  else
  {
    std::cerr << "Unsupported source data type for statistics: " << dt << "\n";
    std::fill_n(out.data(), cnt, 0.0f);
  }
  return true;
}

// Liest das zuletzt von tsGetField gelieferte Feld; der Schritt ist der
// aktuelle Schritt von tsGetNextStep.
bool
tsReadField(tsHandle *handle, float *data, std::size_t n)
{
  if (!handle || !handle->fields_open || !handle->have_field) return false;
  const tsField &field = handle->last_field;
  auto *h = static_cast<tsHandleImpl *>(handle);
  const tsArray &a = handle->metadata.arrayList[field.array];
  if (a.time_constant)
  {
    // Der ganze Hypercube wird beim ersten Feld einmal gelesen; ein Feld ist
    // ein Layer (rank 1: der ganze Hypercube).
    if (field.array >= h->static_data.size()) return false;
    std::vector<float> &buf = h->static_data[field.array];
    if (!h->static_loaded[field.array])
    {
      if (!read_static_hypercube(h, field.array, buf)) return false;
      h->static_loaded[field.array] = 1;
    }
    const std::int64_t r = a.layered ? field.layer - a.bstart[0] : 0;
    if (r < 0 || a.num_cells <= 0) return false;
    const std::size_t off = static_cast<std::size_t>(r) * static_cast<std::size_t>(a.num_cells);
    if (off + n > buf.size()) return false;
    std::copy_n(buf.data() + off, n, data);
    return true;
  }
  if (field.array >= h->readers.size() || !h->readers[field.array]) return false;
  Reader &r = *h->readers[field.array];
  r.ensure(handle->cur_t);
  r.extract(handle->cur_t, field.layer, data, n);
  return true;
}

// Bereitet den Schreibmodus vor: ein Ausgabe-Verzeichnis pro ausgewaehltem
// Datenarray.  Die Stores erzeugt `tsSetMetadata`.
tsHandle *
tsOpenFileWrite(const std::string &path, const tsConfig &config)
{
  tsHandleImpl *h = new tsHandleImpl();
  h->cfg = config;
  h->cfg.path = path;
  h->cfg.arrays = config.arrays;
  std::sort(h->cfg.arrays.begin(), h->cfg.arrays.end());
  make_context(h->cfg, h->context, h->cfg.data_copy_concurrency);
  if (!h->cfg.data_copy_concurrency)
  {
    delete h;
    return nullptr;
  }
  for (const auto &name : h->cfg.arrays) h->out_dirs.push_back(path + "/" + name + "/");
  return h;
}

// Elementgroesse (in Bytes) des --dtype-Werts fuer die Datenarrays: 2 fuer
// float16/bfloat16, 4 fuer float32, 8 fuer float64.
int
out_dtype_size(const std::string &out_dtype)
{
  if (out_dtype == "float16" || out_dtype == "bfloat16") return 2;
  if (out_dtype == "float64") return 8;
  return 4;
}

// Oeffnet einen Ausgabe-Store mit einem festen Elementtyp und entkleidet ihn
// zu einem dtype-neutralen Handle (die Schreibschleife haelt float-Puffer, das
// Schreiben konvertiert elementweise).
template <typename T>
tensorstore::Result<tensorstore::TensorStore<void, tensorstore::dynamic_rank>>
open_output_store(const nlohmann::json &spec, tensorstore::Context &ctx)
{
  auto st
      = tensorstore::Open<T, tensorstore::dynamic_rank>(spec, ctx, tensorstore::OpenMode::create, tensorstore::ReadWriteMode::write)
            .result();
  if (!st) return st.status();
  return tensorstore::TensorStore<void, tensorstore::dynamic_rank>(*st);
}

// Liest Attribute, dimension_names und die Roh-Kompressionsmetadaten aller
// Datenarrays in `meta` ein; unabhaengig vom Output-Config.
bool
tsGetMetadata(tsHandle *input, tsOutputMetadata &meta)
{
  using namespace tensorstore;
  if (!input) return false;
  auto *in = static_cast<tsHandleImpl *>(input);
  if (in->fstores.size() != input->metadata.arrayList.size()) return false;  // tsOpenFields fehlt
  meta = tsOutputMetadata();
  meta.start_t = input->start_t;
  meta.num_steps = input->metadata.num_steps;
  meta.arrays.reserve(input->metadata.arrayList.size());
  for (std::size_t i = 0; i < input->metadata.arrayList.size(); ++i)
  {
    const tsArray &a = input->metadata.arrayList[i];
    nlohmann::json src_attrs, src_dims;
    read_source_metadata(&in->fstores[i], a.format, src_attrs, src_dims);
    const std::string raw = read_store_key(&in->fstores[i], (a.format == 3) ? "zarr.json" : ".zarray");

    tsArrayMeta m;
    m.name = a.name;
    m.format = a.format;
    m.source_metadata = raw;
    m.attributes = src_attrs.dump();
    m.dim_names = src_dims.dump();
    m.bstart = a.bstart;
    m.bcount = a.bcount;
    m.out_chunks = a.out_chunks;
    m.static_array = a.time_constant;
    meta.arrays.push_back(std::move(m));
  }
  return true;
}

// Wendet das Ausgabe-Config auf die gelesenen Metadaten an und baut je Array
// driver + Store-Metadaten.
void
tsApplyMetadataConfig(const tsConfig &cfg, tsOutputMetadata &meta)
{
  using namespace tensorstore;
  for (tsArrayMeta &m : meta.arrays)
  {
    const std::vector<Index> shape = m.bcount;
    const std::vector<Index> chunks = m.out_chunks;

    nlohmann::json src_attrs = nlohmann::json::object(), src_dims = nlohmann::json::array();
    try
    {
      src_attrs = nlohmann::json::parse(m.attributes);
      src_dims = nlohmann::json::parse(m.dim_names);
    }
    catch (...)
    {
      src_attrs = nlohmann::json::object();
      src_dims = nlohmann::json::array();
    }

    nlohmann::json md;
    if (cfg.out_format == "zarr3")
    {
      md = { { "zarr_format", 3 },
             { "node_type", "array" },
             { "shape", shape },
             { "chunk_grid", { { "name", "regular" }, { "configuration", { { "chunk_shape", chunks } } } } },
             { "data_type", cfg.out_dtype },
             { "fill_value", 0.0 } };
      if (!src_attrs.empty()) md["attributes"] = src_attrs;
      if (!src_dims.empty()) md["dimension_names"] = src_dims;
    }
    else if (cfg.out_format == "n5")
    {
      // N5: mandatory attributes plus the source attributes (with
      // `_ARRAY_DIMENSIONS`) as additional attributes in attributes.json.
      md = { { "dimensions", shape }, { "blockSize", chunks }, { "dataType", "float32" } };
      {
        const nlohmann::json extra = n5_extra_attrs(src_attrs, src_dims);
        for (auto it = extra.begin(); it != extra.end(); ++it) md[it.key()] = it.value();
      }
    }
    else
    {
      md = { { "zarr_format", 2 }, { "shape", shape },        { "chunks", chunks },
             { "dtype", "<f4" },   { "compressor", nullptr }, { "fill_value", 0.0 },
             { "order", "C" },     { "filters", nullptr },    { "dimension_separator", "." } };
    }

    if (cfg.compressor) { apply_compressor_by_name(*cfg.compressor, cfg.out_format, out_dtype_size(cfg.out_dtype), md); }
    else
    {
      nlohmann::json src;
      try
      {
        src = nlohmann::json::parse(m.source_metadata);
      }
      catch (...)
      {
        src = nlohmann::json();
      }
      apply_source_compression_json(src, m.format, cfg.out_format, out_dtype_size(cfg.out_dtype), chunks, md);
    }

    m.driver = (cfg.out_format == "zarr3") ? "zarr3" : (cfg.out_format == "n5") ? "n5" : "zarr";
    m.metadata = md.dump();
  }
}

// Legt je Datenarray den Ausgabestore an und setzt die Schreibgrenzen; benoetigt
// nur das Output-Handle.
bool
tsSetMetadata(tsHandle *output, const tsOutputMetadata &meta)
{
  using namespace tensorstore;
  if (!output) return false;
  auto *out = static_cast<tsHandleImpl *>(output);
  if (out->out_dirs.size() < meta.arrays.size()) return false;
  for (std::size_t i = 0; i < meta.arrays.size(); ++i)
  {
    const tsArrayMeta &m = meta.arrays[i];
    const std::string dir = out->out_dirs[i];

    nlohmann::json md;
    try
    {
      md = nlohmann::json::parse(m.metadata);
    }
    catch (...)
    {
      std::cerr << "Invalid metadata for '" << m.name << "'\n";
      return false;
    }

    nlohmann::json spec{ { "driver", m.driver },
                         { "kvstore", { { "driver", "file" }, { "path", dir } } },
                         { "create", true },
                         { "delete_existing", true },
                         { "metadata", md } };
    tensorstore::Result<tensorstore::TensorStore<void, tensorstore::dynamic_rank>> store_result;
    const bool z3 = (m.driver == "zarr3");
    if (z3 && output->cfg.out_dtype == "float16")
      store_result = open_output_store<tensorstore::dtypes::float16_t>(spec, out->context);
    else if (z3 && output->cfg.out_dtype == "float64")
      store_result = open_output_store<double>(spec, out->context);
    else if (z3 && output->cfg.out_dtype == "bfloat16")
      store_result = open_output_store<tensorstore::dtypes::bfloat16_t>(spec, out->context);
    else
      store_result = open_output_store<float>(spec, out->context);
    if (!store_result)
    {
      std::cerr << "Failed to create output store for '" << m.name << "': " << store_result.status().ToString() << "\n";
      return false;
    }
    auto store = *store_result;
    if (m.driver == "zarr")
    {
      nlohmann::json src_attrs = nlohmann::json::object(), src_dims = nlohmann::json::array();
      try
      {
        src_attrs = nlohmann::json::parse(m.attributes);
        src_dims = nlohmann::json::parse(m.dim_names);
      }
      catch (...)
      {
        src_attrs = nlohmann::json::object();
        src_dims = nlohmann::json::array();
      }
      if (!write_v2_zattrs_if_needed(dir, src_attrs, src_dims)) return false;
    }

    Writer w;
    w.name = m.name;
    w.store = std::move(store);
    w.bstart = m.bstart;
    w.bcount = m.bcount;
    w.chunks = m.out_chunks;
    w.static_array = m.static_array;
    out->writers.push_back(std::move(w));
  }
  out->out_start = meta.start_t;
  out->out_t = meta.start_t - 1;
  out->out_end = meta.num_steps;
  out->have_sel = false;
  return true;
}

// Zeitschritt-Zaehler der Ausgabe: rueckt auf den naechsten absoluten
// Schreib-Schritt vor und gibt ihn zurueck (-1 am Ende).
std::int64_t
tsSetNextStep(tsHandle *handle)
{
  if (!handle || static_cast<tsHandleImpl *>(handle)->writers.empty() || handle->out_t >= handle->out_end - 1) return -1;
  ++handle->out_t;
  return handle->out_t;
}

// Merkt sich das Schreibziel des naechsten tsWriteField-Aufrufs.
bool
tsSetField(tsHandle *handle, const tsField &field)
{
  if (!handle || field.array >= static_cast<tsHandleImpl *>(handle)->writers.size()) return false;
  handle->sel_field = field.array;
  handle->sel_layer = field.layer;
  handle->have_sel = true;
  return true;
}

// Schreibt ein Feld in die Ausgabe (Fenstersammelpuffer, chunkweises Schreiben).
bool
tsWriteField(tsHandle *handle, const float *data, std::size_t n)
{
  using namespace tensorstore;
  if (!handle || !handle->have_sel || handle->sel_field >= static_cast<tsHandleImpl *>(handle)->writers.size()) return false;
  Writer &w = static_cast<tsHandleImpl *>(handle)->writers[handle->sel_field];
  const Index step = handle->out_t;
  const DimensionIndex rank = w.store.domain().rank();
  if (w.static_array)
  {
    // Zeitkonstant: ein Feld ist ein Layer des ausgewaehlten Hypercubes
    // (rank 1: der ganze Hypercube), der direkt geschrieben wird.
    const std::size_t rk = w.bcount.size();
    const Index cells = w.bcount[rk - 1];
    if (static_cast<Index>(n) != cells) return false;
    std::vector<Index> ori(rk, 0), shp(rk, 1);
    shp[rk - 1] = cells;
    if (rk >= 2)
    {
      Index flat = handle->sel_layer - w.bstart[0];
      if (flat < 0) return false;
      for (std::size_t d = rk - 1; d-- > 0;)
      {
        ori[d] = flat % w.bcount[d];
        flat /= w.bcount[d];
      }
      if (flat != 0) return false;
    }
    const auto arr = AllocateArray<float>(shp, c_order, value_init);
    std::copy(data, data + n, arr.data());
    return write_shared_block(w, arr, ori, shp);
  }
  if (rank == 1)
  {
    // Rank-1: one value per step (dim 0 is both the time and the cell dim).
    // `data` comes from the reader padded to `num_cells` (= step count); only
    // its first entry holds the value of this step.
    if (n < 1) return false;
    if (!w.buf || step >= w.win_t0 + w.win_n)
    {
      if (!flush_window(w)) return false;
      if (!start_window(w, step)) return false;
    }
    w.buf->data()[step - w.win_t0] = data[0];
    w.filled = std::max(w.filled, step - w.win_t0 + 1);
    return true;
  }
  const Index lay = rank >= 3 ? handle->sel_layer - w.bstart[1] : 0;
  const Index cells = w.bcount.back();
  if (static_cast<Index>(n) != cells)
  {
    std::cerr << "Field '" << w.name << "' has " << n << " values, expected " << cells << "\n";
    return false;
  }
  if (!w.buf || step >= w.win_t0 + w.win_n)
  {
    if (!flush_window(w)) return false;
    if (!start_window(w, step)) return false;
  }
  const Index layers = rank >= 3 ? w.bcount[1] : 1;
  if (lay < 0 || lay >= layers) return false;
  float *dst = w.buf->data() + ((step - w.win_t0) * layers + lay) * static_cast<std::size_t>(cells);
  std::copy(data, data + n, dst);
  w.filled = std::max(w.filled, step - w.win_t0 + 1);
  return true;
}

// Liest die Quell-Metadaten aller benutzten Koordinatenarrays in `coords` ein
// (nativer Datentyp, Roh-Kompression); unabhaengig vom Output-Config.
bool
tsReadCoordinates(tsHandle *input, std::vector<tsCoordinate> &coords)
{
  using namespace tensorstore;
  if (!input) return false;
  auto *in = static_cast<tsHandleImpl *>(input);
  coords.clear();
  coords.reserve(input->metadata.coordList.size());
  for (std::size_t i = 0; i < input->metadata.coordList.size(); ++i)
  {
    const tsArray &c = input->metadata.coordList[i];
    const AnyStoreType &store = in->coord_stores[i];
    const auto domain = store.domain();

    nlohmann::json src_attrs, src_dims;
    read_source_metadata(&store, c.format, src_attrs, src_dims);
    const std::string raw = read_store_key(&store, (c.format == 3) ? "zarr.json" : ".zarray");

    tsCoordinate coord;
    coord.name = c.name;
    coord.format = c.format;
    coord.source_metadata = raw;
    coord.attributes = src_attrs.dump();
    coord.dim_names = src_dims.dump();
    coord.shape.assign(domain.shape().begin(), domain.shape().end());
    coord.source = &store;
    coords.push_back(std::move(coord));
  }
  return true;
}

// Wendet das Ausgabe-Config auf die gelesenen Koordinaten an (Auswahl,
// Zielpfad, Format, Chunk-Overrides und Kompression) und materialisiert die
// ausgewaehlten Werte.
bool
tsApplyCoordinateConfig(const tsConfig &cfg, std::vector<tsCoordinate> &coords)
{
  using namespace tensorstore;
  for (tsCoordinate &coord : coords)
  {
    const std::string &cname = coord.name;
    const AnyStoreType &store = *static_cast<const AnyStoreType *>(coord.source);
    const DimensionIndex rank = static_cast<DimensionIndex>(coord.shape.size());

    nlohmann::json dims_json = nlohmann::json::array();
    try
    {
      dims_json = nlohmann::json::parse(coord.dim_names);
    }
    catch (...)
    {
      dims_json = nlohmann::json::array();
    }
    const std::vector<std::string> cdim_names = json_string_list(dims_json);
    const bool static_coord = is_time_constant(cdim_names);

    std::vector<Index> bstart, bcount;
    if (static_coord)
      resolve_selection_static(cfg, coord.shape, bstart, bcount);
    else
      resolve_selection(cfg, coord.shape, bstart, bcount);
    if (static_coord && rank == 1 && (cdim_names.size() != 1 || cdim_names[0] != "cell"))
    {
      // Zeitkonstante Rang-1-Koordinate, deren einzige Dimension nicht die
      // Zellachse ist (z. B. crs/ocean_model_level): keine Auswahl anwenden.
      bstart[0] = 0;
      bcount[0] = coord.shape[0];
    }
    for (DimensionIndex d = 0; d < rank; ++d)
    {
      if (bstart[d] >= coord.shape[static_cast<std::size_t>(d)])
      {
        std::cerr << "Start index beyond array extent (dim " << d << ") for coordinate '" << cname << "'\n";
        return false;
      }
      bcount[d] = std::min(bcount[d], coord.shape[static_cast<std::size_t>(d)] - bstart[d]);
      bcount[d] = std::max<Index>(bcount[d], 1);
    }

    std::vector<Index> chunks(bcount);
    if (auto cl = store.chunk_layout())
    {
      for (DimensionIndex d = 0; d < rank; ++d)
      {
        const Index s = cl->write_chunk_shape()[d];
        if (s > 0) chunks[static_cast<std::size_t>(d)] = std::min(std::max<Index>(s, 1), bcount[d]);
      }
    }
    for (auto &ch : chunks) ch = std::max<Index>(ch, 1);
    if (cfg.step_chunk && rank > 0 && !static_coord) { chunks[0] = std::max<Index>(1, std::min<Index>(*cfg.step_chunk, bcount[0])); }
    if (cfg.cell_chunk && rank > 0)
    {
      chunks[static_cast<std::size_t>(rank) - 1]
          = std::max<Index>(1, std::min<Index>(*cfg.cell_chunk, bcount[static_cast<std::size_t>(rank) - 1]));
    }

    const auto dtype_code = coord_dtype_encoded(store.dtype(), cfg.out_format);
    if (!dtype_code)
    {
      std::cerr << "Data type '" << store.dtype().name() << "' of coordinate '" << cname << "' is not supported for writing\n";
      return false;
    }

    const std::string dir = *cfg.out_path + "/" + cname + "/";

    nlohmann::json src_attrs = nlohmann::json::object(), src_dims = nlohmann::json::array();
    try
    {
      src_attrs = nlohmann::json::parse(coord.attributes);
      src_dims = nlohmann::json::parse(coord.dim_names);
    }
    catch (...)
    {
      src_attrs = nlohmann::json::object();
      src_dims = nlohmann::json::array();
    }

    nlohmann::json md;
    if (cfg.out_format == "zarr3")
    {
      md = { { "zarr_format", 3 },
             { "node_type", "array" },
             { "shape", bcount },
             { "chunk_grid", { { "name", "regular" }, { "configuration", { { "chunk_shape", chunks } } } } },
             { "data_type", *dtype_code },
             { "fill_value", 0 } };
      if (!src_attrs.empty()) md["attributes"] = src_attrs;
      if (!src_dims.empty()) md["dimension_names"] = src_dims;
    }
    else if (cfg.out_format == "n5")
    {
      // N5: mandatory attributes plus the source attributes (with
      // `_ARRAY_DIMENSIONS`) as additional attributes in attributes.json.
      md = { { "dimensions", bcount }, { "blockSize", chunks }, { "dataType", *dtype_code } };
      {
        const nlohmann::json extra = n5_extra_attrs(src_attrs, src_dims);
        for (auto it = extra.begin(); it != extra.end(); ++it) md[it.key()] = it.value();
      }
    }
    else
    {
      md = { { "zarr_format", 2 },     { "shape", bcount },       { "chunks", chunks },
             { "dtype", *dtype_code }, { "compressor", nullptr }, { "fill_value", 0 },
             { "order", "C" },         { "filters", nullptr },    { "dimension_separator", "." } };
    }

    if (cfg.compressor) { apply_compressor_by_name(*cfg.compressor, cfg.out_format, static_cast<int>(store.dtype().size()), md); }
    else
    {
      nlohmann::json src;
      try
      {
        src = nlohmann::json::parse(coord.source_metadata);
      }
      catch (...)
      {
        src = nlohmann::json();
      }
      apply_source_compression_json(src, coord.format, cfg.out_format, static_cast<int>(store.dtype().size()), chunks, md);
    }

    auto read = Read<zero_origin>(store | Box(bstart, bcount)).result();
    if (!read)
    {
      std::cerr << "Failed to read coordinate '" << cname << "': " << read.status().ToString() << "\n";
      return false;
    }

    coord.directory = dir;
    coord.driver = (cfg.out_format == "zarr3") ? "zarr3" : (cfg.out_format == "n5") ? "n5" : "zarr";
    coord.metadata = md.dump();
    coord.data = std::make_shared<SharedArray<void, dynamic_rank>>(std::move(*read));
  }
  return true;
}

// Schreibt die mit tsReadCoordinates gelesenen Koordinaten in die Ausgabe.
bool
tsWriteCoordinates(tsHandle *output, const std::vector<tsCoordinate> &coords)
{
  using namespace tensorstore;
  if (!output) return false;
  auto *out = static_cast<tsHandleImpl *>(output);
  for (const tsCoordinate &coord : coords)
  {
    nlohmann::json md;
    try
    {
      md = nlohmann::json::parse(coord.metadata);
    }
    catch (...)
    {
      std::cerr << "Invalid metadata for coordinate '" << coord.name << "'\n";
      return false;
    }

    nlohmann::json spec{ { "driver", coord.driver },
                         { "kvstore", { { "driver", "file" }, { "path", coord.directory } } },
                         { "create", true },
                         { "delete_existing", true },
                         { "metadata", md } };
    auto target = Open<void, dynamic_rank>(spec, out->context, OpenMode::create, ReadWriteMode::write).result();
    if (!target)
    {
      std::cerr << "Failed to create output store for coordinate '" << coord.name << "': " << target.status().ToString() << "\n";
      return false;
    }

    if (coord.driver == "zarr")
    {
      nlohmann::json src_attrs = nlohmann::json::object(), src_dims = nlohmann::json::array();
      try
      {
        src_attrs = nlohmann::json::parse(coord.attributes);
        src_dims = nlohmann::json::parse(coord.dim_names);
      }
      catch (...)
      {
      }
      if (!write_v2_zattrs_if_needed(coord.directory, src_attrs, src_dims)) return false;
    }

    auto *arr = static_cast<const SharedArray<void, dynamic_rank> *>(coord.data.get());
    if (!arr)
    {
      std::cerr << "Missing data for coordinate '" << coord.name << "'\n";
      return false;
    }
    const auto st = Write(*arr, *target).result();
    if (!st)
    {
      std::cerr << "Failed to write coordinate '" << coord.name << "': " << st.status().ToString() << "\n";
      return false;
    }
  }
  return true;
}

std::vector<std::string>
tsListArrays(const tsConfig &config)
{
  tensorstore::Context context;
  std::optional<int> concurrency = config.data_copy_concurrency;
  make_context(config, context, concurrency);
  if (!concurrency) return {};
  return list_array_names(config, context);
}

// Uebersicht des Datensatzes: Gruppenliste (konsolidierte Zarr-3-Wurzel) oder
// eine Zeile pro Array.
bool
tsDatasetOverview(const tsConfig &config, tsOverview &ov)
{
  using namespace tensorstore;
  ov = tsOverview();
  Context context;
  std::optional<int> concurrency = config.data_copy_concurrency;
  make_context(config, context, concurrency);
  if (!concurrency) return false;

  if (config.group)
  {
    const nlohmann::json root_kv = build_root_kvstore(config, context);

    const auto meta3 = read_zarr3_consolidated(root_kv, context);
    if (!meta3.empty())
    {
      const std::vector<std::string> arrays = consolidated_children(meta3, *config.group, /*want_groups=*/false);
      if (arrays.empty())
      {
        std::cerr << "No arrays found in group '" << *config.group << "'\n";
        return false;
      }
      return overview_rows_from_consolidated(meta3, *config.group, arrays, ov.arrays);
    }

    const auto meta2 = read_zarr2_consolidated(root_kv, context);
    if (meta2.empty())
    {
      std::cerr << "No consolidated Zarr metadata found at store root: " << config.path << "\n";
      return false;
    }
    const std::vector<std::string> arrays = zarr2_children(meta2, *config.group, /*want_groups=*/false);
    if (arrays.empty())
    {
      std::cerr << "No arrays found in group '" << *config.group << "'\n";
      return false;
    }
    return overview_rows_from_zarr2_consolidated(meta2, *config.group, arrays, ov.arrays);
  }

  const nlohmann::json root_kv = build_root_kvstore(config, context);

  const auto meta3 = read_zarr3_consolidated(root_kv, context);
  if (!meta3.empty())
  {
    ov.groups = consolidated_children(meta3, "", /*want_groups=*/true);
    if (!ov.groups.empty()) return true;
    const std::vector<std::string> arrays = consolidated_children(meta3, "", /*want_groups=*/false);
    if (!arrays.empty()) { return overview_rows_from_consolidated(meta3, "", arrays, ov.arrays); }
  }

  const auto meta2 = read_zarr2_consolidated(root_kv, context);
  if (!meta2.empty())
  {
    ov.groups = zarr2_children(meta2, "", /*want_groups=*/true);
    if (!ov.groups.empty()) return true;
    const std::vector<std::string> arrays = zarr2_children(meta2, "", /*want_groups=*/false);
    if (!arrays.empty()) { return overview_rows_from_zarr2_consolidated(meta2, "", arrays, ov.arrays); }
  }

  const std::vector<std::string> names = list_array_names(config, context);
  if (names.empty())
  {
    std::cerr << "No zarr arrays found in dataset at: " << config.path << "\n";
    return false;
  }
  return overview_rows_from_stores(config, context, names, ov.arrays);
}

void
tsCloseFile(tsHandle *handle)
{
  if (!handle) return;
  auto *h = static_cast<tsHandleImpl *>(handle);
  for (auto &w : h->writers) flush_window(w);
  delete h;
}

bool
ts_read_array(tsHandle *handle, const char *arrayName, double *buffer)
{
  using namespace tensorstore;
  if (!handle || !arrayName || !buffer) return false;
  auto *h = static_cast<tsHandleImpl *>(handle);

  // Array wie in open_array_any öffnen (zarr2/zarr3)
  std::string kvstore_out;
  int detected_format = 0;
  auto st_res = open_array_any(h->cfg, h->context, std::string(arrayName), kvstore_out, detected_format);
  // auto st_res = open_array_impl<double>(h->cfg, h->context, std::string(arrayName), kvstore_out, detected_format);
  if (!st_res)
  {
    std::cerr << "ts_read_array: failed to open array '" << arrayName << "'\n";
    return false;
  }
  auto &st = *st_res;

  // Komplettes Array lesen (zero-origin)
  auto read_res = Read<zero_origin>(st).result();
  if (!read_res)
  {
    std::cerr << "ts_read_array: failed to read array '" << arrayName << "': " << read_res.status().ToString() << "\n";
    return false;
  }

  auto arr = std::move(*read_res);
  // arr ist SharedArray<void,...> oder typisiert; iterieren und nach double konvertieren
  // Total elements
  Index total = 1;
  for (DimensionIndex d = 0; d < arr.rank(); ++d) { total *= arr.shape()[d]; }
  if (total == 0) return true;

  // Typsicher konvertieren (analog zu append_as_float im Repo)
  const DataType dtype = arr.dtype();
  // iterate over array in order
  // einfache Variante: kopieren je nach dtype
  // (kann auch mit tensorstore::Iterate gemacht werden)
  /*
  const double *src = static_cast<const double *>(arr.data());
  std::copy(src, src + total, buffer);
  return true;
  */
  if (dtype == dtype_v<double>)
  {
    auto *src = static_cast<const double *>(arr.data());
    std::copy(src, src + total, buffer);
    return true;
  }
  else if (dtype == dtype_v<float>)
  {
    auto *src = static_cast<const float *>(arr.data());
    for (Index i = 0; i < total; ++i) buffer[i] = static_cast<double>(src[i]);
    return true;
  }
  else if (dtype == dtype_v<int64_t>)
  {
    auto *src = static_cast<const int64_t *>(arr.data());
    for (Index i = 0; i < total; ++i) buffer[i] = static_cast<double>(src[i]);
    return true;
  }
  else if (dtype == dtype_v<int32_t>)
  {
    auto *src = static_cast<const int32_t *>(arr.data());
    for (Index i = 0; i < total; ++i) buffer[i] = static_cast<double>(src[i]);
    return true;
  }
  else if (dtype == dtype_v<tensorstore::dtypes::float16_t>)
  {
    // je nach Toolchain Half/float16 -> konvertieren (Repo hat bereits Pfade dafür)
    // hier vereinfacht
    auto *src = static_cast<const uint16_t *>(arr.data());
    // ... Konvertierung wie im Codebase nutzen
  }
  else if (dtype == dtype_v<tensorstore::dtypes::bfloat16_t>)
  {
    // ähnlich
  }

  std::cerr << "ts_read_array: unsupported dtype '" << dtype.name() << "'\n";
  return false;
}
