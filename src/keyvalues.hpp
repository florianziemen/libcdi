
#pragma once

#include <charconv>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <map>
#include <optional>

// =====================================================
// Value types
// =====================================================

enum class ValueInternalType : uint8_t
{
  None,
  Int64,
  Double,
  Char,
};

enum class ValueExternalType : uint8_t
{
  None,

  Int8,
  UInt8,
  Int16,
  UInt16,
  Int32,
  UInt32,
  Int64,
  UInt64,

  Float32,
  Float64,

  Char,
  String,
};

// =====================================================
// Values
// =====================================================
class Values
{
public:
  static constexpr size_t BufferSize{ 64 };

private:
  ValueInternalType inType{ ValueInternalType::None };
  ValueExternalType exType{ ValueExternalType::None };

  // Number of elements.
  // For Char: length without null terminator.
  size_t count{ 0 };

  // BufferSize means inline storage.
  // Values greater than BufferSize indicate heap storage.
  size_t capacity{ BufferSize };

  union
  {
    alignas(8) char _buffer[BufferSize];
    void *buffer;
  };

  // =================================================
  // Internal type deduction
  // =================================================
  template <typename T>
  static constexpr ValueInternalType
  getInternalType()
  {
    using Type = std::remove_cv_t<T>;

    if constexpr (std::is_same_v<Type, int64_t>) { return ValueInternalType::Int64; }
    else if constexpr (std::is_same_v<Type, double>) { return ValueInternalType::Double; }
    else
    {
      static_assert(std::is_same_v<Type, void>, "Unsupported Values type");
    }
  }

  // =================================================
  // Element size
  // =================================================
  static constexpr size_t
  elementSize(ValueInternalType type) noexcept
  {
    switch (type)
    {
      case ValueInternalType::Int64: return sizeof(int64_t);
      case ValueInternalType::Double: return sizeof(double);
      case ValueInternalType::Char: return sizeof(char);
      default: return 0;
    }
  }

  // =================================================
  // Safe storage size calculation
  // =================================================
  static size_t
  calculateSize(ValueInternalType type, size_t elements)
  {
    if (type == ValueInternalType::Char)
    {
      if (elements == std::numeric_limits<size_t>::max()) throw std::length_error("Value too large");

      return elements + 1;
    }

    const size_t bytesPerElement = elementSize(type);

    if (bytesPerElement == 0) throw std::invalid_argument("Invalid value type");
    if (elements > std::numeric_limits<size_t>::max() / bytesPerElement) { throw std::length_error("Value too large"); }

    return elements * bytesPerElement;
  }

  // =================================================
  // Storage management
  // =================================================
  [[nodiscard]]
  bool
  usesHeap() const noexcept
  {
    return capacity > BufferSize;
  }

  [[nodiscard]]
  void *
  data() noexcept
  {
    return usesHeap() ? buffer : _buffer;
  }

  [[nodiscard]]
  const void *
  data() const noexcept
  {
    return usesHeap() ? buffer : _buffer;
  }

  void
  allocate(size_t bytes)
  {
    if (bytes <= BufferSize) return;

    buffer = ::operator new(bytes);
    capacity = bytes;
  }

  void
  release() noexcept
  {
    if (usesHeap()) ::operator delete(buffer);

    capacity = BufferSize;
    count = 0;

    inType = ValueInternalType::None;
    exType = ValueExternalType::None;
  }

  // =================================================
  // Internal constructor
  // =================================================
  Values(ValueInternalType internalType, ValueExternalType externalType, size_t elements)
      : inType(internalType), exType(externalType), count(elements), _buffer{}
  {
    allocate(calculateSize(inType, count));
  }

public:
  // =================================================
  // Default constructor
  // =================================================
  Values() noexcept : _buffer{} {}

  // =================================================
  // Factory for direct data initialization
  // =================================================
  static Values
  createUninitialized(ValueInternalType internalType, size_t elements, ValueExternalType externalType = ValueExternalType::None)
  {
    return { internalType, externalType, elements };
  }

  // =================================================
  // Numeric constructor
  // Supports int64_t and double.
  // =================================================
  // template <typename T>
  //  requires(std::is_same_v<std::remove_cv_t<T>, int64_t> || std::is_same_v<std::remove_cv_t<T>, double>) // C++20
  template <typename T, typename = std::enable_if_t<std::is_same_v<std::remove_cv_t<T>, int64_t>
                                                    || std::is_same_v<std::remove_cv_t<T>, double>>>
  Values(size_t elements, const T *values, ValueExternalType externalType = ValueExternalType::None)
      : Values(getInternalType<T>(), externalType, elements)
  {
    if (!values && elements != 0) throw std::invalid_argument("Null value data");

    if (elements > 0) { std::memcpy(data(), values, elements * sizeof(T)); }
  }

  // =================================================
  // Char constructor
  // =================================================
  Values(std::string_view value, ValueExternalType externalType = ValueExternalType::None)
      : Values(ValueInternalType::Char, externalType, value.size())
  {
    if (!value.empty()) { std::memcpy(data(), value.data(), value.size()); }

    static_cast<char *>(data())[value.size()] = '\0';
  }

  // =================================================
  // Char constructor for direct data reading
  // =================================================
  Values(size_t charLength, ValueExternalType externalType = ValueExternalType::None)
      : Values(ValueInternalType::Char, externalType, charLength)
  {
    static_cast<char *>(data())[charLength] = '\0';
  }

  // =================================================
  // Destructor
  // =================================================
  ~Values() { release(); }

  // =================================================
  // Copy constructor
  // =================================================
  Values(const Values &other) : inType(other.inType), exType(other.exType), count(other.count), _buffer{}
  {
    const size_t bytes = calculateSize(inType, count);

    allocate(bytes);

    if (bytes > 0) { std::memcpy(data(), other.data(), bytes); }
  }

  // =================================================
  // Copy assignment
  // =================================================
  Values &
  operator=(const Values &other)
  {
    if (this == &other) return *this;

    Values temp(other);
    swap(temp);

    return *this;
  }

  // =================================================
  // Move constructor
  // =================================================
  Values(Values &&other) noexcept
      : inType(other.inType), exType(other.exType), count(other.count), capacity(other.capacity), _buffer{}
  {
    if (other.usesHeap()) { buffer = other.buffer; }
    else
    {
      std::memcpy(_buffer, other._buffer, BufferSize);
    }

    other.capacity = BufferSize;
    other.count = 0;
    other.inType = ValueInternalType::None;
    other.exType = ValueExternalType::None;
  }

  // =================================================
  // Move assignment
  // =================================================
  Values &
  operator=(Values &&other) noexcept
  {
    if (this == &other) return *this;

    release();

    inType = other.inType;
    exType = other.exType;
    count = other.count;
    capacity = other.capacity;

    if (other.usesHeap()) { buffer = other.buffer; }
    else
    {
      std::memcpy(_buffer, other._buffer, BufferSize);
    }

    other.capacity = BufferSize;
    other.count = 0;

    other.inType = ValueInternalType::None;
    other.exType = ValueExternalType::None;

    return *this;
  }

  // =================================================
  // Swap
  // =================================================
  void
  swap(Values &other) noexcept
  {
    if (this == &other) return;

    Values temp(std::move(*this));

    *this = std::move(other);
    other = std::move(temp);
  }

  friend void
  swap(Values &lhs, Values &rhs) noexcept
  {
    lhs.swap(rhs);
  }

  // =================================================
  // Metadata
  // =================================================
  [[nodiscard]]
  ValueInternalType
  internalType() const noexcept
  {
    return inType;
  }

  [[nodiscard]]
  ValueExternalType
  externalType() const noexcept
  {
    return exType;
  }

  [[nodiscard]]
  size_t
  size() const noexcept
  {
    return count;
  }

  [[nodiscard]]
  int
  isize() const noexcept
  {
    return static_cast<int>(count);
  }

  [[nodiscard]]
  size_t
  storageSize() const
  {
    return calculateSize(inType, count);
  }

  [[nodiscard]]
  bool
  isInline() const noexcept
  {
    return !usesHeap();
  }

  // =================================================
  // Raw data access
  // =================================================
  [[nodiscard]]
  void *
  rawData() noexcept
  {
    return data();
  }

  [[nodiscard]]
  const void *
  rawData() const noexcept
  {
    return data();
  }

  // =================================================
  // Integer access
  // =================================================
  [[nodiscard]]
  int64_t *
  integers()
  {
    if (inType != ValueInternalType::Int64) throw std::logic_error("Value is not int64");

    return static_cast<int64_t *>(data());
  }

  [[nodiscard]]
  const int64_t *
  integers() const
  {
    if (inType != ValueInternalType::Int64) throw std::logic_error("Value is not int64");

    return static_cast<const int64_t *>(data());
  }

  // =================================================
  // Double access
  // =================================================
  [[nodiscard]]
  double *
  doubles()
  {
    if (inType != ValueInternalType::Double) throw std::logic_error("Value is not double");

    return static_cast<double *>(data());
  }

  [[nodiscard]]
  const double *
  doubles() const
  {
    if (inType != ValueInternalType::Double) throw std::logic_error("Value is not double");

    return static_cast<const double *>(data());
  }

  // =================================================
  // Char access
  // =================================================
  [[nodiscard]]
  char *
  chars()
  {
    if (inType != ValueInternalType::Char) throw std::logic_error("Value is not char");

    return static_cast<char *>(data());
  }

  [[nodiscard]]
  const char *
  chars() const
  {
    if (inType != ValueInternalType::Char) throw std::logic_error("Value is not char");

    return static_cast<const char *>(data());
  }

  // =================================================
  // convert to string
  // =================================================

  std::string
  to_string() const
  {
    if (size() == 0) return {};
    switch (internalType())
    {
      case ValueInternalType::Int64:
      {
        if (externalType() == ValueExternalType::Int8) return integers()[0] ? std::string("true") : std::string("false");
        if (size() == 1) return std::to_string(integers()[0]);
        std::string s = "[";
        for (size_t i = 0; i < size(); ++i)
        {
          if (i) s += ',';
          s += std::to_string(integers()[i]);
        }
        s += ']';
        return s;
      }
      case ValueInternalType::Double:
      {
        auto fmt_double = [](double d) -> std::string
        {
          char buf[64];
          auto res = std::to_chars(buf, buf + 64, d);
          return std::string(buf, res.ptr);
        };
        if (size() == 1) return fmt_double(doubles()[0]);
        std::string s = "[";
        for (size_t i = 0; i < size(); ++i)
        {
          if (i) s += ",";
          s += fmt_double(doubles()[i]);
        }
        s += "]";
        return s;
      }
      case ValueInternalType::Char:
      {
        return { chars(), size() };
      }
      default: return {};
    }
  }

  // =================================================
  // get values
  // =================================================

  template <typename T>
  std::optional<T>
  get_integral_value() const noexcept
  {
    static_assert(std::is_integral_v<T>, "T must be an integral type");
    if (internalType() != ValueInternalType::Int64) return std::nullopt;

    // Check number of elements.
    if (size() != 1) return std::nullopt;

    const auto value = *integers();
    // Check conversion range.
    if constexpr (std::is_signed_v<T>)
    {
      if (value < std::numeric_limits<T>::min() || value > std::numeric_limits<T>::max()) return std::nullopt;
    }
    else
    {
      if (value < 0 || static_cast<uint64_t>(value) > std::numeric_limits<T>::max()) return std::nullopt;
    }
    return static_cast<T>(value);
  }

  template <typename T>
  std::optional<T>
  get_floating_value() const noexcept
  {
    static_assert(std::is_floating_point_v<T>, "T must be an floating point type");
    if (internalType() != ValueInternalType::Double && internalType() != ValueInternalType::Int64) return std::nullopt;

    // Check number of elements.
    if (size() != 1) return std::nullopt;

    if (internalType() == ValueInternalType::Int64) return static_cast<T>(*integers());
    return static_cast<T>(*doubles());
  }

  std::string_view
  get_string_view() const noexcept
  {
    if (internalType() != ValueInternalType::Char) return {};

    // Check number of elements.
    if (size() == 0) return {};

    return std::string_view(chars(), size());
  }

  std::optional<std::string>
  get_string() const noexcept
  {
    if (internalType() != ValueInternalType::Char) return std::nullopt;

    // Check number of elements.
    if (size() == 0) return std::nullopt;

    return std::string(chars(), size());
  }
};

// =====================================================
// KeyValuesMap
// =====================================================
using KeyValuesMap = std::map<std::string, Values>;

template <typename T>
std::optional<T>
kvmap_get_value(const KeyValuesMap &kvmap, const std::string &key)
{
  // Check if key exists.
  const auto it = kvmap.find(key);
  if (it == kvmap.end()) return std::nullopt;

  const Values &values = it->second;

  if constexpr (std::is_integral_v<T>) { return values.get_integral_value<T>(); }
  else if constexpr (std::is_floating_point_v<T>) { return values.get_floating_value<T>(); }
  else if constexpr (std::is_same_v<T, std::string>) { return values.get_string(); }

  return std::nullopt;
}

inline std::string_view
kvmap_get_stringview(const KeyValuesMap &kvmap, const std::string &key)
{
  // Check if key exists.
  const auto it = kvmap.find(key);
  if (it == kvmap.end()) return {};

  const Values &values = it->second;
  return values.get_string_view();
}
