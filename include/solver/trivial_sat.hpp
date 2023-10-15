#ifndef TRIVIAL_SAT_HPP
#define TRIVIAL_SAT_HPP

#include "binary_domain.hpp"
#include <cassert>
#include <cinttypes>
#include <solver/solver_library_export.hpp>
#include <vector>

namespace solver {

enum class solve_status : int8_t { SAT, UNSAT, UNKNOWN };

SOLVER_LIBRARY_EXPORT class trivial_sat
{
private:
  static constexpr uint64_t default_max_attempts = static_cast<uint64_t>(1) << 32U;

public:
  SOLVER_LIBRARY_EXPORT class clause;

  SOLVER_LIBRARY_EXPORT explicit trivial_sat(uint64_t max_attempts = default_max_attempts);

  [[nodiscard]] unsigned add_var(binary_domain domain = {})
  {
    m_domains.push_back(domain);
    return static_cast<unsigned>(m_domains.size() - 1);
  }
  [[nodiscard]] clause &add_clause();

  solve_status solve();

  [[nodiscard]] bool get_value(unsigned var) const { return solver::get_value(m_domains[var]); }

private:
  [[nodiscard]] std::pair<solve_status, uint64_t> solve_recursive(std::vector<binary_domain>::iterator depth,
    uint64_t num_attempts) const;

  [[nodiscard]] bool has_conflict() const;

  [[nodiscard]] bool has_conflict(const clause &) const;

  uint64_t m_max_attempts;

  /**
   * @brief The domains of the CNF
   *
   * Positive literal values indicate \p true literals, and negatives indicate \p false literals.
   *
   * That's why, domain index 0 is unused, to make it easier to distinguish positive and negative literals.
   */
  std::vector<binary_domain> m_domains{ binary_domain{} };
  std::vector<clause> m_clauses;
};


class trivial_sat::clause
{
public:
  clause() = default;
  void reserve(unsigned num_literals) { m_literals.reserve(num_literals); }
  void add_literal(unsigned var_num, bool is_positive)
  {
    m_literals.push_back(is_positive ? static_cast<int>(var_num) : -static_cast<int>(var_num));
  }
  [[nodiscard]] unsigned get_variable(unsigned literal_num) const
  {
    assert(literal_num < m_literals.size());
    int literal = m_literals[literal_num];
    return literal > 0 ? static_cast<unsigned>(literal) : -static_cast<unsigned>(literal);
  }
  [[nodiscard]] bool is_positive_literal(unsigned literal_num) const
  {
    assert(literal_num < m_literals.size());
    return m_literals[literal_num] > 0;
  }
  [[nodiscard]] size_t size() const { return m_literals.size(); }


private:
  /**
   * @brief The literals of the CNF
   *
   * Positive values indicate \p true literals, and negatives indicate \p false literals.
   *
   * Variable 0 is unused, to make it easier to distinguish positive and negative literals.
   */
  std::vector<int> m_literals;
};

inline trivial_sat::trivial_sat(uint64_t max_attempts) : m_max_attempts(max_attempts) {}

inline trivial_sat::clause &trivial_sat::add_clause() { return m_clauses.emplace_back(); }

}// namespace solver

#endif
