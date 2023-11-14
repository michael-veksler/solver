#ifndef TRIVIAL_SAT_HPP
#define TRIVIAL_SAT_HPP

#include "binary_domain.hpp"
#include "sat_types.hpp"
#include <cassert>
#include <cinttypes>
#include <solver/solver_library_export.hpp>
#include <vector>

namespace solver {

/**
 * @brief A SAT solver with trivial search algorithm.
 *
 * This solver uses a simple search algorithm with no propagation or failure analysis.
 * It is intended only to be a testing-reference to be compared with a more efficient
 * solver.
 *
 */
SOLVER_LIBRARY_EXPORT class trivial_sat
{
private:
  static constexpr uint64_t default_max_attempts = static_cast<uint64_t>(1) << 32U;

public:
  SOLVER_LIBRARY_EXPORT class clause;
  using variable_handle = unsigned;
  SOLVER_LIBRARY_EXPORT explicit trivial_sat(uint64_t max_attempts = default_max_attempts);

  void reserve_vars(unsigned var_count) { m_domains.reserve(var_count); }
  [[nodiscard]] size_t num_vars() const { return m_domains.size(); }
  [[nodiscard]] variable_handle add_var(binary_domain domain = {})
  {
    m_domains.push_back(domain);
    return static_cast<variable_handle>(m_domains.size() - 1);
  }
  [[nodiscard]] binary_domain get_current_domain(variable_handle var) const { return m_domains[var]; }
  void reserve_clauses(unsigned clause_count);
  [[nodiscard]] clause &add_clause();

  solve_status solve();

  [[nodiscard]] bool get_variable_value(variable_handle var) const { return solver::get_value(m_domains[var]); }

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
  void add_literal(variable_handle var_num, bool is_positive)
  {
    m_literals.push_back(is_positive ? static_cast<int>(var_num) : -static_cast<int>(var_num));
  }
  [[nodiscard]] variable_handle get_variable(unsigned literal_num) const
  {
    assert(literal_num < m_literals.size()); // NOLINT
    int literal = m_literals[literal_num];
    return literal > 0 ? static_cast<variable_handle>(literal) : static_cast<variable_handle>(-literal);
  }
  [[nodiscard]] bool is_positive_literal(unsigned literal_num) const
  {
    assert(literal_num < m_literals.size()); // NOLINT
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

inline void trivial_sat::reserve_clauses(unsigned clause_count) { m_clauses.reserve(clause_count); }

/**
 * @brief Create a variables for the solver
 *
 * @param solver  The solver to work on
 * @param num_vars  The number of variables to create.
 * @return The created variables
 */
std::vector<trivial_sat::variable_handle> create_variables(trivial_sat &solver, unsigned num_vars);

}// namespace solver

#endif
