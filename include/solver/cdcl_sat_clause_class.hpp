#ifndef CDCL_SAT_CLAUSE_CLASS_HPP
#define CDCL_SAT_CLAUSE_CLASS_HPP

#include "solver/binary_literal_type.hpp"
#include "solver/cdcl_sat_class.hpp"
#include <solver/solver_library_export.hpp>

namespace solver {
template<cdcl_sat_strategy Strategy, sat_literal_type LiteralType> class cdcl_sat_clause
{
public:
  using literal_index_t = typename Strategy::literal_index_t;
  using cdcl_sat = solver::cdcl_sat<Strategy>;
  using clause_handle = typename cdcl_sat::clause_handle;
  using variable_handle = typename cdcl_sat::variable_handle;
  using domain_type = typename cdcl_sat::domain_type;
  using this_literal_type = LiteralType;
  struct propagation_context
  {
    cdcl_sat &solver;
    clause_handle clause;
  };

  cdcl_sat_clause() = default;
  cdcl_sat_clause(const cdcl_sat_clause &) = delete;
  cdcl_sat_clause(cdcl_sat_clause &&) noexcept = default;
  cdcl_sat_clause &operator=(const cdcl_sat_clause &) = delete;
  cdcl_sat_clause &operator=(const cdcl_sat_clause &&) = delete;
  ~cdcl_sat_clause() = default;
  void reserve(literal_index_t num_literals) { m_literals.reserve(num_literals); }
  void add_literal(variable_handle var_num, bool is_positive)
  {
    m_literals.push_back(binary_literal_type{var_num, is_positive});
  }

  [[nodiscard]] solve_status initial_propagate(propagation_context propagation);
  [[nodiscard]] solve_status propagate(propagation_context propagation, variable_handle triggering_var);

  [[nodiscard]] variable_handle get_variable(literal_index_t literal_num) const
  {
    assert(literal_num < m_literals.size());// NOLINT
    return m_literals[literal_num].get_variable();
  }
  [[nodiscard]] typename LiteralType::value_type get_literal_value(literal_index_t literal_num) const
  {
    assert(literal_num < m_literals.size());// NOLINT
    return m_literals[literal_num].get_value();
  }

  [[nodiscard]] literal_index_t size() const
  {
    assert(m_literals.size() <= std::numeric_limits<literal_index_t>::max());// NOLINT
    return static_cast<literal_index_t>(m_literals.size());
  }

  friend std::ostream &operator<<(std::ostream &out, const cdcl_sat_clause<Strategy, binary_literal_type> &clause)
  {
    out << '{';
    for (unsigned i = 0; i != clause.m_literals.size(); ++i) {
      if (i != 0) { out << ", "; }
      out << clause.m_literals[i];
      if (i == clause.m_watched_literals[0] || i == clause.m_watched_literals[1]) { out << '*'; }
    }
    return out << '}';
  }

private:
  [[nodiscard]] literal_index_t linear_find_free_literal(const cdcl_sat &solver,
    std::pair<literal_index_t, literal_index_t> literal_range) const;
  [[nodiscard]] literal_index_t find_different_watch(const cdcl_sat &solver, unsigned watch_index) const;

  [[nodiscard]] solve_status unit_propagate(propagation_context propagation, literal_index_t literal_num) const;
  [[nodiscard]] solve_status literal_state(const cdcl_sat &solver, literal_index_t literal_num) const;
  /**
   * @brief Remove all duplicate variables.
   *
   * @retval true all went well
   * @return false Can't be removed since it could change semantics, i.e.,
                   there are both positive and negative literals for the same variable, making the clause a tautology.
   */
  [[nodiscard]] bool remove_duplicate_variables();

  /**
   * @brief The literals of the CNF
   *
   * Positive values indicate \p true literals, and negatives indicate \p false literals.
   *
   * Variable 0 is unused, to make it easier to distinguish positive and negative literals.
   */
  std::vector<this_literal_type> m_literals;
  std::array<literal_index_t, 2> m_watched_literals = { 0, 0 };
};

} // namespace solver

template<solver::cdcl_sat_strategy Strategy> struct fmt::formatter<solver::cdcl_sat_clause<Strategy, solver::binary_literal_type>> : fmt::ostream_formatter {};

#endif
