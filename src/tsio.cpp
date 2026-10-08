#include "tsio.h"

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

const char *
ts_lib_version(void)
{
  return TENSORSTORE_VERSION;
}
