#ifndef DOMAIN_UTILS_HPP
#define DOMAIN_UTILS_HPP

#include <concepts>
#include <iostream>
#include <string>
#include <type_traits>

namespace solver {

template<typename T>
concept domain_class = requires(T a, T b, typename T::value_type val)// NOLINT(readability-identifier-length)
{
  typename T::value_type;
  {
    a.is_universal()
    } -> std::same_as<bool>;
  {
    a.empty()
    } -> std::same_as<bool>;
  {
    a.is_singleton()
    } -> std::same_as<bool>;
  {
    a.begin()
    } -> std::same_as<typename T::iterator>;
  {
    *a.begin()
    } -> std::same_as<typename T::value_type>;
  {
    a.end()
    } -> std::same_as<typename T::iterator>;
  {
    a.clear()
  };
  {
    a.erase(val)
  };
  {
    a.insert(val)
  };
  {
    a.contains(val)
    } -> std::same_as<bool>;
  {
    min(a)
    } -> std::same_as<typename T::value_type>;
  {
    max(a)
    } -> std::same_as<typename T::value_type>;
  {
    a == b
    } -> std::same_as<bool>;
  {
    T::MIN_VALUE
    } -> std::same_as<const typename T::value_type &>;
  {
    T::MAX_VALUE
    } -> std::same_as<const typename T::value_type &>;
  {
    a = b
    } -> std::same_as<T &>;
  {
    a = val
    } -> std::same_as<T &>;
  {
    T(a)
    } -> std::same_as<T>;
  {
    T(val)
    } -> std::same_as<T>;
};


template<domain_class Domain> inline std::ostream &operator<<(std::ostream &out, const Domain &domain)
{
  constexpr bool small_range = Domain::MAX_VALUE < 8 && Domain::MIN_VALUE >= -1;
  if constexpr (!small_range) {
    if (domain.is_universal()) { return out << "{*}"; }
  }
  out << "{";
  bool first = true;
  for (auto value : domain) {
    if (!first) { out << ", "; }
    out << value;
    first = false;
  }
  return out << "}";
}

template<domain_class Domain> std::string to_string(const Domain &domain)
{
  constexpr bool small_range = Domain::MAX_VALUE < 8 && Domain::MIN_VALUE >= -1;
  if constexpr (!small_range) {
    if (domain.is_universal()) { return "{*}"; }
  }
  std::string result = "{";
  bool first = true;
  for (auto value : domain) {
    if (!first) { result += ", "; }
    result += std::to_string(value);
    first = false;
  }
  result += "}";
  return result;
}

[[nodiscard]] inline constexpr auto get_value(const domain_class auto &domain)
{
  assert(min(domain) == max(domain));
  return min(domain);
}

template<typename T>
concept domain =
  requires(T a, T b, typename T::value_type val, std::ostream &out)// NOLINT(readability-identifier-length)
{
  requires domain_class<T>;
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

#endif
