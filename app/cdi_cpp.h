#ifndef CDI_CPP_H
#define CDI_CPP_H

#include "cdi.h"
#include "cdi_query.h"
#include <memory>

template <auto DestroyFn>
struct CdiHandle
{
  int id = CDI_UNDEFID;
  ~CdiHandle()
  {
    if (id >= 0) DestroyFn(id);
  }
  CdiHandle() = default;
  CdiHandle(int x) : id(x) {}
  CdiHandle(const CdiHandle &) = delete;
  CdiHandle &operator=(const CdiHandle &) = delete;
  CdiHandle(CdiHandle &&o) noexcept : id(std::exchange(o.id, CDI_UNDEFID)) {}
  CdiHandle &
  operator=(CdiHandle &&o) noexcept
  {
    if (this != &o)
    {
      if (id >= 0) DestroyFn(id);
      id = std::exchange(o.id, CDI_UNDEFID);
    }
    return *this;
  }
  operator int() const { return id; }
  explicit
  operator bool() const
  {
    return id >= 0;
  }
};

using StreamH = CdiHandle<+[](int x) { streamClose(x); }>;
using VlistH = CdiHandle<+[](int x) { vlistDestroy(x); }>;
using TaxisH = CdiHandle<+[](int x) { taxisDestroy(x); }>;

struct CdiQueryDeleter
{
  void
  operator()(struct CdiQuery *query) const noexcept
  {
    if (query) cdiQueryDelete(query);
  }
};

using CdiQueryPtr = std::unique_ptr<struct CdiQuery, CdiQueryDeleter>;

inline CdiQueryPtr
cdi_query_create()
{
  return CdiQueryPtr(cdiQueryCreate());
}

#endif
