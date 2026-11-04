#include "cdi_query.h"

#include <string>
#include <string_view>
#include <vector>
#include <format>
#include <iostream>
#include <optional>
#include <charconv>
#include <system_error>

template <typename... Args>
static bool
handle_error(std::format_string<Args...> fmt, Args &&...args)
{
  printf("\n");
  const std::string message = std::format(fmt, std::forward<Args>(args)...);
  fprintf(stderr, "%s\n", message.c_str());
  return true;
}

std::optional<int64_t>
string_to_int64(std::string_view str)
{
  int64_t value{};
  auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), value);
  if (ec != std::errc{} || ptr != str.data() + str.size())
  {
    std::cerr << "Error: '" << str << "' kann nicht in einen Integer konvertiert werden!\n";
    return std::nullopt;
  }

  return value;
}
/*
std::optional<double>
string_to_double(const std::string &str)
{
  double value{};
  const auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), value, std::chars_format::general);
  if (ec != std::errc{} || ptr != str.data() + str.size())
  {
    std::cerr << "Fehler: '" << str << "' kann nicht in einen Double konvertiert werden!\n";
    return std::nullopt;
  }

  return value;
}
*/

std::optional<std::vector<int64_t>>
string_to_intlist(std::string_view str)
{
  constexpr auto separator = '/';
  std::vector<int64_t> result;

  size_t start = 0;
  while (true)
  {
    auto pos = str.find(separator, start);
    auto part = str.substr(start, pos == std::string::npos ? pos : pos - start);

    auto value = string_to_int64(part);
    if (!value)
    {
      std::cerr << "Fehler: Ungültiger Integer in '" << str << "'!\n";
      return std::nullopt;
    }

    result.push_back(*value);

    if (pos == std::string::npos) break;

    start = pos + 1;

    // Maximal zwei Werte erlauben
    if (result.size() >= 2)
    {
      std::cerr << "Fehler: Mehr als zwei Werte in '" << str << "'!\n";
      return std::nullopt;
    }
  }

  return result;
}

static std::optional<std::vector<int64_t>>
decode_int_parameter(std::string_view key, std::string const &value)
{
  auto values = string_to_intlist(value);
  if (!values)
  {
    handle_error("decode_int_parameter failed!");
    return {};
  }

  if (values->size() == 2)
  {
    auto start = (*values)[0];
    auto end = (*values)[1];
    if (end < start)
    {
      handle_error("key={} parse error: END value={} must be greater than START value={}!", key, end, start);
      return {};
    }
    auto numVals = (end - start + 1);
    if (numVals > 1) { (*values)[1] = numVals; }
  }

  return *values;
}

/*

static std::vector<double>
decode_float_parameter(std::string const &param)
{
  std::vector<double> values;
  auto sep = separator1;
  auto n = param.find(sep);
  if (n == std::string::npos)
  {
    sep = separator2;
    n = param.find(sep);
  }
  if (n == std::string::npos) { values.push_back(parameter_to_double(param)); }
  else
  {
    auto v1 = parameter_to_double(param.substr(0, n));
    auto v2 = parameter_to_double(param.substr(n + sep.size()));
    if (v2 < v1) cdo_abort("Second parameter %g must be greater than first parameter %g!", v2, v1);
    values.push_back(v1);
    values.push_back(v2);
  }

  return values;
}

*/

#include <string>
#include <string_view>
#include <vector>
#include <utility>

using KeyValue = std::pair<std::string, std::vector<std::string>>;

std::vector<KeyValue>
parseKeyValues(std::string_view input)
{
  std::vector<KeyValue> result;

  size_t pos = 0;
  while (pos < input.size())
  {
    // Key bis zum '=' suchen
    const size_t equalPos = input.find('=', pos);
    if (equalPos == std::string_view::npos) break;

    std::string key(input.substr(pos, equalPos - pos));
    pos = equalPos + 1;

    std::vector<std::string> values;
    while (pos < input.size())
    {
      const size_t commaPos = input.find(',', pos);
      if (commaPos == std::string_view::npos)
      {
        values.emplace_back(input.substr(pos));
        pos = input.size();
        break;
      }

      // Prüfen, ob nach dem Komma ein neuer Key beginnt
      const size_t nextEqual = input.find('=', commaPos + 1);
      const size_t nextComma = input.find(',', commaPos + 1);

      if (nextEqual != std::string_view::npos && (nextComma == std::string_view::npos || nextEqual < nextComma))
      {
        values.emplace_back(input.substr(pos, commaPos - pos));
        pos = commaPos + 1;
        break;
      }

      // Andernfalls gehört das nächste Element noch zu den Values
      values.emplace_back(input.substr(pos, commaPos - pos));
      pos = commaPos + 1;
    }

    result.emplace_back(std::move(key), std::move(values));
  }

  return result;
}

bool
query_set_parameter(std::string_view keyValueString, struct CdiQuery *query)
{
  auto keyValueList = parseKeyValues(keyValueString);

  bool processStartEndDate{ false };
  double startDateFloat{ -99999999999.0 };
  double endDateFloat{ 99999999999.0 };

  for (const auto &[key, values] : keyValueList)
  {
    int numValues = values.size();
    if (numValues < 1) return handle_error("Missing value for parameter key >%s<!", key);

    if (key == "group")
    {
      std::vector<const char *> queryGroups(numValues);
      for (int i = 0; i < numValues; ++i) { queryGroups[i] = values[i].c_str(); }
      cdiQuerySetGroups(query, queryGroups.size(), queryGroups.data());
    }
    else if (key == "name")
    {
      std::vector<const char *> queryNames(numValues);
      for (int i = 0; i < numValues; ++i) { queryNames[i] = values[i].c_str(); }
      cdiQuerySetNames(query, queryNames.size(), queryNames.data());
    }
    else if (key == "startdate")
    {
      processStartEndDate = true;
      // startDateFloat = datestr_to_double(values[0], 0);
    }
    else if (key == "enddate")
    {
      processStartEndDate = true;
      // endDateFloat = datestr_to_double(values[0], 1);
    }
    else if (key == "date")
    {
      if (numValues > 1) return handle_error("Too many values for key=date (maxvalues=1 or range=start/end)");
      // auto queryDates = decode_float_parameter(values[0]);
      // cdiQuerySetDates(query, queryDates.size(), queryDates.data());
    }
    else if (key == "step")
    {
      if (numValues > 1) return handle_error("Too many values for key=step (maxvalues=1 or range=START/END)");
      auto querySteps = decode_int_parameter(key, values[0]);
      if (!querySteps) return true;
      cdiQuerySetSteps(query, (*querySteps).size(), (*querySteps).data());
    }
    else if (key == "layer")
    {
      if (numValues > 1) return handle_error("Too many values for key=layer (maxvalues=1 or range=start/end)");
      auto queryLayers = decode_int_parameter(key, values[0]);
      cdiQuerySetLayers(query, (*queryLayers).size(), (*queryLayers).data());
    }
    else if (key == "cell")
    {
      if (numValues > 1) return handle_error("Too many values for key=cell (maxvalues=1 or range=start/end)");
      auto queryCells = decode_int_parameter(key, values[0]);
      cdiQuerySetCells(query, (*queryCells).size(), (*queryCells).data());
    }
    else
    {
      return handle_error("Invalid parameter key >%s<!", key);
    }
  }

  if (processStartEndDate)
  {
    std::vector<double> queryDates = { startDateFloat, endDateFloat };
    cdiQuerySetDates(query, queryDates.size(), queryDates.data());
  }

  return false;
}
