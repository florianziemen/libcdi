#ifndef CDI_QUERY_RAII_H
#define CDI_QUERY_RAII_H

#include "cdi_query.h"
#include <memory>

struct CdiQueryDeleter
{
  void operator()(struct CdiQuery *query) const noexcept
  {
    if (query) cdiQueryDelete(query);
  }
};

using CdiQueryPtr = std::unique_ptr<struct CdiQuery, CdiQueryDeleter>;

inline CdiQueryPtr cdi_query_create()
{
  return CdiQueryPtr(cdiQueryCreate());
}

#endif
