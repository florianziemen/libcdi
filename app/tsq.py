#! /usr/bin/env python3
"""Reference implementation (Python) for the C++ program zarr_mean.

Reads Zarr 2 / Zarr 3 arrays from disk, S3 or HTTP (via TensorStore semantics)
and prints the NaN-aware mean over all spatial dimensions for each
timestep of the first dimension.

Usage mirrors zarr_mean:
  tsq.py --path <DIR or bucket/prefix or https://...> --name a,b,c
               [--kvstore file|s3|http] [--format zarr2|zarr3] [--endpoint <URL>]
               [--step <START/END>] [--layer <START/END>] [--cell <START/END>]
               [--consolidated] [--group <NAME>] [--data-copy-concurrency <N>]
The Zarr format and the kvstore type are auto-detected.
A kvstore is 'file' when --path contains no '://', else 'http'.
--group selects a nested Zarr 3 group below --path; its metadata is served
from the consolidated `zarr.json` of the store root (no recursive reads).
Without --group, a Zarr 3 root that only contains groups lists them.
--step, --layer and --cell select the hypercube as 1-based inclusive
START/END ranges (time dim, layer dim, last dim); omitted means all.
"""

import argparse
import asyncio
import math
import os
import resource
import shutil
import sys
import time
import urllib.error
import urllib.request

import numpy as np
from zarr.core.buffer import Buffer, BufferPrototype
from zarr.storage import MemoryStore

# Mirrors the TensorStore version pinned in CMakeLists.txt for the C++ tool;
# printed as the very first stdout line when --info is given (byte-identical).
TENSORSTORE_VERSION = "0.1.dev"

_METADATA_KEYS = (".zarray", ".zattrs", ".zgroup", "zarr.json")


def _apply_byte_range(value: bytes, byte_range) -> bytes:
    """Slice `value` per a store `byte_range` request.

    Accepts the zarr-python ByteRequest objects (RangeByteRequest /
    OffsetByteRequest / SuffixByteRequest) as well as the legacy
    ``(start, length)`` tuple where ``length is None`` means "to the end".
    """
    if byte_range is None:
        return value
    if isinstance(byte_range, tuple):
        offset, length = byte_range
        if length is None:
            length = len(value) - offset
        return value[offset : offset + length]
    from zarr.abc.store import OffsetByteRequest, RangeByteRequest, SuffixByteRequest

    if isinstance(byte_range, RangeByteRequest):
        return value[byte_range.start : byte_range.end]
    if isinstance(byte_range, OffsetByteRequest):
        return value[byte_range.offset :]
    if isinstance(byte_range, SuffixByteRequest):
        return value[max(0, len(value) - byte_range.suffix) :]
    return value


def sanitize_invalid_json(data: bytes) -> bytes:
    """Quote bare NaN/Infinity/-Infinity tokens that appear outside of JSON
    strings (e.g. ``"fill_value": NaN``), so that non-conforming metadata can
    still be parsed.  Mirrors the C++ implementation in main.cpp."""
    tokens = (b"NaN", b"Infinity", b"-Infinity")

    def is_ident(c: int) -> bool:
        # ASCII alnum, underscore, '-', '.'
        return (
            (48 <= c <= 57) or (65 <= c <= 90) or (97 <= c <= 122) or c in (95, 45, 46)
        )

    out = bytearray()
    in_string = False
    escape = False
    i = 0
    n = len(data)
    while i < n:
        c = data[i]
        if in_string:
            out.append(c)
            i += 1
            if escape:
                escape = False
            elif c == 0x5C:  # backslash
                escape = True
            elif c == 0x22:  # double quote
                in_string = False
            continue
        if c == 0x22:
            in_string = True
            out.append(c)
            i += 1
            continue
        replaced = False
        for token in tokens:
            if not data.startswith(token, i):
                continue
            prev_ok = i == 0 or not is_ident(data[i - 1])
            nxt = i + len(token)
            next_ok = nxt >= n or not is_ident(data[nxt])
            if not (prev_ok and next_ok):
                continue
            out.append(0x22)
            out.extend(token)
            out.append(0x22)
            i = nxt
            replaced = True
            break
        if not replaced:
            out.append(c)
            i += 1
    return bytes(out)


class _RemoteHTTPStore(MemoryStore):
    """Read-only HTTP-backed store rooted at an array directory.

    Array metadata files (``.zarray``, ``.zattrs``, ``.zgroup``, ``zarr.json``)
    are sanitized (invalid bare NaN/Infinity tokens are quoted) and cached;
    chunk entries are fetched lazily and passed through unchanged.  Mirrors the
    C++ `kvstack` + sanitize overlay.
    """

    def __init__(self, base_url: str, array_dir: str):
        super().__init__(store_dict={}, read_only=True)
        self._base = base_url.rstrip("/") + "/" + array_dir.strip("/")

    def _url(self, key: str) -> str:
        return f"{self._base}/{key}"

    def _fetch(self, key: str) -> bytes | None:
        try:
            with urllib.request.urlopen(self._url(key), timeout=60) as resp:
                data = resp.read()
        except urllib.error.HTTPError as e:
            if e.code == 404:
                return None
            raise
        if key in _METADATA_KEYS:
            data = sanitize_invalid_json(data)
        return data

    @staticmethod
    def _slice(value: bytes, byte_range):
        return _apply_byte_range(value, byte_range)

    async def get(
        self, key, prototype: BufferPrototype, byte_range=None
    ) -> Buffer | None:
        # docstring inherited
        if not self._is_open:
            await self._open()
        try:
            value = self._store_dict[key]
        except KeyError:
            raw = await asyncio.to_thread(self._fetch, key)
            if raw is None:
                return None
            value = raw
            self._store_dict[key] = value
        return prototype.buffer.from_bytes(self._slice(value, byte_range))

    async def exists(self, key: str) -> bool:
        # docstring inherited
        if key in self._store_dict:
            return True
        raw = await asyncio.to_thread(self._fetch, key)
        if raw is None:
            return False
        self._store_dict[key] = raw
        return True

    async def list(self):
        # docstring inherited
        for key in _METADATA_KEYS:
            if await self.exists(key):
                yield key


class _ConsolidatedStore(MemoryStore):
    """Read-only store whose metadata is supplied from the group-level
    ``.zmetadata`` (zarr2 consolidated metadata) and whose chunk data is
    read lazily from a backend via ``fetch_chunk(key)``.

    ``metadata`` maps array-relative keys (``.zarray``, ``.zattrs``, etc.) to
    raw bytes as they would be stored in ``.zmetadata``.  ``fetch_chunk`` is
    called for all non-metadata keys (chunk data) and should return ``None``
    on 404.  Mirrors the C++ ``build_consolidated_kvstore``.
    """

    def __init__(
        self,
        metadata: dict[str, bytes],
        fetch_chunk,  # Callable[[str], bytes | None]
    ):
        super().__init__(store_dict={}, read_only=True)
        self._metadata = metadata
        self._fetch_chunk = fetch_chunk
        self._cache: dict[str, bytes] = {}

    def _get_data(self, key: str) -> bytes | None:
        if key in self._metadata:
            return self._metadata[key]
        if key in self._cache:
            return self._cache[key]
        data = self._fetch_chunk(key)
        if data is not None:
            self._cache[key] = data
        return data

    async def get(
        self, key, prototype: BufferPrototype, byte_range=None
    ) -> Buffer | None:
        # docstring inherited
        if not self._is_open:
            await self._open()
        raw = await asyncio.to_thread(self._get_data, key)
        if raw is None:
            return None
        return prototype.buffer.from_bytes(_apply_byte_range(raw, byte_range))

    async def exists(self, key: str) -> bool:
        # docstring inherited
        return self._get_data(key) is not None

    async def list(self):
        # docstring inherited
        for key in self._metadata:
            yield key


def _read_consolidated_zmetadata(
    group_url_or_path: str, array_name: str
) -> dict[str, bytes]:
    """Fetch the ``.zmetadata`` file from a group root and return a dict
    mapping array-relative metadata keys (``.zarray``, ``.zattrs``,
    ``.zgroup``) to serialized JSON bytes."""
    import json

    url = group_url_or_path.rstrip("/") + "/.zmetadata"
    if url.startswith("http"):
        with urllib.request.urlopen(url, timeout=60) as resp:
            raw = resp.read()
    else:
        try:
            with open(url, "rb") as fh:
                raw = fh.read()
        except FileNotFoundError:
            raise RuntimeError(
                f"No '.zmetadata' found at {url} (consolidated metadata missing)"
            ) from None

    meta = json.loads(raw).get("metadata", {})
    out: dict[str, bytes] = {}
    for suffix in (".zarray", ".zattrs", ".zgroup"):
        key = f"{array_name}/{suffix}"
        value = meta.get(key)
        if value is not None:
            out[suffix] = json.dumps(value).encode()
    return out


def _read_root_zarr3_consolidated(root: str) -> dict:
    """Return the node map {relative path: node dict} from the consolidated
    ``zarr.json`` of a Zarr 3 store root (e.g. "group" and "group/array").
    Raises when the root has no consolidated metadata.  Mirrors the C++
    ``read_zarr3_consolidated``."""
    import json

    url = root.rstrip("/") + "/zarr.json"
    j = json.loads(_read_file_or_url(url))
    meta = (j.get("consolidated_metadata") or {}).get("metadata")
    if not isinstance(meta, dict):
        raise RuntimeError(f"No consolidated Zarr 3 metadata found at {url}")
    return meta


def _read_root_zarr2_consolidated(root: str) -> dict:
    """Return the `.zmetadata` node map {full relative file key: node dict} of
    a Zarr 2 store root (e.g. "group/.zgroup", "group/array/.zarray").
    Raises when the root has no `.zmetadata`.  Mirror of the C++
    `read_zarr2_consolidated`."""
    import json

    url = root.rstrip("/") + "/.zmetadata"
    j = json.loads(_read_file_or_url(url))
    meta = j.get("metadata")
    if not isinstance(meta, dict):
        raise RuntimeError(f"No consolidated Zarr 2 metadata found at {url}")
    return meta


def _zarr2_children(meta: dict, prefix: str, want_groups: bool) -> list[str]:
    """Direct child names below `prefix` in a Zarr 2 `.zmetadata` map.  A name
    is a group when `prefix/name/.zgroup` exists, an array when
    `prefix/name/.zarray` exists.  Mirrors the C++ `zarr2_children`."""
    pfx = f"{prefix}/" if prefix else ""
    marker = ".zgroup" if want_groups else ".zarray"
    names = set()
    for key in meta:
        if not key.startswith(pfx):
            continue
        rest = key[len(pfx) :]
        if "/" not in rest:
            continue  # e.g. ".zgroup", ".zattrs"
        head, tail = rest.split("/", 1)
        if tail == marker:
            names.add(head)
    return sorted(names)


def _consolidated_children(meta: dict, prefix: str, want_groups: bool) -> list[str]:
    """Direct child node names below `prefix` in a consolidated node map
    (`prefix == ""` selects root children).  Keeps group nodes when
    `want_groups`, else array nodes.  Mirrors the C++ `consolidated_children`."""
    pfx = f"{prefix}/" if prefix else ""
    out = []
    for key, node in meta.items():
        if not key.startswith(pfx):
            continue
        base = key[len(pfx) :]
        if "/" in base or not base:
            continue
        if (node.get("node_type") == "group") is want_groups:
            out.append(base)
    return sorted(out)


def _build_group_store(kvstore: str, path: str, group: str, array_name: str):
    """Build a store rooted at ``--path`` whose metadata for ``group`` /
    ``group/array`` is served from the root's consolidated metadata (Zarr 3
    `zarr.json`, or Zarr 2 `.zmetadata`) and whose chunk data is read from the
    real store.  Returns (store, open_path) with open_path = "group/array"."""
    import json

    try:
        meta = _read_root_zarr3_consolidated(path)
        group_node = meta.get(group)
        array_node = meta.get(f"{group}/{array_name}")
        if group_node is None:
            raise RuntimeError(f"No consolidated Zarr 3 metadata for group '{group}'")
        if array_node is None:
            raise RuntimeError(
                f"No consolidated Zarr 3 metadata for array '{array_name}' in group '{group}'"
            )
        metadata = {
            f"{group}/zarr.json": json.dumps(group_node).encode(),
            f"{group}/{array_name}/zarr.json": json.dumps(array_node).encode(),
        }
    except Exception as z3_err:
        try:
            meta2 = _read_root_zarr2_consolidated(path)
        except Exception:
            raise z3_err from None
        group_node = meta2.get(f"{group}/.zgroup")
        array_node = meta2.get(f"{group}/{array_name}/.zarray")
        if group_node is None:
            raise RuntimeError(
                f"No consolidated Zarr 2 metadata for group '{group}'"
            ) from None
        if array_node is None:
            raise RuntimeError(
                f"No consolidated Zarr 2 metadata for array '{array_name}' in group '{group}'"
            ) from None
        metadata = {
            f"{group}/.zgroup": json.dumps(group_node).encode(),
            f"{group}/{array_name}/.zarray": json.dumps(array_node).encode(),
        }
        attrs = meta2.get(f"{group}/{array_name}/.zattrs")
        if attrs is not None:
            metadata[f"{group}/{array_name}/.zattrs"] = json.dumps(attrs).encode()

    if kvstore == "file":

        def fetch(key):
            try:
                with open(os.path.join(path, key), "rb") as fh:
                    return fh.read()
            except FileNotFoundError:
                return None

    elif kvstore == "http":
        scheme = path.find("://")
        slash = path.find("/", scheme + 3)
        base_url = path[:slash]
        group_dir = path[slash + 1 :]
        fetch = _RemoteHTTPStore(base_url, group_dir)._fetch
    else:
        raise RuntimeError(
            "--group is not supported for s3 kvstore in the Python reference implementation"
        )

    return _ConsolidatedStore(metadata, fetch), f"{group}/{array_name}"


def resolve_kvstore(cfg):
    """Return (kvstore, path, endpoint) with auto-detection applied.

    kvstore is "" (auto): 'file' if --path has no '://', else 'http'.
    For s3, --path may be "bucket/prefix" or a full
    "https://endpoint/bucket/prefix" URL from which the endpoint is derived.
    For http, --path is a full "https://host/base/group" URL.
    """
    kvstore = cfg.kvstore
    path = cfg.path
    endpoint = cfg.endpoint

    if not kvstore:
        kvstore = "file" if "://" not in path else "http"

    if kvstore == "s3" and path.startswith("https://"):
        slash = path.find("/", len("https://"))
        derived_endpoint = path if slash == -1 else path[:slash]
        path = "" if slash == -1 else path[slash + 1 :]
        endpoint = endpoint or derived_endpoint

    return kvstore, path, endpoint


def build_store(kvstore, path, endpoint, array_name, consolidated=False, group=None):
    """Return (store, open_path).  `open_path` is the array name relative to
    the store root, or None when the store is already rooted at the array."""

    def _http_group_base():
        scheme = path.find("://")
        slash = path.find("/", scheme + 3)
        base_url = path[:slash]
        group_dir = path[slash + 1 :]
        return base_url, group_dir

    if group is not None:
        return _build_group_store(kvstore, path, group, array_name)

    if kvstore == "file":
        if consolidated:
            meta = _read_consolidated_zmetadata(path, array_name)
            array_dir = os.path.join(path, array_name)

            def _fetch_file(key):
                try:
                    return open(os.path.join(array_dir, key), "rb").read()
                except FileNotFoundError:
                    return None

            return _ConsolidatedStore(meta, _fetch_file), None
        return path, array_name

    if kvstore == "http":
        base_url, group_dir = _http_group_base()
        array_dir = f"{group_dir}/{array_name}"
        if consolidated:
            meta = _read_consolidated_zmetadata(path, array_name)
            remote = _RemoteHTTPStore(base_url, array_dir)
            return _ConsolidatedStore(meta, remote._fetch), None
        return _RemoteHTTPStore(base_url, array_dir), None

    if consolidated:
        raise RuntimeError(
            "--consolidated is not supported for s3 kvstore in the Python reference implementation"
        )

    import fsspec
    from zarr.storage import FSStore

    opts = {}
    if endpoint:
        opts["client_kwargs"] = {"endpoint_url": endpoint}
    fs = fsspec.filesystem("s3", **opts)
    return FSStore(path, fs=fs), array_name


class _MeanEngine:
    """Per-array chunk-caching state for the t-major mean table.  Every array
    gets its OWN engine (own window cache), so the required memory is the sum
    of all array caches, and each cache is sized to that array's hypercube.
    Layout: dim 0 = time, last dim = grid points; for rank >= 3 the middle
    dims are the layers, for which one mean is printed per layer."""

    def __init__(
        self,
        z,
        name,
        step,
        layer,
        cell,
    ):
        self.name = name
        shape = z.shape
        rank = len(shape)
        if rank < 1:
            raise ValueError("Array must have at least rank 1")

        # Time-constant arrays (dimension names known and no "time") have no
        # time axis: they are read exactly once.  For rank >= 2 the first
        # dimension is then the layer dimension and --step is ignored.
        dims = _array_dims(z)
        self.time_constant = bool(dims) and "time" not in dims

        # Resolve the requested hypercube (1-based inclusive --step/--layer/
        # --cell ranges) against the domain.  0 = time, 1 = layers (rank >= 3),
        # last dim = grid points.  A None end means "up to the extent".
        bs = [0] * rank
        bc = list(shape)
        if self.time_constant:
            if rank >= 2:
                bs[0] = max(0, layer[0] - 1)
                if layer[1] is not None:
                    bc[0] = layer[1] - layer[0] + 1
            bs[rank - 1] = max(0, cell[0] - 1)
            if cell[1] is not None:
                bc[rank - 1] = cell[1] - cell[0] + 1
        else:
            bs[0] = max(0, step[0] - 1)
            if step[1] is not None:
                bc[0] = step[1] - step[0] + 1
            if rank >= 3:
                bs[1] = max(0, layer[0] - 1)
                if layer[1] is not None:
                    bc[1] = layer[1] - layer[0] + 1
            if rank >= 2:
                bs[rank - 1] = max(0, cell[0] - 1)
                if cell[1] is not None:
                    bc[rank - 1] = cell[1] - cell[0] + 1
        for d in range(rank):
            bs[d] = min(max(bs[d], 0), shape[d] - 1)
            bc[d] = max(1, min(bc[d], shape[d] - bs[d]))
        self.bs = bs
        self.bc = bc
        self.layered = rank >= 2 if self.time_constant else rank >= 3

        chunks = z.chunks
        time_chunk = chunks[0] if chunks else None
        if not time_chunk or time_chunk <= 0:
            time_chunk = bc[0]

        # Time windows over the selected range, split at physical time-chunk
        # boundaries so a window never spans two chunks (this keeps the
        # per-window allocated data = the timesteps of one chunk that are
        # actually needed).
        self.windows = []
        if not self.time_constant:
            tcur = bs[0]
            remaining = bc[0]
            while remaining > 0:
                chunk_base = (tcur // time_chunk) * time_chunk
                nw = min(remaining, chunk_base + time_chunk - tcur)
                self.windows.append((tcur, nw))
                tcur += nw
                remaining -= nw
        self.z = z
        self.widx = 0
        self.wcache = None  # cached np.ndarray for one window
        self.wcache_win = -1  # window index of wcache
        self.wcache_t0 = 0
        self.raw = False  # keep the window data in the original dtype
        self._out_win = -1  # window already written to the --out-path store

    def _read_window(self, win):
        tcur, nw = win
        rank = len(self.bs)
        sl = (slice(tcur, tcur + nw),) + tuple(
            slice(self.bs[d], self.bs[d] + self.bc[d]) for d in range(1, rank)
        )
        # In --out-path write mode the raw float32 values are written verbatim;
        # otherwise the window is converted to float64 for exact min/mean/max.
        dtype = None if self.raw else np.float64
        return np.asarray(self.z[sl], dtype=dtype)

    def ensure(self, at):
        # Advance to the window containing `at` (windows are visited in
        # ascending t, so this only moves forward).
        while at >= self.windows[self.widx][0] + self.windows[self.widx][1]:
            self.widx += 1
        if self.wcache_win != self.widx:
            self.wcache = self._read_window(self.windows[self.widx])
            self.wcache_t0 = self.windows[self.widx][0]
            self.wcache_win = self.widx


def _source_metadata(z, fmt):
    """Return the source array's raw `attributes` object and `dimension_names`
    list so both can be copied onto the corresponding output array.  For Zarr 2
    the `_ARRAY_DIMENSIONS` key is separated out from the attributes (it is
    restored when the output attributes are written)."""
    if fmt == 3:
        attrs = dict(getattr(z.metadata, "attributes", None) or {})
        dn = getattr(z.metadata, "dimension_names", None)
        dims = list(dn) if dn else []
    else:
        attrs = dict(z.attrs or {})
        d = attrs.pop("_ARRAY_DIMENSIONS", None)
        dims = list(d) if d else []
    return attrs, dims


def _out_meta_kwargs(fmt, attrs, dims):
    """Extra zarr.open keyword arguments that copy the source array's
    attributes and dimension names onto an output array.  For Zarr 3 both are
    metadata keys; for Zarr 2 they are merged into the `.zattrs` sidecar."""
    if fmt == "zarr3":
        kw = {}
        if attrs:
            kw["attributes"] = attrs
        if dims:
            kw["dimension_names"] = dims
        return kw
    combined = dict(attrs)
    if dims and "_ARRAY_DIMENSIONS" not in combined:
        combined["_ARRAY_DIMENSIONS"] = dims
    return {"attributes": combined} if combined else {}


def _source_compression(z, fmt):
    """Raw compression metadata of the source: a Zarr 3 `codecs` list (verbatim
    JSON) or a Zarr 2 compressor dict (`None` when uncompressed)."""
    if fmt == 3:
        import json as _json

        return (
            _json.loads(_json.dumps([c.to_dict() for c in z.metadata.codecs]))
            if z.metadata.codecs
            else []
        )
    comp = getattr(z.metadata, "compressor", None)
    return None if comp is None else comp.get_config()


def _v2_shuffle_to_v3(v):
    if v == 1:
        return "shuffle"
    if v == 2:
        return "bitshuffle"
    return "noshuffle"


def _v3_shuffle_to_v2(s):
    if s == "shuffle":
        return 1
    if s in ("bitshuffle", "byte_shuffle"):
        return 2
    return 0


def _v2_compressor_to_v3(comp, dtype_size):
    """Convert a Zarr 2 compressor dict into a Zarr 3 codec JSON (None for
    `None` / unknown compressors, i.e. no compression codec)."""
    if not comp or not isinstance(comp, dict):
        return None
    cid = comp.get("id")
    if cid == "zlib":
        return {"name": "zlib", "configuration": {"level": comp.get("level", -1)}}
    if cid == "gzip":
        return {"name": "gzip", "configuration": {"level": comp.get("level", -1)}}
    if cid == "blosc":
        return {
            "name": "blosc",
            "configuration": {
                "typesize": int(dtype_size),
                "cname": comp.get("cname", "lz4"),
                "clevel": comp.get("clevel", 5),
                "shuffle": _v2_shuffle_to_v3(comp.get("shuffle", 0)),
                "blocksize": comp.get("blocksize", 0),
            },
        }
    return None


_V3_STRUCTURAL_CODECS = {
    "bytes",
    "crc32c",
    "crc32c_no_separate_header",
    "transpose",
    "bitshuffle",
    "delta",
    "pcodec",
    "sharding_indexed",
}


def _v3_codec_to_v2(codec):
    """Convert a single Zarr 3 compressor codec into a Zarr 2 compressor dict."""
    name = codec.get("name") if isinstance(codec, dict) else None
    conf = codec.get("configuration") or {} if isinstance(codec, dict) else {}
    if not isinstance(conf, dict):
        conf = {}
    if name == "blosc":
        return {
            "id": "blosc",
            "cname": conf.get("cname", "lz4"),
            "clevel": conf.get("clevel", 5),
            "shuffle": _v3_shuffle_to_v2(conf.get("shuffle", "shuffle")),
            "blocksize": conf.get("blocksize", 0),
        }
    if name == "zlib":
        return {"id": "zlib", "level": conf.get("level", -1)}
    if name == "gzip":
        return {"id": "gzip", "level": conf.get("level", -1)}
    if name == "zstd":
        return {"id": "zstd", "level": conf.get("level", 0)}
    return None


def _first_v3_compressor(codecs):
    """First effective compressor of a Zarr 3 `codecs` list (recursing into
    nested sharding configurations) as a Zarr 2 compressor dict (or None)."""
    if not isinstance(codecs, list):
        return None
    for c in codecs:
        name = c.get("name") if isinstance(c, dict) else None
        if name == "sharding_indexed":
            conf = c.get("configuration") or {}
            inner = (
                _first_v3_compressor(conf.get("codecs", []))
                if isinstance(conf, dict)
                else None
            )
            if inner:
                return inner
            continue
        if isinstance(c, dict) and name and name not in _V3_STRUCTURAL_CODECS:
            return _v3_codec_to_v2(c)
    return None


def _adapt_sharding(codecs, out_chunks):
    """Adapt a copied Zarr 3 `sharding_indexed` inner `chunk_shape` to the
    output chunk shape: the inner chunk must divide the outer chunk, which a
    selected hypercube's clamped outer chunk may violate."""
    if not isinstance(codecs, list) or not isinstance(out_chunks, (tuple, list)):
        return codecs
    for c in codecs:
        if isinstance(c, dict) and c.get("name") == "sharding_indexed":
            conf = c.get("configuration") or {}
            cs = conf.get("chunk_shape")
            if not isinstance(cs, list) or len(cs) != len(out_chunks):
                continue
            nc = []
            for inner, outer in zip(cs, out_chunks):
                inner = int(inner)
                if outer < 1:
                    nc.append(inner)
                    continue
                if outer % inner != 0:
                    if inner > outer:
                        inner = outer
                    while outer % inner != 0:
                        inner -= 1
                    if inner < 1:
                        inner = 1
                nc.append(inner)
            conf["chunk_shape"] = nc
    return codecs


def _out_compression_kwargs(src_fmt, src_comp, out_fmt, dtype_size, out_chunks=None):
    """zarr.open keyword arguments that reproduce the input array's compression
    on the output array (same method, converted if the formats differ)."""
    if out_fmt == "zarr3":
        if src_fmt == 3:
            if src_comp:
                return {"codecs": _adapt_sharding(src_comp, out_chunks)}
            return {
                "codecs": [{"name": "bytes", "configuration": {"endian": "little"}}]
            }
        codecs = [{"name": "bytes", "configuration": {"endian": "little"}}]
        mapped = _v2_compressor_to_v3(src_comp, dtype_size)
        if mapped:
            codecs.append(mapped)
        return {"codecs": codecs}
    if src_fmt == 2:
        return {"compressor": src_comp}
    return {"compressor": _first_v3_compressor(src_comp)}


def _compressor_v2_by_name(name):
    """Canonical Zarr 2 compressor dict for --compressor NAME (None if unknown)."""
    if name == "blosc":
        return {"id": "blosc", "cname": "zstd", "clevel": 5, "shuffle": 1, "blocksize": 0}
    if name == "gzip":
        return {"id": "gzip", "level": 5}
    if name == "zstd":
        return {"id": "zstd", "level": 5}
    return None


def _compressor_v3_by_name(name, dtype_size):
    """Zarr 3 compressor codec dict for --compressor NAME (excluding the bytes
    preamble codec; None if unknown)."""
    if name == "blosc":
        return {
            "name": "blosc",
            "configuration": {
                "typesize": int(dtype_size),
                "cname": "zstd",
                "clevel": 5,
                "shuffle": "shuffle",
                "blocksize": 0,
            },
        }
    if name == "gzip":
        return {"name": "gzip", "configuration": {"level": 5}}
    if name == "zstd":
        return {"name": "zstd", "configuration": {"level": 5, "checksum": False}}
    return None


def _compressor_name_kwargs(name, out_format, dtype_size):
    """zarr.open kwargs / N5 compression for --compressor NAME (no sharding).
    Mirrors `apply_compressor_by_name` in tsio.cpp."""
    if out_format == "zarr3":
        codecs = [{"name": "bytes", "configuration": {"endian": "little"}}]
        c = _compressor_v3_by_name(name, dtype_size)
        if c is not None:
            codecs.append(c)
        return {"codecs": codecs}
    if out_format == "n5":
        if name == "none":
            return {"n5_compression": {"type": "raw"}}
        return {"n5_compression": _n5_compression_from_v2(_compressor_v2_by_name(name))}
    if name == "none":
        return {"compressor": None}
    return {"compressor": _compressor_v2_by_name(name)}


_N5_DATA_TYPES = {
    "uint8", "uint16", "uint32", "uint64",
    "int8", "int16", "int32", "int64",
    "float32", "float64",
}

# Attribute keys reserved by the N5 format (they expect arrays, unlike the
# Zarr attribute of the same name) - they are NOT copied to N5.
_N5_RESERVED_ATTRS = {
    "dimensions", "blockSize", "dataType", "compression",
    "axes", "units", "resolution",
}


def _n5_extra_attrs(attrs, dims):
    """Additional N5 attributes for attributes.json: the source attributes
    minus the N5-reserved keys, plus `_ARRAY_DIMENSIONS`.  Mirrors
    `n5_extra_attrs` in tsio.cpp."""
    extra = {k: v for k, v in attrs.items() if k not in _N5_RESERVED_ATTRS}
    if dims:
        extra.setdefault("_ARRAY_DIMENSIONS", dims)
    return extra


def _n5_data_type(dtype):
    """N5 `dataType` string for a numpy dtype (None if N5 cannot store it)."""
    s = np.dtype(dtype).name
    return s if s in _N5_DATA_TYPES else None


def _n5_compression_from_v2(comp):
    """Map a Zarr 2 compressor dict (or None) to the N5 `compression` object.
    Unknown or missing compressors fall back to raw, like the other
    cross-format conversions.  Mirrors `v2_compressor_to_n5` in tsio.cpp."""
    if not comp or not isinstance(comp, dict):
        return {"type": "raw"}
    cid = comp.get("id")
    if cid == "gzip":
        return {"type": "gzip", "level": comp.get("level", -1), "useZlib": False}
    if cid == "zlib":
        return {"type": "gzip", "level": comp.get("level", -1), "useZlib": True}
    if cid == "zstd":
        return {"type": "zstd", "level": comp.get("level", 0)}
    if cid == "blosc":
        return {
            "type": "blosc",
            "cname": comp.get("cname", "lz4"),
            "clevel": comp.get("clevel", 5),
            "shuffle": comp.get("shuffle", 0),
            "blocksize": comp.get("blocksize", 0),
        }
    return {"type": "raw"}


def _n5_codec(comp):
    """numcodecs codec matching an N5 `compression` object (None = raw)."""
    import numcodecs

    if not isinstance(comp, dict):
        return None
    cid = comp.get("type")
    if cid == "gzip":
        if comp.get("useZlib"):
            return numcodecs.Zlib(level=comp.get("level", -1))
        return numcodecs.GZip(level=comp.get("level", -1))
    if cid == "zstd":
        return numcodecs.Zstd(level=comp.get("level", 0))
    if cid == "blosc":
        return numcodecs.Blosc(
            cname=comp.get("cname", "lz4"),
            clevel=comp.get("clevel", 5),
            shuffle=comp.get("shuffle", 0),
            blocksize=comp.get("blocksize", 0),
        )
    return None


class _N5Out:
    """Minimal N5 dataset writer (zarr-python has no N5 support): writes
    `attributes.json` plus the standard chunk files, each with the N5 header
    (mode 0, rank, block size - all uint big endian) followed by the raw
    big-endian, Fortran-order block data, optionally compressed.  Values match
    the TensorStore N5 driver; the chunk file bytes need not be identical."""

    def __init__(self, base, shape, chunks, dtype, compression, attrs):
        import json

        self.base = base
        self.shape = tuple(int(s) for s in shape)
        self.chunks = tuple(int(c) for c in chunks)
        self.dtype = np.dtype(dtype)
        self.codec = _n5_codec(compression)
        data_type = _n5_data_type(self.dtype)
        if data_type is None:
            raise ValueError(
                f"data type '{self.dtype.name}' is not supported for N5 output"
            )
        os.makedirs(base, exist_ok=True)
        meta = {
            "dimensions": list(self.shape),
            "blockSize": list(self.chunks),
            "dataType": data_type,
            "compression": compression,
        }
        if attrs:
            meta.update(attrs)
        with open(os.path.join(base, "attributes.json"), "w") as f:
            json.dump(meta, f)

    def __setitem__(self, sl, arr):
        arr = np.asarray(arr, dtype=self.dtype)
        rank = len(self.shape)
        if sl is Ellipsis:
            sl = [slice(None)] * rank
        else:
            if sl == (Ellipsis,):
                sl = [slice(None)] * rank
            else:
                sl = list(sl)
        while len(sl) < rank:
            sl.append(slice(None))
        ranges = []
        for s, ext in zip(sl, arr.shape):
            if isinstance(s, slice):
                ranges.append(
                    (
                        0 if s.start is None else int(s.start),
                        int(ext) if s.stop is None else int(s.stop),
                    )
                )
            else:
                v = int(s)
                ranges.append((v, v + 1))
        been = self.dtype.newbyteorder(">")
        grid = tuple(int(-(-s // c)) for s, c in zip(self.shape, self.chunks))
        for ci in np.ndindex(*grid):
            d0 = tuple(ci[d] * self.chunks[d] for d in range(rank))
            reg = tuple(
                (d0[d], min(d0[d] + self.chunks[d], self.shape[d]))
                for d in range(rank)
            )
            ov = tuple(
                (max(reg[d][0], ranges[d][0]), min(reg[d][1], ranges[d][1]))
                for d in range(rank)
            )
            if any(o[1] <= o[0] for o in ov):
                continue
            win = tuple(
                slice(ov[d][0] - ranges[d][0], ov[d][1] - ranges[d][0])
                for d in range(rank)
            )
            dst = tuple(
                slice(o[0] - d0[d], o[1] - d0[d]) for d, o in enumerate(ov)
            )
            blk = self._read_block(ci)
            blk[dst] = arr[win]
            import struct

            hdr = struct.pack(">HH", 0, rank) + struct.pack(
                ">" + "I" * rank, *self.chunks
            )
            payload = blk.astype(been).tobytes(order="F")
            if self.codec is not None:
                payload = self.codec.encode(payload)
            path = os.path.join(self.base, "/".join(str(int(x)) for x in ci))
            os.makedirs(os.path.dirname(path), exist_ok=True)
            with open(path, "wb") as f:
                f.write(hdr + payload)

    def _read_block(self, ci):
        """Current block content of chunk `ci` (zeros-padded to the full block
        size) so that partial writes merge instead of overwriting the other
        cells of the chunk."""
        import struct

        path = os.path.join(self.base, "/".join(str(int(x)) for x in ci))
        blk = np.zeros(self.chunks, dtype=self.dtype)
        if not os.path.exists(path):
            return blk
        with open(path, "rb") as f:
            data = f.read()
        rank = len(self.chunks)
        hsz = 2 + 2 + 4 * rank
        if len(data) < hsz:
            return blk
        dims = struct.unpack(">" + "I" * rank, data[4:hsz])
        payload = data[hsz:]
        if self.codec is not None:
            payload = self.codec.decode(payload)
        have = np.frombuffer(
            payload, dtype=self.dtype.newbyteorder(">")
        ).reshape(dims, order="F").astype(self.dtype)
        sl = tuple(slice(0, min(d, c)) for d, c in zip(dims, self.chunks))
        blk[sl] = have[sl]
        return blk


def _out_dtype_size(out_dtype):
    """Element size in bytes of an output data type (for the copied blosc
    `typesize`): float16/bfloat16 = 2, float64 = 8, else 4.  Mirrors
    `out_dtype_size` in tsio.cpp."""
    return {"float16": 2, "bfloat16": 2, "float64": 8}.get(out_dtype, 4)


def _out_dtype_np(out_dtype):
    """numpy dtype for a zarr3 output data type ('bfloat16' needs ml_dtypes)."""
    if out_dtype == "bfloat16":
        import ml_dtypes

        return ml_dtypes.bfloat16
    return {"float64": np.float64, "float32": np.float32, "float16": np.float16}[
        out_dtype
    ]


def _drop_index_location(value):
    """Recursively remove the (non-normalized) `index_location` key from a
    copied Zarr 3 codec JSON structure."""
    if isinstance(value, dict):
        out = {}
        for k, v in value.items():
            if k == "index_location":
                continue
            out[k] = _drop_index_location(v)
        return out
    if isinstance(value, list):
        return [_drop_index_location(v) for v in value]
    return value


class _V3Bf16Out:
    """Hand-rolled Zarr 3 store for bfloat16 DATA arrays (zarr-python 3.0.x
    cannot create/read bfloat16 arrays).  Mirrors the C++/TensorStore output:
    the zarr.json is written in TensorStore-normalized form (a sharding
    `index_location` is implicit = "end", `chunk_key_encoding: default`) and
    the chunk bytes are produced by running the copied source codecs
    (bytes/blosc/zlib/gzip/zstd, plus `sharding_indexed` with a crc32c index).
    Values are converted float32 -> bfloat16 with the same rounding
    (round-to-nearest-even) as the C++ tool."""

    _SHUFFLE = {"noshuffle": 0, "shuffle": 1, "bitshuffle": 2}

    def __init__(self, path, shape, chunks, codecs, attrs, dims):
        import json as _json
        import ml_dtypes

        self._path = path
        self._dtype = np.dtype(ml_dtypes.bfloat16)
        self.shape = tuple(shape)
        self.chunks = tuple(chunks)
        # TensorStore normalizes the sharding `index_location` away (it is
        # "end" by default); drop it so the zarr.json matches the C++ output.
        self._codecs = _drop_index_location(codecs)
        self._inner = None
        for c in self._codecs:
            if isinstance(c, dict) and c.get("name") == "sharding_indexed":
                self._inner = c.get("configuration") or {}
        os.makedirs(path, exist_ok=True)
        md = {
            "zarr_format": 3,
            "node_type": "array",
            "shape": list(shape),
            "data_type": "bfloat16",
            "chunk_grid": {
                "name": "regular",
                "configuration": {"chunk_shape": list(chunks)},
            },
            "codecs": self._codecs,
            "fill_value": 0.0,
            "chunk_key_encoding": {"name": "default"},
        }
        if attrs:
            md["attributes"] = attrs
        if dims:
            md["dimension_names"] = dims
        with open(os.path.join(path, "zarr.json"), "w") as f:
            _json.dump(md, f)
        self._pending = {}  # outer chunk index -> `_PendingShard`

    class _PendingShard:
        __slots__ = ("parts", "buf")

    @staticmethod
    def _slice_box(sl, shape):
        starts, stops = [], []
        for d, s in enumerate(sl):
            if not isinstance(s, slice) or s.step not in (None, 1):
                raise NotImplementedError(f"unsupported slice {s!r}")
            starts.append(0 if s.start is None else max(0, s.start))
            stops.append(shape[d] if s.stop is None else min(shape[d], s.stop))
        return starts, stops

    def _codec_obj(self, codec, elem_size):
        from numcodecs import Blosc, GZip, Zlib, Zstd

        conf = codec.get("configuration") or {}
        n = codec.get("name")
        if n == "blosc":
            return Blosc(
                cname=conf.get("cname", "lz4"),
                clevel=conf.get("clevel", 5),
                shuffle=self._SHUFFLE.get(conf.get("shuffle", "shuffle"), 0),
                blocksize=conf.get("blocksize", 0),
                typesize=int(conf.get("typesize", elem_size)),
            )
        if n == "zlib":
            return Zlib(level=conf.get("level", -1))
        if n == "gzip":
            return GZip(level=conf.get("level", -1))
        if n == "zstd":
            return Zstd(level=conf.get("level", 0))
        return None

    def _encode_arr(self, arr, codecs, elem_size):
        import struct

        data = arr.tobytes()  # bfloat16 is little-endian native
        for c in codecs:
            n = c.get("name")
            conf = c.get("configuration") or {}
            if n == "bytes":
                if conf.get("endian", "little") != "little":
                    raise NotImplementedError("bytes codec with big endian")
                continue
            if n in ("crc32c", "crc32c_no_separate_header"):
                from crc32c import crc32c

                data += struct.pack("<I", crc32c(data))
                continue
            obj = self._codec_obj(c, elem_size)
            if obj is None:
                raise NotImplementedError(f"codec {n}")
            data = obj.encode(data)
        return data

    def _chunk_key(self, oc):
        return os.path.join(self._path, "c", *map(str, oc))

    def __setitem__(self, sl, value):
        value = np.asarray(value)
        if value.dtype != self._dtype:
            value = value.astype(self._dtype)
        starts, stops = self._slice_box(sl, self.shape)
        rank = len(self.shape)
        outer = [int(np.ceil(self.shape[d] / self.chunks[d])) for d in range(rank)]
        for oc in np.ndindex(*outer):
            oc0 = [oc[d] * self.chunks[d] for d in range(rank)]
            oc1 = [min(oc0[d] + self.chunks[d], self.shape[d]) for d in range(rank)]
            if any(stops[d] <= oc0[d] or starts[d] >= oc1[d] for d in range(rank)):
                continue
            if self._inner is not None:
                ic = [int(x) for x in self._inner["chunk_shape"]]
                nic = [int(np.ceil((oc1[d] - oc0[d]) / ic[d])) for d in range(rank)]
                pend = self._pending.get(oc)
                if pend is None:
                    pend = self._pending[oc] = self._PendingShard()
                    pend.parts = [None] * int(np.prod(nic))
                inner_codes = self._inner.get("codecs") or [
                    {"name": "bytes", "configuration": {"endian": "little"}}
                ]
                for j in np.ndindex(*nic):
                    jc0 = [oc0[d] + j[d] * ic[d] for d in range(rank)]
                    jc1 = [min(jc0[d] + ic[d], self.shape[d]) for d in range(rank)]
                    if not all(
                        stops[d] > jc0[d] and starts[d] < jc1[d] for d in range(rank)
                    ):
                        continue  # already filled by another window write
                    arr = np.zeros([jc1[d] - jc0[d] for d in range(rank)], dtype=self._dtype)
                    arr[self._overlay_sl(jc0, jc1, starts, stops)] = value[
                        self._overlay_vs(jc0, jc1, starts, stops)
                    ]
                    pend.parts[sum(j[d] * int(np.prod(nic[d + 1 :])) for d in range(rank))] = (
                        self._encode_arr(arr, inner_codes, 2)
                    )
            else:
                pend = self._pending.get(oc)
                if pend is None:
                    pend = self._pending[oc] = self._PendingShard()
                    pend.buf = np.zeros(
                        [oc1[d] - oc0[d] for d in range(rank)], dtype=self._dtype
                    )
                pend.buf[self._overlay_sl(oc0, oc1, starts, stops)] = value[
                    self._overlay_vs(oc0, oc1, starts, stops)
                ]
        self._flush_past(starts[0])

    @staticmethod
    def _overlay_sl(lo, hi, starts, stops):
        sl = []
        for d in range(len(lo)):
            a = max(starts[d], lo[d])
            b = min(stops[d], hi[d])
            sl.append(slice(a - lo[d], b - lo[d]))
        return tuple(sl)

    @staticmethod
    def _overlay_vs(lo, hi, starts, stops):
        vs = []
        for d in range(len(lo)):
            a = max(starts[d], lo[d])
            b = min(stops[d], hi[d])
            vs.append(slice(a - starts[d], b - starts[d]))
        return tuple(vs)

    def _flush_past(self, t0):
        for oc in list(self._pending):
            oc1 = min((oc[0] + 1) * self.chunks[0], self.shape[0])
            if oc1 <= t0:
                self._write_pending(oc)

    def _write_pending(self, oc):
        import itertools
        import struct

        pend = self._pending.get(oc)
        if pend is None:
            return
        if self._inner is None:
            self._write_chunk(oc, self._encode_arr(pend.buf, self._codecs, 2))
            self._pending.pop(oc, None)
            return
        parts = pend.parts
        inner = self._inner
        index_codes = inner.get("index_codecs") or [
            {"name": "bytes", "configuration": {"endian": "little"}},
            {"name": "crc32c"},
        ]
        buf = b"".join(p for p in parts if p is not None)
        table = struct.pack(
            "<%dQ" % (2 * len(parts)),
            *itertools.chain.from_iterable(self._cum(parts)),
        )
        index = table
        for c in index_codes:
            n = c.get("name")
            if n == "bytes":
                continue
            if n in ("crc32c", "crc32c_no_separate_header"):
                from crc32c import crc32c

                index += struct.pack("<I", crc32c(index))
                continue
            raise NotImplementedError(f"index codec {n}")
        self._write_chunk(oc, buf + index)
        self._pending.pop(oc, None)

    @staticmethod
    def _cum(parts):
        off = 0
        for p in parts:
            yield off, len(p) if p is not None else 0
            off += len(p) if p is not None else 0

    def _write_chunk(self, oc, data):
        out = self._chunk_key(oc)
        os.makedirs(os.path.dirname(out), exist_ok=True)
        with open(out, "wb") as f:
            f.write(data)

    def _flush(self):
        for oc in list(self._pending):
            self._write_pending(oc)


def _out_chunks(cfg, shapes, bc, time_constant=False):
    """Output chunk shape for --out-path: source physical chunks clamped to the
    selected extent, with --step-chunk (dim 0) and --cell-chunk (last dim)
    overrides (also clamped to the extent).  Mirrors tsResolveArray in
    tsio.cpp."""
    nc = [max(1, min(int(c), int(b))) for c, b in zip(shapes, bc)]
    if cfg.step_chunk and not time_constant:
        nc[0] = max(1, min(int(cfg.step_chunk), int(bc[0])))
    if cfg.cell_chunk:
        nc[-1] = max(1, min(int(cfg.cell_chunk), int(bc[-1])))
    return tuple(nc)


def print_means_table(cfg, items, coords=()):
    """Print the preamble table (one row per selected array) and then, for
    every timestep, one row per (array, layer):
    `<t>  <layer>  <count>  <missing>  <min>  <mean>  <max>  <array>` where
    `count` is the number of values of the layer, `missing` the number of
    invalid (NaN) values and `min`/`mean`/`max` (C format "%12.5g") are over
    the valid values.  Arrays without layers use layer index 0.  All arrays
    stay open and are reduced together t-major, each with its OWN window cache,
    so peak memory equals the sum of all array caches.

    With `cfg.out_path` set, only the preamble is printed and the selected
    data is written to a NEW Zarr store at that path instead (one float32 array
    per selected data array, using the same window loops)."""
    print("=== Selected arrays ===")
    head = "#  name  format  kvstore  dtype  compressor  fill_value  shape  chunks  indices"
    if cfg.out_path:
        head += "  out_chunks"
    print(head)
    engs = []
    for idx, (name, z, kvstore, fmt) in enumerate(items, start=1):
        shape_s = f"[{', '.join(str(s) for s in z.shape)}]"
        dtype_s = str(z.dtype)
        # Physical (outer) chunk shape: shard shape for sharded Zarr 3,
        # otherwise the chunk grid - matches the C++ tool's write_chunk_shape().
        chunk_shape = getattr(z, "shards", None) or z.chunks
        chunk_s = f"[{', '.join(str(s) for s in chunk_shape)}]"
        eng = _MeanEngine(
            z,
            name,
            cfg.step,
            cfg.layer,
            cfg.cell,
        )
        eng.fmt = fmt
        # Selected indices per dimension (clamped to the domain), first and
        # last 1-based index separated by ':', e.g. all of [416, 7, 768] ->
        # [1:416, 1:7, 1:768].
        idx_s = f"[{', '.join(f'{eng.bs[d] + 1}:{eng.bs[d] + eng.bc[d]}' for d in range(len(eng.bs)))}]"
        # In --out-path mode the OUTPUT chunk size (source chunk shape clamped
        # to the selected hypercube) is appended at the end of the line.
        out_s = ""
        if cfg.out_path:
            ochunks = _out_chunks(cfg, chunk_shape, eng.bc, eng.time_constant)
            out_s = f"  [{', '.join(str(s) for s in ochunks)}]"
        print(
            f"{idx}  {name}  zarr{fmt}  {kvstore}  {dtype_s}  {_source_compressor_name(z, fmt)}  {_source_fill_value(cfg, name, z)}  {shape_s}  {chunk_s}  {idx_s}{out_s}"
        )
        engs.append(eng)

    # Table of the coordinate arrays referenced by the selected data arrays'
    # dimension names (derived from the metadata by the caller).  Coordinate
    # arrays usually hold one coordinate value per timestep/grid point.  Each
    # row is `idx  name  shape`, followed - for a grid_mapping coordinate such
    # as "crs" - by its `grid_mapping=`/`refinement_level=`/`indexing_scheme=`
    # attributes.
    print("=== Coordinate arrays ===")
    print("#  name  shape")
    for cidx, (cname, cshape, cdetails) in enumerate(coords, start=1):
        suffix = f"  {cdetails}" if cdetails else ""
        print(f"{cidx}  {cname}  {cshape}{suffix}")

    if cfg.info:
        return True

    # --out-path mode: create one output store per selected data array (same
    # selected hypercube as the tables), then run the same window loops but
    # write the data instead of printing min/mean/max.
    targets = None
    if cfg.out_path:
        import zarr

        os.makedirs(cfg.out_path, exist_ok=True)
        targets = {}
        for eng in engs:
            # Physical (outer) chunk shape (shard shape), clamped to the
            # selected extent - matches the C++ tool's output chunking.
            shapes = getattr(eng.z, "shards", None) or eng.z.chunks
            ochunks = _out_chunks(cfg, shapes, eng.bc, eng.time_constant)
            src_attrs, src_dims = _source_metadata(eng.z, eng.fmt)
            src_comp = _source_compression(eng.z, eng.fmt)
            comp_kwargs = (
                _compressor_name_kwargs(
                    cfg.compressor, cfg.out_format, _out_dtype_size(cfg.out_dtype)
                )
                if cfg.compressor
                else (
                    _out_compression_kwargs(
                        eng.fmt,
                        src_comp,
                        cfg.out_format,
                        _out_dtype_size(cfg.out_dtype),
                        ochunks,
                    )
                    if cfg.out_format in ("zarr2", "zarr3")
                    else {}
                )
            )
            if cfg.out_format == "n5":
                if comp_kwargs:
                    n5_comp = comp_kwargs["n5_compression"]
                else:
                    src_comp_v2 = (
                        src_comp if eng.fmt == 2 else _first_v3_compressor(src_comp)
                    )
                    n5_comp = _n5_compression_from_v2(src_comp_v2)
                out = _N5Out(
                    os.path.join(cfg.out_path, eng.name),
                    tuple(eng.bc),
                    ochunks,
                    np.float32,
                    n5_comp,
                    _n5_extra_attrs(src_attrs, src_dims),
                )
            elif cfg.out_format == "zarr3" and cfg.out_dtype == "bfloat16":
                out = _V3Bf16Out(
                    os.path.join(cfg.out_path, eng.name),
                    tuple(eng.bc),
                    ochunks,
                    comp_kwargs["codecs"],
                    src_attrs,
                    src_dims,
                )
            else:
                dtype_np = (
                    _out_dtype_np(cfg.out_dtype)
                    if cfg.out_format == "zarr3"
                    else np.float32
                )
                out = zarr.open(
                    os.path.join(cfg.out_path, eng.name),
                    mode="w",
                    shape=tuple(eng.bc),
                    chunks=ochunks,
                    dtype=dtype_np,
                    zarr_format={"zarr2": 2, "zarr3": 3}[cfg.out_format],
                    **_out_meta_kwargs(cfg.out_format, src_attrs, src_dims),
                    **comp_kwargs,
                )
            targets[eng.name] = out
            eng.raw = True
            eng._out_win = -1

    if targets is None:
        print("# timestep  layer  count  missing  min  mean  max  array")

    static_engs = [e for e in engs if e.time_constant]
    dynamic_engs = [e for e in engs if not e.time_constant]

    # Time-constant arrays (no "time" dimension) are treated as fields of the
    # first timestep only, appended after the time-varying arrays, so the
    # time-varying arrays keep the same field index in every step (mirrors the
    # field list in tsio.cpp::tsOpenFields).
    first_at = min((e.bs[0] for e in dynamic_engs), default=0)
    max_at = max((e.bs[0] + e.bc[0] for e in dynamic_engs), default=first_at + 1)
    for at in range(first_at, max_at):
        for e in dynamic_engs:
            if not (e.bs[0] <= at < e.bs[0] + e.bc[0]):
                continue
            e.ensure(at)
            data = e.wcache
            name = e.name

            if targets is not None:
                # Write the window slice to the output store once per window
                # (same window loop as the statistics pass, t-major).
                if e._out_win != e.wcache_win:
                    pt = e.wcache_t0 - e.bs[0]
                    nw = e.wcache.shape[0]
                    sl = (slice(pt, pt + nw),) + tuple(slice(0, b) for b in e.bc[1:])
                    targets[name][sl] = e.wcache
                    e._out_win = e.wcache_win
                continue

            lt = at - e.wcache_t0
            total = e.bc[-1]  # number of values of one layer
            if not e.layered:
                row = data[lt]
                valid = ~np.isnan(row)
                nvalid = int(valid.sum())
                if nvalid == 0:
                    print(
                        f"{at + 1}  0  {total}  {total - nvalid}  NaN  NaN  NaN  {name}"
                    )
                else:
                    rv = row[valid]
                    print(
                        f"{at + 1}  0  {total}  {total - nvalid}  {float(rv.min()):12.5g}  {float(rv.mean()):12.5g}  {float(rv.max()):12.5g}  {name}"
                    )
            else:
                # One line per layer: statistics over the last (grid) dim only.
                # Layers start at bs[1] (absolute, 1-based on output) and the
                # last layer dim varies fastest (row-major).
                for lc, li in enumerate(np.ndindex(*(e.bc[1:-1]))):
                    lay = e.bs[1] + li[0]
                    row = data[lt][li]
                    valid = ~np.isnan(row)
                    nvalid = int(valid.sum())
                    if nvalid == 0:
                        print(
                            f"{at + 1}  {lay + 1}  {total}  {total - nvalid}  NaN  NaN  NaN  {name}"
                        )
                    else:
                        rv = row[valid]
                        print(
                            f"{at + 1}  {lay + 1}  {total}  {total - nvalid}  {float(rv.min()):12.5g}  {float(rv.mean()):12.5g}  {float(rv.max()):12.5g}  {name}"
                        )

        if at != first_at:
            continue
        # The time-constant arrays appear once, in the first timestep.  In write
        # mode the whole selected hypercube is written in one shot.
        for e in static_engs:
            sel = tuple(slice(e.bs[d], e.bs[d] + e.bc[d]) for d in range(len(e.bs)))
            data = np.asarray(e.z[sel], dtype=None if targets is not None else np.float64)
            if targets is not None:
                targets[e.name][tuple(slice(0, b) for b in e.bc)] = data
                continue
            total = e.bc[-1]
            rows = int(np.prod(e.bc[:-1])) if e.layered else 1
            flat = data.reshape((rows, total))
            for r in range(rows):
                row = flat[r]
                valid = ~np.isnan(row)
                nvalid = int(valid.sum())
                lay = e.bs[0] + r + 1 if e.layered else 0
                if nvalid == 0:
                    print(f"{at + 1}  {lay}  {total}  {total - nvalid}  NaN  NaN  NaN  {e.name}")
                else:
                    rv = row[valid]
                    print(
                        f"{at + 1}  {lay}  {total}  {total - nvalid}  {float(rv.min()):12.5g}  {float(rv.mean()):12.5g}  {float(rv.max()):12.5g}  {e.name}"
                    )

    if targets is not None:
        for out in targets.values():
            flush = getattr(out, "_flush", None)
            if flush:
                flush()
    return True


def _write_coord_arrays(cfg, coord_items):
    """Write every used coordinate array (`coord_items`, i.e. exactly the rows
    of the `=== Coordinate arrays ===` table) into the --out-path store.  The
    dtype is preserved and, like the data arrays, the selected hypercube and a
    chunk shape clamped from the source physical chunks are used."""
    import zarr

    os.makedirs(cfg.out_path, exist_ok=True)
    for cname, cz, _kv, fmt in coord_items:
        e = _MeanEngine(
            cz,
            cname,
            cfg.step,
            cfg.layer,
            cfg.cell,
        )
        # A time-constant rank-1 coordinate whose single dimension is not the
        # cell dimension (e.g. crs/ocean_model_level) is not clipped.  Mirrors
        # tsApplyCoordinateConfig in tsio.cpp.
        if e.time_constant and len(e.bs) == 1 and _array_dims(cz) != ["cell"]:
            e.bs[0] = 0
            e.bc[0] = cz.shape[0]
        bs, bc = e.bs, e.bc
        sel = tuple(slice(bs[d], bs[d] + bc[d]) for d in range(len(bs)))
        shapes = getattr(cz, "shards", None) or cz.chunks
        ochunks = _out_chunks(cfg, shapes, bc, e.time_constant)
        src_attrs, src_dims = _source_metadata(cz, fmt)
        src_comp = _source_compression(cz, fmt)
        dtype_size = np.dtype(cz.dtype).itemsize
        comp_kwargs = (
            _compressor_name_kwargs(cfg.compressor, cfg.out_format, dtype_size)
            if cfg.compressor
            else (
                _out_compression_kwargs(fmt, src_comp, cfg.out_format, dtype_size, ochunks)
                if cfg.out_format in ("zarr2", "zarr3")
                else {}
            )
        )
        if cfg.out_format == "n5":
            if comp_kwargs:
                n5_comp = comp_kwargs["n5_compression"]
            else:
                src_comp_v2 = src_comp if fmt == 2 else _first_v3_compressor(src_comp)
                n5_comp = _n5_compression_from_v2(src_comp_v2)
            out = _N5Out(
                os.path.join(cfg.out_path, cname),
                tuple(bc),
                ochunks,
                cz.dtype,
                n5_comp,
                _n5_extra_attrs(src_attrs, src_dims),
            )
        else:
            out = zarr.open(
                os.path.join(cfg.out_path, cname),
                mode="w",
                shape=tuple(bc),
                chunks=ochunks,
                dtype=cz.dtype,
                zarr_format={"zarr2": 2, "zarr3": 3}[cfg.out_format],
                **_out_meta_kwargs(cfg.out_format, src_attrs, src_dims),
                **comp_kwargs,
            )
        out[...] = np.asarray(cz[sel])
    return True


def detect_format(store, array_name):
    """Return the Zarr format (2 or 3) or None, based on metadata files."""
    if isinstance(store, str):
        for fmt, key in ((3, "zarr.json"), (2, ".zarray")):
            if os.path.exists(os.path.join(store, array_name, key)):
                return fmt
    else:
        for fmt, key in ((3, "zarr.json"), (2, ".zarray")):
            try:
                if f"{array_name}/{key}" in store:
                    return fmt
            except Exception:
                pass
    return None


def _read_file_or_url(url_or_path: str) -> bytes:
    """Read an entire file from either a local path or an http(s) URL."""
    if url_or_path.startswith(("http://", "https://")):
        with urllib.request.urlopen(url_or_path, timeout=60) as resp:
            return resp.read()
    with open(url_or_path, "rb") as fh:
        return fh.read()


def _discover_array_names(cfg):
    """Return the sorted list of direct array children of the dataset root.

    Consolidated `.zmetadata` (if present) enumerates every child and also
    works for http stores that do not support directory listing; otherwise the
    filesystem is scanned for `.zarray` / `zarr.json` markers.
    """
    import json

    kvstore, path, endpoint = resolve_kvstore(cfg)
    names = set()

    try:
        raw = _read_file_or_url(path.rstrip("/") + "/.zmetadata")
        meta = json.loads(raw).get("metadata", {})
        for key in meta:
            slash = key.find("/")
            if slash <= 0:
                continue
            head, tail = key[:slash], key[slash + 1 :]
            if tail in (".zarray", "zarr.json"):
                names.add(head)
        if names:
            return sorted(names)
    except Exception:
        pass

    if kvstore == "file":
        for entry in sorted(os.listdir(path)):
            full = os.path.join(path, entry)
            if not os.path.isdir(full):
                continue
            if any(
                os.path.exists(os.path.join(full, k)) for k in (".zarray", "zarr.json")
            ):
                names.add(entry)
    return sorted(names)


def _raw_metadata(cfg, name):
    """Return (meta_dict, attrs_dict, fmt) for one array, reading the raw
    metadata files.  attrs_dict is None for zarr3 (its `attributes` live inside
    the zarr.json meta).  Invalid bare NaN/Infinity tokens are sanitized."""
    import json

    kvstore, path, endpoint = resolve_kvstore(cfg)

    if cfg.consolidated:
        raw = _read_consolidated_zmetadata(path, name)
        meta = json.loads(raw[".zarray"])
        attrs = json.loads(raw[".zattrs"]) if ".zattrs" in raw else {}
        return meta, attrs, 2

    if kvstore == "file":
        base = os.path.join(path, name)
        if os.path.exists(os.path.join(base, ".zarray")):
            meta = json.loads(open(os.path.join(base, ".zarray"), "rb").read())
            attrs = json.loads(open(os.path.join(base, ".zattrs"), "rb").read())
            return meta, attrs, 2
        if os.path.exists(os.path.join(base, "zarr.json")):
            return (
                json.loads(open(os.path.join(base, "zarr.json"), "rb").read()),
                None,
                3,
            )
        raise RuntimeError("no per-array metadata found")

    if kvstore == "http":
        scheme = path.find("://")
        slash = path.find("/", scheme + 3)
        base_url = path[:slash]
        group_dir = path[slash + 1 :]
        array_dir = f"{group_dir}/{name}" if group_dir else name
        remote = _RemoteHTTPStore(base_url, array_dir)
        meta_raw = remote._fetch(".zarray")
        if meta_raw is not None:
            meta = json.loads(meta_raw)
            attrs_raw = remote._fetch(".zattrs")
            attrs = json.loads(attrs_raw) if attrs_raw is not None else {}
            return meta, attrs, 2
        zraw = remote._fetch("zarr.json")
        if zraw is not None:
            return json.loads(zraw), None, 3
        raise RuntimeError("no per-array metadata found")

    raise RuntimeError(f"overview not supported for kvstore '{kvstore}'")


def _zarr3_chunks(meta):
    """The read chunk shape of a zarr3 array: the shard chunk shape when the
    codecs use `sharding_indexed`, otherwise the chunk_grid chunk shape.
    Mirrors TensorStore's read_chunk_shape() / C++ overview."""
    for codec in meta.get("codecs", []):
        if isinstance(codec, dict) and codec.get("name") == "sharding_indexed":
            conf = codec.get("configuration", {})
            if conf.get("chunk_shape"):
                return list(conf["chunk_shape"])
    return list(
        meta.get("chunk_grid", {}).get("configuration", {}).get("chunk_shape", [])
    )


def _zarr3_codec_names(codecs):
    """Names of the compression codecs in a zarr3 `codecs` list, recursing into
    nested sharding configurations.  Structural codecs are excluded; multiple
    compressors are joined with '+'.  Mirrors the C++ overview."""
    structural = {
        "bytes",
        "crc32c",
        "crc32c_no_separate_header",
        "transpose",
        "bitshuffle",
        "delta",
        "pcodec",
        "sharding_indexed",
    }
    found = set()

    def walk(items):
        if not isinstance(items, list):
            return
        for codec in items:
            if not isinstance(codec, dict):
                continue
            name = codec.get("name")
            if name and name not in structural:
                found.add(name)
            conf = codec.get("configuration")
            if isinstance(conf, dict):
                for key in ("codecs", "index_codecs"):
                    walk(conf.get(key))

    walk(codecs)
    return "+".join(sorted(found))


def _source_compressor_name(z, fmt):
    """Name of the source compressor of an open array ("-" when none/unknown).
    Zarr 2: compressor `id`; Zarr 3: `_zarr3_codec_names` of the codecs list
    (recursing through sharding).  Mirrors `array_compressor` in tsio.cpp."""
    comp = _source_compression(z, fmt)
    if fmt == 2:
        return comp.get("id") if isinstance(comp, dict) else "-"
    return _zarr3_codec_names(comp) or "-"


def _fill_value_text(value):
    """Table text for a raw `fill_value` JSON scalar ("-" for null/missing)."""
    import json

    if value is None:
        return "-"
    if isinstance(value, bool):
        return "true" if value else "false"
    if isinstance(value, str):
        return value
    if isinstance(value, float) and not math.isfinite(value):
        if math.isnan(value):
            return "NaN"
        return "Infinity" if value > 0 else "-Infinity"
    return json.dumps(value)


def _source_fill_value(cfg, name, z):
    """Raw source `fill_value` of an open array, formatted for the table.
    Read from the raw metadata so it is not dtype-normalized (mirrors
    `array_fill_value` in tsio.cpp); falls back to the parsed metadata when the
    raw metadata is not directly reachable (e.g. s3)."""
    kvstore, path, _endpoint = resolve_kvstore(cfg)
    try:
        if (cfg.consolidated or cfg.group) and kvstore in ("file", "http"):
            meta = _read_root_zarr3_consolidated(path)
            key = f"{cfg.group}/{name}" if cfg.group else name
            node = meta.get(key)
            if isinstance(node, dict):
                return _fill_value_text(node.get("fill_value"))
    except Exception:
        pass
    try:
        meta, _attrs, _fmt = _raw_metadata(cfg, name)
        if isinstance(meta, dict):
            return _fill_value_text(meta.get("fill_value"))
    except Exception:
        pass
    value = getattr(z.metadata, "fill_value", None)
    if hasattr(value, "item"):
        value = value.item()
    return _fill_value_text(value)


def _overview_fields(meta, attrs, fmt):
    """Return (dtype, shape, chunks, compressor, units) for one array."""
    if fmt == 2:
        dtype = np.dtype(meta["dtype"]).name
        shape = meta["shape"]
        chunks = meta["chunks"]
        compressor_obj = meta.get("compressor")
        compressor = (
            compressor_obj.get("id") if isinstance(compressor_obj, dict) else None
        )
        units = (attrs or {}).get("units")
        return (
            str(dtype),
            shape,
            chunks,
            compressor or "-",
            "-" if units is None else str(units),
        )
    dtype = meta.setdefault("data_type", "-")
    shape = meta["shape"]
    chunks = _zarr3_chunks(meta)
    compressor = _zarr3_codec_names(meta.get("codecs", [])) or "-"
    attrs_obj = meta.get("attributes") or {}
    units = meta.get("units")
    if units is None:
        units = attrs_obj.get("units")
    return str(dtype), shape, chunks, compressor, "-" if units is None else str(units)


def _meta_dim_names(meta, attrs, fmt):
    """Dimension names of an array's metadata: Zarr 2 `_ARRAY_DIMENSIONS`
    attribute or Zarr 3 `dimension_names`.  Empty list when not defined."""
    if fmt == 2:
        return list((attrs or {}).get("_ARRAY_DIMENSIONS") or [])
    dn = meta.get("dimension_names")
    if dn:
        return list(dn)
    a = meta.get("attributes") or {}
    return list(a.get("_ARRAY_DIMENSIONS") or [])


def _meta_grid_mapping(meta, attrs, fmt):
    """The `grid_mapping` attribute of an array's metadata (None if unset)."""
    if fmt == 2:
        return (attrs or {}).get("grid_mapping")
    a = meta.get("attributes") or {}
    return a.get("grid_mapping")


def _coordinate_names(entries):
    """Set of coordinate-array names among `entries` (tuples of
    `(name, dim_names, grid_mapping)`).  An array is a coordinate when its name
    is the dimension name of some array or the `grid_mapping` target of some
    array."""
    names = set()
    for _name, dims, gm in entries:
        names.update(dims or ())
        if gm:
            names.add(gm)
    return names


def _discover_data_array_names(cfg):
    """Names of the DATA arrays below `--path` / `--group`: every array whose
    name is not a coordinate array.  An array is a coordinate when its name is
    the dimension name of some array or the `grid_mapping` target of some array
    (coordinate arrays are selected automatically alongside the data arrays)."""
    entries = []
    if cfg.group:
        kvstore, path, endpoint = resolve_kvstore(cfg)
        try:
            meta = _read_root_zarr3_consolidated(path)
            names = _consolidated_children(meta, cfg.group, want_groups=False)
            if not names:
                print(f"No arrays found in group '{cfg.group}'", file=sys.stderr)
                return []
            for name in names:
                key = f"{cfg.group}/{name}"
                node = meta.get(key)
                if node is None:
                    print(
                        f"Failed to read metadata for array '{name}' (missing '{key}')",
                        file=sys.stderr,
                    )
                    continue
                entries.append((name, node, None, 3))
        except Exception:
            meta = _read_root_zarr2_consolidated(path)
            names = _zarr2_children(meta, cfg.group, want_groups=False)
            if not names:
                print(f"No arrays found in group '{cfg.group}'", file=sys.stderr)
                return []
            for name in names:
                key = f"{cfg.group}/{name}"
                za = meta.get(f"{key}/.zarray")
                if za is None:
                    print(
                        f"Failed to read metadata for array '{name}' (missing '{key}/.zarray')",
                        file=sys.stderr,
                    )
                    continue
                entries.append((name, za, meta.get(f"{key}/.zattrs"), 2))
    else:
        # A root that contains groups is listed (groups win), exactly like the
        # dataset overview; only a root of plain arrays is selected.
        kvstore, path, endpoint = resolve_kvstore(cfg)
        try:
            root = _read_root_zarr3_consolidated(path)
            if _consolidated_children(root, "", want_groups=True):
                return []
        except Exception:
            try:
                root = _read_root_zarr2_consolidated(path)
                if _zarr2_children(root, "", want_groups=True):
                    return []
            except Exception:
                pass
        names = _discover_array_names(cfg)
        for name in names:
            try:
                meta, attrs, fmt = _raw_metadata(cfg, name)
                entries.append((name, meta, attrs, fmt))
            except Exception as e:
                print(f"Failed to open '{name}': {e}", file=sys.stderr)
    coord = _coordinate_names(
        [(n, _meta_dim_names(m, a, f), _meta_grid_mapping(m, a, f)) for n, m, a, f in entries]
    )
    return sorted(n for n, _m, _a, _f in entries if n not in coord)


def _print_consolidated_overview(meta: dict, prefix: str, names: list[str]) -> bool:
    """Print the overview table for Zarr 3 arrays served from a consolidated
    node map.  `prefix` is "" for the store root, or the group name.  Mirrors
    the C++ `print_overview_from_consolidated`.  Coordinate arrays are omitted."""
    ok = True
    rows = []
    for name in names:
        key = f"{prefix}/{name}" if prefix else name
        node = meta.get(key)
        if node is None:
            print(
                f"Failed to read metadata for array '{name}' (missing '{key}')",
                file=sys.stderr,
            )
            ok = False
            continue
        dtype, shape, chunks, compressor, units = _overview_fields(node, None, 3)
        rows.append(
            (
                name,
                dtype,
                shape,
                chunks,
                compressor,
                units,
                _meta_dim_names(node, None, 3),
                _meta_grid_mapping(node, None, 3),
            )
        )
    coord = _coordinate_names([(r[0], r[6], r[7]) for r in rows])
    data_rows = [r for r in rows if r[0] not in coord]
    if not data_rows:
        return False
    print("=== Dataset overview ===")
    print("#  Name  [Units]  DataType  Shape  Chunks  Compressor")
    for index, (name, dtype, shape, chunks, compressor, units, _dims, _gm) in enumerate(
        data_rows, start=1
    ):
        shape_s = "[" + ",".join(str(s) for s in shape) + "]"
        chunk_s = "[" + ",".join(str(s) for s in chunks) + "]"
        print(
            f"{index:>2}  {name}  [{units}]  {dtype}  {shape_s}  {chunk_s}  {compressor}"
        )
    return ok


def _print_zarr2_overview(meta: dict, prefix: str, names: list[str]) -> bool:
    """Print the overview table for Zarr 2 arrays served from a `.zmetadata`
    map.  `prefix` is "" for the store root, or the group name.  Mirrors the
    C++ `print_zarr2_overview_from_consolidated`.  Coordinate arrays are
    omitted."""
    ok = True
    rows = []
    for name in names:
        key = f"{prefix}/{name}" if prefix else name
        za = meta.get(f"{key}/.zarray")
        if za is None:
            print(
                f"Failed to read metadata for array '{name}' (missing '{key}/.zarray')",
                file=sys.stderr,
            )
            ok = False
            continue
        attrs = meta.get(f"{key}/.zattrs")
        dtype, shape, chunks, compressor, units = _overview_fields(za, attrs, 2)
        rows.append(
            (
                name,
                dtype,
                shape,
                chunks,
                compressor,
                units,
                _meta_dim_names(za, attrs, 2),
                _meta_grid_mapping(za, attrs, 2),
            )
        )
    coord = _coordinate_names([(r[0], r[6], r[7]) for r in rows])
    data_rows = [r for r in rows if r[0] not in coord]
    if not data_rows:
        return False
    print("=== Dataset overview ===")
    print("#  Name  [Units]  DataType  Shape  Chunks  Compressor")
    for index, (name, dtype, shape, chunks, compressor, units, _dims, _gm) in enumerate(
        data_rows, start=1
    ):
        shape_s = "[" + ",".join(str(s) for s in shape) + "]"
        chunk_s = "[" + ",".join(str(s) for s in chunks) + "]"
        print(
            f"{index:>2}  {name}  [{units}]  {dtype}  {shape_s}  {chunk_s}  {compressor}"
        )
    return ok


def print_dataset_overview(cfg):
    """When `--name` is omitted, print a one-line summary (Name, Units,
    DataType, Shape, Chunks, Compressor) for every array in the dataset.
    With `--group`, the summary is served from the consolidated root metadata
    (`zarr.json` for Zarr 3, `.zmetadata` for Zarr 2)."""
    if cfg.group:
        kvstore, path, endpoint = resolve_kvstore(cfg)
        try:
            meta = _read_root_zarr3_consolidated(path)
            arrays = _consolidated_children(meta, cfg.group, want_groups=False)
            if not arrays:
                print(f"No arrays found in group '{cfg.group}'", file=sys.stderr)
                return False
            return _print_consolidated_overview(meta, cfg.group, arrays)
        except Exception:
            meta = _read_root_zarr2_consolidated(path)
            arrays = _zarr2_children(meta, cfg.group, want_groups=False)
            if not arrays:
                print(f"No arrays found in group '{cfg.group}'", file=sys.stderr)
                return False
            return _print_zarr2_overview(meta, cfg.group, arrays)

    names = _discover_array_names(cfg)
    if not names:
        print(f"No zarr arrays found in dataset at: {cfg.path}", file=sys.stderr)
        return False

    ok = True
    rows = []
    for name in names:
        try:
            meta, attrs, fmt = _raw_metadata(cfg, name)
            dtype, shape, chunks, compressor, units = _overview_fields(meta, attrs, fmt)
            rows.append(
                (
                    name,
                    dtype,
                    shape,
                    chunks,
                    compressor,
                    units,
                    _meta_dim_names(meta, attrs, fmt),
                    _meta_grid_mapping(meta, attrs, fmt),
                )
            )
        except Exception as e:
            print(f"Failed to open '{name}': {e}", file=sys.stderr)
            ok = False
    coord = _coordinate_names([(r[0], r[6], r[7]) for r in rows])
    data_rows = [r for r in rows if r[0] not in coord]
    if not data_rows:
        return False
    print("=== Dataset overview ===")
    print("#  Name  [Units]  DataType  Shape  Chunks  Compressor")
    for index, (name, dtype, shape, chunks, compressor, units, _dims, _gm) in enumerate(
        data_rows, start=1
    ):
        shape_s = "[" + ",".join(str(s) for s in shape) + "]"
        chunk_s = "[" + ",".join(str(s) for s in chunks) + "]"
        print(
            f"{index:>2}  {name}  [{units}]  {dtype}  {shape_s}  {chunk_s}  {compressor}"
        )
    return ok


def _array_dims(z):
    """Dimension names of an array: Zarr 2 `_ARRAY_DIMENSIONS` attribute or
    Zarr 3 `dimension_names` metadata.  Empty list when not defined."""
    d = z.attrs.get("_ARRAY_DIMENSIONS") if z.attrs else None
    if d:
        return list(d)
    dn = getattr(getattr(z, "metadata", None), "dimension_names", None)
    if dn:
        return list(dn)
    return []


def _array_attrs(z):
    """Attributes of an array (Zarr 2 `.zattrs` / Zarr 3 `attributes` in
    zarr.json) as a dict of strings (numbers/booleans like str())."""
    out = {}
    try:
        a = dict(z.attrs) if z.attrs else {}
    except Exception:
        a = {}
    for k, v in a.items():
        if isinstance(v, str):
            out[k] = v
        elif isinstance(v, bool):
            out[k] = "true" if v else "false"
        elif isinstance(v, int):
            out[k] = str(v)
        elif isinstance(v, float):
            out[k] = repr(v)
        else:
            import json as _json

            out[k] = _json.dumps(v)
    return out


def process_array(cfg, array_name, quiet=False):
    """Open one selected array (with format/kvstore detection) and return
    (name, zarr_array, kvstore, format).  Ordering and printing are handled by
    print_means_table.  With `quiet` open failures do not print to stderr."""
    import zarr

    kvstore, path, endpoint = resolve_kvstore(cfg)
    try:
        store, open_path = build_store(
            kvstore,
            path,
            endpoint,
            array_name,
            consolidated=cfg.consolidated,
            group=cfg.group,
        )
    except Exception as e:
        if not quiet:
            print(f"Failed to open '{array_name}': {e}", file=sys.stderr)
        return None

    try:
        if kvstore == "http" and cfg.group is None:
            z = zarr.open(store, mode="r")
        else:
            z = zarr.open(store, path=open_path, mode="r")
    except Exception as e:
        if not quiet:
            print(f"Failed to open '{array_name}': {e}", file=sys.stderr)
        return None

    fmt = {"zarr2": 2, "zarr3": 3}.get(cfg.format) or int(z.metadata.zarr_format)
    # The format is the same for the whole dataset, so it is reported once.
    if cfg.debug and not getattr(cfg, "_debug_reported", False):
        print(f"debug: zarr format {fmt}", file=sys.stderr)
        if cfg.consolidated or cfg.group:
            print("debug: consolidated metadata", file=sys.stderr)
        cfg._debug_reported = True
    return (array_name, z, kvstore, fmt)


def _parse_index_range(value):
    """Parse a `START/END` value of --step/--layer/--cell (1-based, inclusive)
    into a (start, end) tuple of ints.  Raises ValueError when malformed."""
    parts = value.split("/")
    if len(parts) != 2:
        raise ValueError("expected START/END")
    try:
        start, end = (int(part) for part in parts)
    except ValueError:
        raise ValueError("START and END must be integers") from None
    if start <= 0 or end <= 0:
        raise ValueError("START and END must be positive 1-based indices")
    if end < start:
        raise ValueError("END must not be smaller than START")
    return (start, end)


def parse_args(argv):
    p = argparse.ArgumentParser(
        description="Read Zarr2/Zarr3 arrays (disk or S3) and print the "
        "per-timestep mean of all elements in the trailing dimensions. "
        "The Zarr format is auto-detected.  When --name is omitted, every "
        "data array in the dataset is selected (coordinate arrays are "
        "excluded; they are selected automatically alongside the data "
        "arrays)."
    )
    p.add_argument(
        "--format",
        choices=("zarr2", "zarr3"),
        help="force Zarr 2 or 3 as 'zarr2'/'zarr3' (default: auto-detect)",
    )
    p.add_argument(
        "--kvstore",
        choices=("file", "s3", "http"),
        help="'file', 's3' or 'http' (default: auto-detect; "
        "no '://' in --path => file)",
    )
    p.add_argument(
        "--path",
        help='file: filesystem directory; s3: "bucket/prefix" '
        'or "https://endpoint/bucket/prefix"; '
        'http: "https://host/base/group"',
    )
    p.add_argument(
        "--name",
        dest="arrays",
        help="comma-separated array names (omit to select all data arrays in the dataset)",
    )
    p.add_argument("--endpoint", help="S3 endpoint URL (optional, for s3)")
    p.add_argument(
        "--consolidated",
        action="store_true",
        help="read Zarr 2 metadata from the group-level .zmetadata "
        "instead of the per-array metadata files",
    )
    p.add_argument(
        "--step",
        metavar="START/END",
        help="1-based inclusive range of timesteps (time dim); "
        "default: the whole dim",
    )
    p.add_argument(
        "--layer",
        metavar="START/END",
        help="1-based inclusive range of layers (dim 1, rank>=3); "
        "default: the whole dim",
    )
    p.add_argument(
        "--cell",
        metavar="START/END",
        help="1-based inclusive range of grid points (last dim); "
        "default: the whole dim",
    )
    p.add_argument(
        "--info",
        action="store_true",
        help=(
            "print the TensorStore version as the first line of output; with "
            "--path, print only the present array names (the Selected/"
            "Coordinate arrays tables) and no measurements"
        ),
    )
    p.add_argument(
        "--debug",
        action="store_true",
        help="write detection details to stderr: one line with the "
        "Zarr format of the dataset, and a further line when "
        "consolidated metadata was found",
    )
    p.add_argument(
        "--group",
        help="name of a nested Zarr 3 group below --path; array "
        "metadata is served from the consolidated `zarr.json` "
        "of the store root.  When omitted and --path is a "
        "Zarr 3 root that only contains groups, all groups "
        "are listed.",
    )
    p.add_argument(
        "--data-copy-concurrency",
        type=int,
        help="cap the number of chunks fetched/decoded "
        "concurrently per read (default: zarr's async "
        "concurrency of 10)",
    )
    p.add_argument(
        "--out-path",
        help="write the selected data arrays to a NEW Zarr store at "
        "PATH (one float32 array per selected data array; the "
        "used coordinate arrays are written alongside them, "
        "preserving their dtype).  When set, only the header "
        "tables are printed - no time series table is "
        "computed; the data is read via the same chunk window "
        "loops as the min/mean/max mode.",
    )
    p.add_argument(
        "--overwrite",
        action="store_true",
        help="replace an existing store at --out-path (it is "
        "deleted first).  Without it, an existing --out-path "
        "is rejected.  Only valid with --out-path.",
    )
    p.add_argument(
        "--out-format",
        type=str,
        choices=("zarr2", "zarr3", "n5"),
        default="zarr3",
        help="output store format: zarr2 or zarr3 (Zarr) or n5 (default: zarr3)",
    )
    p.add_argument(
        "--dtype",
        dest="out_dtype",
        type=str,
        choices=("float64", "float32", "float16", "bfloat16"),
        default="float32",
        help="output data type of the float data arrays for --out-format "
        "zarr3 (default: float32).  Zarr 2 and N5 outputs always use "
        "float32.",
    )
    p.add_argument(
        "--compressor",
        type=str,
        help="output compressor for --out-path: 'none' disables compression, "
        "else one of blosc/gzip/zstd (default: reuse the input "
        "compression).  Applies to the data arrays and the coordinate "
        "arrays.",
    )
    p.add_argument(
        "--step-chunk",
        type=int,
        help="for --out-path, override the output chunk length of the step "
        "(time) dimension (clamped to the selected extent; default: "
        "clamped source chunk).",
    )
    p.add_argument(
        "--cell-chunk",
        type=int,
        help="for --out-path, override the output chunk length of the cell "
        "(last) dimension (clamped to the selected extent; default: "
        "clamped source chunk).",
    )
    args = p.parse_args(argv)
    if args.arrays:
        args.arrays = [a for a in args.arrays.split(",") if a]
    else:
        args.arrays = []
    for opt in ("step", "layer", "cell"):
        value = getattr(args, opt)
        if value is None:
            setattr(args, opt, (1, None))
            continue
        try:
            setattr(args, opt, _parse_index_range(value))
        except ValueError as e:
            p.error(f"Invalid --{opt} value: {value} ({e})")
    if args.data_copy_concurrency is not None and args.data_copy_concurrency <= 0:
        p.error("--data-copy-concurrency must be positive")
    if args.out_path and args.out_format not in ("zarr2", "zarr3", "n5"):
        p.error("--out-format must be zarr2, zarr3 or n5")
    if args.compressor is not None and args.compressor not in ("none", "blosc", "gzip", "zstd"):
        p.error(
            f"Invalid --compressor value: {args.compressor} (must be none, blosc, gzip or zstd)"
        )
    for opt in ("step_chunk", "cell_chunk"):
        value = getattr(args, opt)
        if value is not None and value <= 0:
            p.error(f"Invalid --{opt.replace('_', '-')} value: {value} (must be a positive integer)")
    return args


def _list_root_contents(cfg) -> int:
    """With no `--name` and no `--group`, list the top-level groups (or the
    direct arrays) of a consolidated Zarr root.  Returns a process exit code.
    Falls back to `print_dataset_overview` for non-consolidated stores."""
    kvstore, path, endpoint = resolve_kvstore(cfg)
    try:
        meta = _read_root_zarr3_consolidated(path)
        z3 = True
    except Exception:
        try:
            meta = _read_root_zarr2_consolidated(path)
        except Exception:
            return 0 if print_dataset_overview(cfg) else 1
        z3 = False

    children = (
        _consolidated_children(meta, "", want_groups=True)
        if z3
        else _zarr2_children(meta, "", want_groups=True)
    )
    if children:
        print(f"=== Groups in {cfg.path} ===")
        print("#  Group")
        for index, name in enumerate(children, start=1):
            print(f"{index:>2}  {name}")
        return 0

    arrays = (
        _consolidated_children(meta, "", want_groups=False)
        if z3
        else _zarr2_children(meta, "", want_groups=False)
    )
    if arrays:
        overview = _print_consolidated_overview if z3 else _print_zarr2_overview
        return 0 if overview(meta, "", arrays) else 1

    return 0 if print_dataset_overview(cfg) else 1


def main(argv=None):
    t0 = time.perf_counter()
    try:
        return _main_core(argv)
    finally:
        ru = resource.getrusage(resource.RUSAGE_SELF)
        sys.stderr.write(f"# time_seconds: {time.perf_counter() - t0:.3f}\n")
        sys.stderr.write(f"# peak_rss_bytes: {ru.ru_maxrss}\n")


def _main_core(argv):
    cfg = parse_args(sys.argv[1:] if argv is None else argv)
    if cfg.info:
        sys.stderr.write(f"TensorStore {TENSORSTORE_VERSION}\n")
    if cfg.path is None:
        if cfg.info:
            return 0
        sys.stderr.write("error: the following arguments are required: --path\n")
        return 2
    if cfg.data_copy_concurrency is not None:
        from zarr import config as zconfig

        zconfig.set({"async.concurrency": cfg.data_copy_concurrency})
    if not cfg.arrays:
        cfg.arrays = _discover_data_array_names(cfg)
        if not cfg.arrays:
            if cfg.group:
                return 0 if print_dataset_overview(cfg) else 1
            return _list_root_contents(cfg)

    # --info prints only the two header tables (Selected arrays / Coordinate
    # arrays) and reads no measurements; any write mode is ignored for it.
    if cfg.info:
        cfg.out_path = None
        cfg.overwrite = False

    # --overwrite without --out-path is an error; an existing --out-path is
    # only replaced (deleted first) with --overwrite, and must not be the
    # source dataset itself.
    if cfg.overwrite and not cfg.out_path:
        sys.stderr.write("--overwrite requires --out-path\n")
        return 1
    if cfg.out_path:
        src = os.path.abspath(cfg.path)
        dst = os.path.abspath(cfg.out_path)
        if src == dst:
            sys.stderr.write("--out-path must not be the source dataset itself\n")
            return 1
        if os.path.exists(cfg.out_path):
            if not cfg.overwrite:
                sys.stderr.write(
                    f"output directory {cfg.out_path} already exists "
                    "(use --overwrite to replace it)\n"
                )
                return 1
            shutil.rmtree(cfg.out_path)
    # Open every selected array (alphabetically sorted) BEFORE printing and
    # reduce them all together in one t-major table; each engine keeps its own
    # chunk cache, so peak memory = sum of all array caches.
    items = []
    for name in sorted(cfg.arrays):
        item = process_array(cfg, name)
        if item is None:
            return 1
        items.append(item)

    # Data arrays vs. coordinate arrays: the dimension names of a data array
    # reference coordinates, and any array whose name is such a dimension name
    # is a *coordinate* array.  Coordinate arrays cannot be selected with
    # `--name`.  The used coordinate arrays of the selected data arrays are
    # listed in a separate table (alphabetically sorted, like the data arrays).
    used_dims = set()
    for _, z, _, _ in items:
        used_dims.update(_array_dims(z))
    for name, _, _, _ in items:
        if name in used_dims:
            print(
                f"'{name}' is a coordinate array and cannot be selected with --name.",
                file=sys.stderr,
            )
            return 1

    # Table of the used coordinate arrays: for every dimension name of the
    # selected data arrays that is itself an array, list it.  For a dimension
    # without a same-named coordinate array (the horizontal "cell"), the
    # coordinate name is taken from the data arrays' `grid_mapping` attribute
    # (e.g. "crs").  All coordinate arrays are listed alphabetically (like the
    # data arrays); a grid_mapping coordinate additionally shows its
    # `grid_mapping_name` (as `grid_mapping=`), `refinement_level` and
    # `indexing_scheme` attributes.
    coord_names = set(used_dims)
    for _, z, _kvstore, _fmt in items:
        gm = _array_attrs(z).get("grid_mapping")
        if gm:
            coord_names.add(gm)

    # Open every candidate coordinate array once; keep the ones that exist.
    coord_items = []
    coord_arrays = set()  # names that turned out to be arrays
    for cname in sorted(coord_names):
        item = process_array(cfg, cname, quiet=True)
        if item is None:
            continue  # no array with this dimension name -> not a coordinate
        coord_items.append(item)
        coord_arrays.add(cname)

    # A grid_mapping coordinate like "crs" is a degenerate (usually [1])
    # reference; its printed length is the horizontal dimension ("cell") of
    # the data array that maps to it - the dimension without a same-named
    # coordinate array (e.g. "time" has one, "cell" does not) - taken from the
    # selected data arrays (sorted).
    grid_len = {}
    for _name, z, _kvstore, _fmt in items:
        a = _array_attrs(z)
        gm = a.get("grid_mapping")
        if not gm or gm in grid_len:
            continue
        dims = _array_dims(z)
        for d, length in zip(dims, z.shape):
            if d in coord_arrays:
                continue  # dimension with same-named coordinate (e.g. time)
            grid_len[gm] = length  # horizontal dimension of this grid_mapping
            break

    coords = []
    for cname, z, _kvstore, _fmt in coord_items:
        if cname in grid_len:
            shape_s = (
                f"[{grid_len[cname]}]"  # cell length of the data array instead of [1]
            )
        else:
            shape_s = f"[{', '.join(str(s) for s in z.shape)}]"
        a = _array_attrs(z)
        details = ""
        for key, label in (
            ("grid_mapping_name", "grid_mapping"),
            ("refinement_level", "refinement_level"),
            ("indexing_scheme", "indexing_scheme"),
        ):
            if key in a:
                if details:
                    details += "  "
                details += f"{label}={a[key]}"
        coords.append((cname, shape_s, details))

    # --out-path mode: write the used coordinate arrays first, then the data
    # arrays (via print_means_table, which uses the same window loops but
    # writes instead of reducing).
    if cfg.out_path:
        if not _write_coord_arrays(cfg, coord_items):
            return 1

    return 0 if print_means_table(cfg, items, coords) else 1


if __name__ == "__main__":
    sys.exit(main())
