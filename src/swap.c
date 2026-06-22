/* DO NOT REMOVE the config.h include file under any circumstances,
 * it's very much needed on some platforms */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
/* DO NOT REMOVE the above config.h include file under any
 * circumstances as long as it's the autoconf configuration header
 * used to build this package. When it's missing on some platforms,
 * some poor person has to do long, tedious debugging sessions, where
 * struct offsets almost imperceptibly change from one file to the
 * next to find out what happened */

#include <inttypes.h>
#include <stdio.h>

#include "swap.h"

static inline uint32_t
swap4byte_value(uint32_t x)
{
  // clang-format off
  return (((x >> 24) & 0x00ff) | ((x & 0x00ff) << 24) |
          ((x >>  8) & 0xff00) | ((x & 0xff00) <<  8));
  // clang-format on
}

static inline uint64_t
swap8byte_value(uint64_t x)
{
  // clang-format off
  return (((x >> 56) & 0x000000ff) | ((x & 0x000000ff) << 56) |
          ((x >> 40) & 0x0000ff00) | ((x & 0x0000ff00) << 40) |
          ((x >> 24) & 0x00ff0000) | ((x & 0x00ff0000) << 24) |
          ((x >>  8) & 0xff000000) | ((x & 0xff000000) <<  8));
  // clang-format on
}

void
swap4byte(void *ptr, size_t size)
{
  uint32_t *v = (uint32_t *) ptr;
  for (size_t i = 0; i < size; ++i) { v[i] = swap4byte_value(v[i]); }
}

void
swap8byte(void *ptr, size_t size)
{
  uint64_t *v = (uint64_t *) ptr;
  for (size_t i = 0; i < size; ++i) { v[i] = swap8byte_value(v[i]); }
}
/*
 * Local Variables:
 * c-file-style: "Java"
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * show-trailing-whitespace: t
 * require-trailing-newline: t
 * End:
 */
