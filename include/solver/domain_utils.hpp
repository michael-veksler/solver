#ifndef DOMAIN_UTILS_HPP
#define DOMAIN_UTILS_HPP

#include <concepts>
#include <iostream>
#include <sstream>
#include <string>
#include <type_traits>
#include <fmt/ostream.h>
#include <fmt/core.h>

namespace solver {

template<typename T>
concept domain_class_concept = requires(T a, const T b, typename T::value_type val)// NOLINT(readability-identifier-length)
{
  typename T::value_type;
  { b.is_universal() } -> std::same_as<bool>;
  { b.empty() } -> std::same_as<bool>;
  { b.is_singleton() } -> std::same_as<bool>;
  { b.begin() } -> std::same_as<typename T::iterator>;
  { *b.begin() } -> std::same_as<typename T::value_type>;
  { b.end() } -> std::same_as<typename T::iterator>;
  { a.clear() };
  { a.erase(val) };
  { a.insert(val) };
  { b.contains(val) } -> std::same_as<bool>;
  { min(b) } -> std::same_as<typename T::value_type>;
  { max(b) } -> std::same_as<typename T::value_type>;
  { b.size() } -> std::same_as<typename T::size_type>;
  { a == b } -> std::same_as<bool>;
  { T::MIN_VALUE } -> std::same_as<const typename T::value_type &>;
  { T::MAX_VALUE} -> std::same_as<const typename T::value_type &>;
  requires T::MIN_VALUE < T::MAX_VALUE;
  { a = b } -> std::same_as<T &>;
  { a = val } -> std::same_as<T &>;
  { T(b) } -> std::same_as<T>;
  { T(val) } -> std::same_as<T>;
};


template<domain_class_concept Domain> inline std::ostream &operator<<(std::ostream &out, const Domain &domain)
{
  // This is to avoid warnings of using bool in numeric context
  constexpr auto to_numeric = [](auto value) {
    if constexpr (std::is_same_v<decltype(value), bool>) {
      return value ? 1 : 0;
    } else {
      return value;
    }
  };
  constexpr bool small_range = 0 <= to_numeric(Domain::MIN_VALUE) && to_numeric(Domain::MAX_VALUE) < 8;
  if constexpr (!small_range) {
    if (domain.is_universal()) { return out << "{*}"; }
  }
  out << "{";
  bool first = true;
  for (auto value : domain) {
    if (!first) { out << ", "; }
    if constexpr (sizeof(typename Domain::value_type) == 1) {
      // This is to print any type as integer, even if it is a char
      out << static_cast<int>(value);
    } else {
      out << value;
    }
    first = false;
  }
  return out << "}";
}

template<domain_class_concept Domain> std::string to_string(const Domain &domain)
{
  std::ostringstream out;
  out << domain;
  return out.str();
}

[[nodiscard]] inline constexpr auto get_value(const domain_class_concept auto &domain)
{
  assert(min(domain) == max(domain));
  return min(domain);
}

template<typename T>
concept domain_concept =
  requires(T a, T b, typename T::value_type val, std::ostream &out)// NOLINT(readability-identifier-length)
{
  requires domain_class_concept<T>;
  {
    to_string(a)
    } -> std::same_as<std::string>;
  {
    get_value(a)
    } -> std::same_as<typename T::value_type>;
  {
    out << a
    } -> std::convertible_to<std::ostream &>;
};
}// namespace solver

// ostream formatter for all domain_concept classes
template<solver::domain_concept Domain> struct fmt::formatter<Domain> : fmt::ostream_formatter
{
};

#endif
