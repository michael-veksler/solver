#ifndef BINARY_DOMAIN_HPP
#define BINARY_DOMAIN_HPP

#include <cassert>
#include <cinttypes>
#include <iterator>
#include <solver/solver_library_export.hpp>
#include <vector>

namespace solver {

SOLVER_LIBRARY_EXPORT class binary_domain_iterator;
SOLVER_LIBRARY_EXPORT class binary_domain
{
public:
  constexpr binary_domain() = default;
  using iterator = binary_domain_iterator;
  explicit constexpr binary_domain(bool value) : m_zero(!value), m_one(value) {}
  [[nodiscard]] constexpr bool is_universal() const { return m_one && m_zero; }
  [[nodiscard]] constexpr bool empty() const { return !m_one && !m_zero; }
  [[nodiscard]] constexpr bool is_singleton() const { return m_one != m_zero; }
  [[nodiscard]] iterator begin() const;
  [[nodiscard]] iterator end() const;
  constexpr void clear() { m_zero = m_one = 0; }
  constexpr void erase(bool value)
  {
    if (value) {
      assert(m_one);
      m_one = false;
    } else {
      assert(m_zero);
      m_zero = false;
    }
  }
  constexpr void insert(bool value)
  {
    if (value) {
      m_one = true;
    } else {
      m_zero = true;
    }
  }

  [[nodiscard]] constexpr bool contains(bool value) const { return static_cast<bool>(value ? m_one : m_zero); }

  friend constexpr bool min(binary_domain dom) { return dom.m_zero == 0; }
  friend constexpr bool max(binary_domain dom) { return dom.m_one == 1; }
  friend constexpr bool operator==(binary_domain left, binary_domain right) = default;

private:
  uint8_t m_zero : 1 = 1;
  uint8_t m_one : 1 = 1;
};


class binary_domain_iterator
{
public:
  using iterator_category = std::bidirectional_iterator_tag;
  using difference_type = int8_t;
  using value_type = bool;
  using pointer = void;
  using reference = bool;
  explicit binary_domain_iterator(binary_domain dom, bool is_end = false) : m_whole_domain(dom), m_values_left(dom)
  {
    if (is_end) { m_values_left.clear(); }
  }

  binary_domain_iterator()
  {
    m_whole_domain.clear();
    m_values_left.clear();
  }

  binary_domain_iterator &operator++()
  {
    assert(!m_values_left.empty());
    m_values_left.erase(**this);
    return *this;
  }
  binary_domain_iterator &operator--()
  {
    assert(m_values_left != m_whole_domain);
    if (max(m_whole_domain) == max(m_values_left)) {
      m_values_left.insert(min(m_whole_domain));
    } else {
      m_values_left.insert(max(m_whole_domain));
    }
    return *this;
  }

  [[nodiscard]] bool operator*() const { return min(m_values_left); }
  [[nodiscard]] friend bool operator==(binary_domain_iterator left, binary_domain_iterator right) = default;


private:
  binary_domain m_whole_domain;
  binary_domain m_values_left;
};

inline binary_domain::iterator binary_domain::begin() const { return iterator{ *this }; }
inline binary_domain::iterator binary_domain::end() const { return iterator{ *this, true }; }

}// namespace solver

#endif
