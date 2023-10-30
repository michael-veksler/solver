#ifndef DISCRETE_DOMAIN_HPP
#define DISCRETE_DOMAIN_HPP

#include <boost/icl/discrete_interval.hpp>
#include <solver/solver_library_export.hpp>
#include <boost/icl/interval_set.hpp>

#include <cassert>
#include <cinttypes>
#include <iterator>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>
#include <concepts>
#include <limits>

namespace solver {


template <std::integral ValueType=int32_t>
SOLVER_LIBRARY_EXPORT class discrete_domain
{
private:
  static constexpr ValueType MIN_VALUE = std::numeric_limits<ValueType>::min();
  static constexpr ValueType MAX_VALUE = std::numeric_limits<ValueType>::max();
  using interval_type = typename boost::icl::interval_set<ValueType>::interval_type;
public:
  using value_type = ValueType;

  constexpr discrete_domain() = default;
  using iterator = typename boost::icl::interval_set<value_type>::element_const_iterator;
  explicit constexpr discrete_domain(value_type value) : m_set(value) {}
  constexpr discrete_domain &operator=(value_type value) {
    m_set.clear();
    m_set.insert(value);
    return *this;
  }

  [[nodiscard]] constexpr bool is_universal() const { return boost::icl::contains(m_set, UNIVERSAL_INTERVAL); }
  [[nodiscard]] constexpr bool empty() const { return m_set.empty(); }
  [[nodiscard]] constexpr bool is_singleton() const { return !is_universal() && m_set.size() == 1; }
  [[nodiscard]] iterator begin() const { return elements_begin(m_set);}
  [[nodiscard]] iterator end() const { return elements_end(m_set);}
  constexpr void clear() { m_set.clear(); }
  constexpr void erase(value_type value)
  {
    m_set.erase(value);
  }
  constexpr void erase(std::pair<value_type, value_type> interval)
  {
    m_set.erase(make_interval(interval.first, interval.second));
  }
  constexpr void insert(value_type value)
  {
    m_set.insert(value);
  }
  constexpr void insert(std::pair<value_type, value_type> interval)
  {
    m_set.insert(make_interval(interval.first, interval.second));
  }

  [[nodiscard]] constexpr bool contains(value_type value) const { return boost::icl::contains(m_set, value); }

  friend constexpr bool min(const discrete_domain & dom)
  {
    assert(!dom.empty());
    return dom.m_set.begin()->lower();
  }
  friend constexpr bool max(const discrete_domain & dom)
  {
    assert(!dom.empty());
    return dom.m_set.rbegin()->upper();
  }
  friend bool operator==(const discrete_domain & left, const discrete_domain &right) = default;
  friend bool operator==(const discrete_domain & left, value_type right) {
    return left.is_singleton() && left.contains(right);
  }
  [[nodiscard]] std::string to_string() const
  {
    std::ostringstream out;
    out << *this;
    return out.str();
  }

private:
  [[nodiscard]] static constexpr auto make_interval(value_type lower, value_type upper)
  {
    return interval_type::closed(lower, upper);
  }

  static const interval_type UNIVERSAL_INTERVAL; // NOLINT(cert-err58-cpp)
  boost::icl::interval_set<value_type> m_set{UNIVERSAL_INTERVAL};
};

template <std::integral ValueType>
[[nodiscard]] inline constexpr ValueType get_value(const discrete_domain<ValueType> &domain) { return min(domain); }


template <std::integral ValueType>
// NOLINTNEXTLINE(cert-err58-cpp)
const typename discrete_domain<ValueType>::interval_type discrete_domain<ValueType>::UNIVERSAL_INTERVAL =  discrete_domain<ValueType>::make_interval(MIN_VALUE, MAX_VALUE);

}// namespace solver

#endif
