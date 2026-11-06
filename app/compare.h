#ifndef COMPARE_H
#define COMPARE_H

#include <cmath>

// clang-format off
constexpr auto is_not_equal = [](auto a, auto b) noexcept { return  (a < b || b < a); };
constexpr auto is_equal     = [](auto a, auto b) noexcept { return  !is_not_equal(a, b); };

constexpr auto fp_is_equal = [](auto a, auto b) noexcept
{
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wfloat-equal"
  auto aNan = std::isnan(a);
  auto bNan = std::isnan(b);
  return (aNan || bNan) ? (aNan && bNan): (a == b);
  #pragma GCC diagnostic pop
};
constexpr auto fp_is_not_equal = [](auto a, auto b) noexcept { return !fp_is_equal(a, b); };
// clang-format on

#endif /* COMPARE_H */
