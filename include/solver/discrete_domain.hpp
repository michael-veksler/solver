#ifndef DISCRETE_DOMAIN_HPP
#define DISCRETE_DOMAIN_HPP

#include <boost/icl/discrete_interval.hpp>
#include <boost/icl/interval_set.hpp>
#include <solver/domain_utils.hpp>
#include <solver/solver_library_export.hpp>

#include <cassert>
#include <cinttypes>
#include <concepts>
#include <iterator>
#include <limits>
#include <string>
#include <type_traits>
#include <vector>


namespace solver {


template<std::integral ValueType = int32_t> SOLVER_LIBRARY_EXPORT class discrete_domain
{
private:
  using interval_type = typename boost::icl::interval_set<ValueType>::interval_type;

public:
  using value_type = ValueType;
  static constexpr ValueType MIN_VALUE = std::numeric_limits<ValueType>::min();

  /// The value is smaller than max() because are bugs in isl when it deals with max()
  static constexpr ValueType MAX_VALUE = std::numeric_limits<ValueType>::max() - 1;

  constexpr discrete_domain() = default;
  using iterator = typename boost::icl::interval_set<value_type>::element_const_iterator;
  explicit constexpr discrete_domain(value_type value) : m_set() { *this = value; }

  // cppcheck-suppress noExplicitConstructor ; cppcheck and clang-tidy disagree on this
  constexpr discrete_domain(std::initializer_list<value_type> values) : m_set()
  {
    for (auto value : values) { insert(value); }
  }

  constexpr discrete_domain &operator=(value_type value)
  {
    validate_inserted_value(value);
    m_set.clear();
    m_set.insert(value);
    return *this;
  }

  [[nodiscard]] constexpr bool is_universal() const { return boost::icl::contains(m_set, get_universal_interval()); }
  [[nodiscard]] constexpr bool empty() const { return m_set.empty(); }
  [[nodiscard]] constexpr bool is_singleton() const { return !is_universal() && m_set.size() == 1; }
  [[nodiscard]] iterator begin() const { return elements_begin(m_set); }
  [[nodiscard]] iterator end() const { return elements_end(m_set); }
  constexpr void clear() { m_set.clear(); }
  constexpr void erase(value_type value) { m_set.erase(value); }
  constexpr void erase(std::pair<value_type, value_type> interval)
  {
    m_set.erase(make_interval(interval.first, interval.second));
  }
  constexpr void insert(value_type value)
  {
    validate_inserted_value(value);
    m_set.insert(value);
  }
  constexpr void insert(std::pair<value_type, value_type> interval)
  {
    if (interval.second > MAX_VALUE) { throw std::invalid_argument("Value is too big"); }
    if (interval.first > interval.second) { throw std::invalid_argument("Invalid interval"); }
    m_set.insert(make_interval(interval.first, interval.second));
  }

  [[nodiscard]] constexpr bool contains(value_type value) const { return boost::icl::contains(m_set, value); }

  friend constexpr value_type min(const discrete_domain &dom)
  {
    assert(!dom.empty());
    return dom.m_set.begin()->lower();
  }
  friend constexpr value_type max(const discrete_domain &dom)
  {
    assert(!dom.empty());
    return dom.m_set.rbegin()->upper();
  }
  friend bool operator==(const discrete_domain &left, const discrete_domain &right) = default;
  friend bool operator==(const discrete_domain &left, value_type right)
  {
    return left.is_singleton() && left.contains(right);
  }

private:
  static void validate_inserted_value(value_type value)
  {
    if (value > MAX_VALUE) { throw std::invalid_argument("Value is too big"); }
  }

  [[nodiscard]] static constexpr auto make_interval(value_type lower, value_type upper)
  {
    return interval_type::closed(lower, upper);
  }

  static constexpr interval_type get_universal_interval() { return make_interval(MIN_VALUE, MAX_VALUE); }
  boost::icl::interval_set<value_type> m_set{ get_universal_interval() };
};

static_assert(domain_concept<discrete_domain<int32_t>>);
static_assert(domain_concept<discrete_domain<uint8_t>>);

}// namespace solver

#endif
