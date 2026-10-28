#ifndef FIELD_H
#define FIELD_H

#include "cdi_cpp.h"
#include "compare.h"
#include <vector>

constexpr size_t ompLoopSizeLarge = 999999;

enum struct MemType
{
  Native,
  Float,
  Double
};

struct CdiVar
{
  size_t counter{ 0 };
  std::string name;
  std::string longname;
  std::string units;
  std::string stdname;
  std::vector<double> layers;
  MemType memType{ MemType::Native };
  int gridID{ -1 };
  int zaxisID{ -1 };
  int gridType{ -1 };
  int zaxisType{ -1 };
  int timeType{ -1 };
  int stepType{ -1 };
  size_t numCells{ 0 };
  int numLayers{ 0 };
  int dataType{ -1 };
  double missval{ 0 };
  int code{ 0 };
  int param{ 0 };
  int nwpv{ 1 };  // number of words per value; real:1  complex:2
  bool isConstant{ false };
  int ID{ 0 };
};

using VarList = std::vector<CdiVar>;

struct Field
{
  std::vector<float> vec_f;
  std::vector<double> vec_d;
  double missval{};
  size_t numMiss{ 0 };
  size_t size{ 0 };
  MemType memType{ MemType::Native };

  void
  init(CdiVar const &var)
  {
    if (var.numCells != size || var.memType != memType)
    {
      size = var.numCells;
      memType = var.memType;
      (memType == MemType::Float) ? vec_f.resize(size) : vec_d.resize(size);
    }
  }
};

// clang-format off
template <typename FUNC, typename FIELD>
inline auto
field_operation(FUNC func, FIELD &field)
-> decltype(func(field.vec_f))
{
  if      (field.memType == MemType::Float)  return func(field.vec_f);
  else if (field.memType == MemType::Double) return func(field.vec_d);
  else throw std::runtime_error("Type of field unsupported!");
}

template <typename FUNC, typename FIELD, typename... ARGS>
inline auto
field_operation(FUNC func, FIELD &field, ARGS &...args)
-> decltype(func(field.vec_f, args...))
{
  if      (field.memType == MemType::Float)  return func(field.vec_f, args...);
  else if (field.memType == MemType::Double) return func(field.vec_d, args...);
  else throw std::runtime_error("Type of field unsupported!");
}

// clang-format off
struct MinMax
{
  double min {std::numeric_limits<double>::max()};
  double max {-std::numeric_limits<double>::max()};
  size_t n {0};
  MinMax() = default;
  MinMax(double rmin, double rmax, size_t rn) : min(rmin), max(rmax), n(rn) {};
  MinMax(double rmin, double rmax) : min(rmin), max(rmax), n(0) {};
};

struct MinMaxSum : MinMax
{
  double sum {0.0};
  MinMaxSum() = default;
  MinMaxSum(double rmin, double rmax, double rsum, size_t rn) : sum(rsum) { min = rmin; max = rmax; n = rn; };
  MinMaxSum(double rmin, double rmax, double rsum) : sum(rsum) { min = rmin; max = rmax; n = 0; };
};

struct MinMaxMean : MinMax
{
  double mean {0.0};
  MinMaxMean() = default;
  MinMaxMean(double rmin, double rmax, double rmean, size_t rn) : mean(rmean) { min = rmin; max = rmax; n = rn; };
  MinMaxMean(double rmin, double rmax, double rmean) : mean(rmean) { min = rmin; max = rmax; n = 0; };
};
// clang-format on

template <typename T>
MinMaxSum
varray_min_max_sum(std::vector<T> const &v, size_t n, const MinMaxSum &mms)
{
  auto f_minmaxsum = [](auto val, auto &vmin, auto &vmax, auto &vsum)
  {
    vmin = std::min(vmin, val);
    vmax = std::max(vmax, val);
    vsum += val;
  };

  auto vmin = mms.min;
  auto vmax = mms.max;
  auto vsum = mms.sum;

  if (n > ompLoopSizeLarge)
  {
#ifdef _OPENMP
#pragma omp parallel for simd default(shared) schedule(static) reduction(min : vmin) reduction(max : vmax) reduction(+ : vsum)
#endif
    for (size_t i = 0; i < n; ++i) { f_minmaxsum((double) v[i], vmin, vmax, vsum); }
  }
  else
  {
#ifdef _OPENMP
#pragma omp simd reduction(min : vmin) reduction(max : vmax) reduction(+ : vsum)
#endif
    for (size_t i = 0; i < n; ++i) { f_minmaxsum((double) v[i], vmin, vmax, vsum); }
  }

  return { vmin, vmax, vsum, n };
}

template <typename T>
MinMaxSum
varray_min_max_sum_mv(std::vector<T> const &v, size_t n, const MinMaxSum &mms, double missval)
{
  auto f_minmaxsum_mv = [](auto val, auto mv, auto &vmin, auto &vmax, auto &vsum, auto &nvals, auto &is_NE)
  {
    if (is_NE(val, mv))
    {
      vmin = std::min(vmin, val);
      vmax = std::max(vmax, val);
      vsum += val;
      nvals++;
    }
  };

  T mv = static_cast<T>(missval);
  auto vmin = mms.min;
  auto vmax = mms.max;
  auto vsum = mms.sum;

  size_t nvals = 0;
  if (std::isnan(mv))
  {
#ifdef _OPENMP
#pragma omp parallel for simd if (n > ompLoopSizeLarge) default(shared) schedule(static) reduction(min : vmin) \
    reduction(max : vmax) reduction(+ : vsum, nvals)
#endif
    for (size_t i = 0; i < n; ++i) { f_minmaxsum_mv((double) v[i], mv, vmin, vmax, vsum, nvals, fp_is_not_equal); }
  }
  else
  {
#ifdef _OPENMP
#pragma omp parallel for simd if (n > ompLoopSizeLarge) default(shared) schedule(static) reduction(min : vmin) \
    reduction(max : vmax) reduction(+ : vsum, nvals)
#endif
    for (size_t i = 0; i < n; ++i) { f_minmaxsum_mv((double) v[i], mv, vmin, vmax, vsum, nvals, is_not_equal); }
  }

  if (nvals == 0 && is_equal(vmin, std::numeric_limits<double>::max())) vmin = mv;
  if (nvals == 0 && is_equal(vmax, -std::numeric_limits<double>::max())) vmax = mv;

  return { vmin, vmax, vsum, nvals };
}

template <typename T>
MinMaxMean
varray_min_max_mean(std::vector<T> const &v, size_t n)
{
  auto mms = varray_min_max_sum(v, n, MinMaxSum());
  auto rmean = (n != 0) ? mms.sum / static_cast<double>(n) : 0.0;
  return MinMaxMean(mms.min, mms.max, rmean, n);
}

// Explicit instantiation

template <typename T>
MinMaxMean
varray_min_max_mean_mv(std::vector<T> const &v, size_t n, double missval)
{
  auto mms = varray_min_max_sum_mv(v, n, MinMaxSum(), missval);
  auto rmean = (mms.n != 0) ? mms.sum / static_cast<double>(mms.n) : missval;
  return MinMaxMean(mms.min, mms.max, rmean, mms.n);
}

inline MinMaxMean
field_min_max_mean(Field const &field)
{
  auto func = [&](auto const &v, auto n) { return varray_min_max_mean(v, n); };
  return field_operation(func, field, field.size);
}

inline MinMaxMean
field_min_max_mean_mv(Field const &field)
{
  auto func = [&](auto const &v, auto n, double mv) { return varray_min_max_mean_mv(v, n, mv); };
  return field_operation(func, field, field.size, field.missval);
}

inline void
stream_read_var_slice(StreamHandle const &stream, int varID, int levelID, Field &field)
{
  if (field.memType == MemType::Float)
    streamReadVarSliceF(stream, varID, levelID, field.vec_f.data(), &field.numMiss);
  else
    streamReadVarSlice(stream, varID, levelID, field.vec_d.data(), &field.numMiss);
}

inline void
stream_write_var_slice(StreamHandle const &stream, int varID, int levelID, Field &field)
{
  if (field.memType == MemType::Float)
    streamWriteVarSliceF(stream, varID, levelID, field.vec_f.data(), field.numMiss);
  else
    streamWriteVarSlice(stream, varID, levelID, field.vec_d.data(), field.numMiss);
}

inline void
stream_read_field(StreamHandle const &stream, Field &field)
{
  if (field.memType == MemType::Float)
    streamReadFieldF(stream, field.vec_f.data(), &field.numMiss);
  else
    streamReadField(stream, field.vec_d.data(), &field.numMiss);
}

inline void
stream_write_field(StreamHandle const &stream, Field &field)
{
  if (field.memType == MemType::Float)
    streamWriteFieldF(stream, field.vec_f.data(), field.numMiss);
  else
    streamWriteField(stream, field.vec_d.data(), field.numMiss);
}

#endif
