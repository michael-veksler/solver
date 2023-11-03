#ifndef CDCL_SAT_HPP
#define CDCL_SAT_HPP

#include "binary_domain.hpp"
#include "sat_types.hpp"
#include <limits>
#include <solver/solver_library_export.hpp>

#include <array>
#include <cassert>
#include <cinttypes>
#include <deque>
#include <limits>
#include <optional>
#include <string>
#include <variant>
#include <vector>


namespace solver {
/**
 * @brief A Conflict-Driven Clause-Learning SAT solver.
 *
 */
SOLVER_LIBRARY_EXPORT class cdcl_sat
{
private:
  static constexpr uint64_t default_max_backtracks = static_cast<uint64_t>(1) << 32U;
  using clause_handle = uint32_t;
  using watch_container = std::vector<clause_handle>;

public:
  SOLVER_LIBRARY_EXPORT class clause;
  using variable_handle = uint32_t;
  using level_t = variable_handle;
  SOLVER_LIBRARY_EXPORT explicit cdcl_sat(uint64_t max_backtracks = default_max_backtracks);
  void set_debug(bool is_debug) { m_debug = is_debug; }
  [[nodiscard]] bool get_debug() const { return m_debug; }

  [[nodiscard]] uint64_t get_max_backtracks() const { return m_max_backtracks; }

  void reserve_vars(unsigned var_count) { m_domains.reserve(var_count); }
  [[nodiscard]] size_t num_vars() const { return m_domains.size(); }
  [[nodiscard]] variable_handle add_var(binary_domain domain = {})
  {
    m_domains.push_back(domain);
    return static_cast<variable_handle>(m_domains.size() - 1);
  }

  void reserve_clauses(unsigned clause_count);

  [[nodiscard]] clause &add_clause();

  solve_status solve();

  [[nodiscard]] bool get_value(variable_handle var) const { return solver::get_value(m_domains[var]); }

  [[nodiscard]] size_t count_variables() const { return m_domains.size(); }

  [[nodiscard]] binary_domain get_current_domain(variable_handle var) const { return m_domains[var]; }
  /**
   * @brief Set the domain of a variable.
   *
   * During solving, this also updates the historical implication-data, used for conflict analysis.
   *
   * @param var   The variable to update.
   * @param domain  The domain to set to the variable.
   * @param by_clause If set, this is the trigger of the update (either a decision of a clause).
   */
  void set_domain(variable_handle var, binary_domain domain, clause_handle by_clause = implication::DECISION);

  void watch_value_removal(clause_handle this_clause, variable_handle watched_var, bool watched_value)
  {
    assert(watched_var < m_watches[watched_value].size());// NOLINT
    m_watches.at(static_cast<unsigned>(watched_value))[watched_var].push_back(this_clause);
  }

  /**
   * @brief Get current decision level.
   *
   * The decision level is the number of active decision, i.e., the search-points at which
   * the algorithm made a decision branch.
   *
   * @return The current decision level.
   */
  [[nodiscard]] level_t get_level() const
  {
    assert(m_chosen_vars.size() <= std::numeric_limits<level_t>::max());
    return static_cast<level_t>(m_chosen_vars.size());
  }

private:
  /**
   * @brief Holds historic information about a single implication.
   *
   * An implication is the act of propagating a single clause that ends with the
   * reduction, i.e., implication, of a single variable's domain.
   */
  struct implication
  {
    static constexpr clause_handle DECISION = static_cast<clause_handle>(-1LL);

    /**
     * @brief Who caused the implication.
     *
     * It can be either one of:
     *   - DECISION: A decision has been made.
     *   - clause: A clause has made the domain reduction.
     */
    clause_handle implication_cause = DECISION;

    /**
     * @brief The index of this implication in m_implied_vars
     */
    variable_handle implication_depth = 0;
    /**
     * @brief The decision level that this implication belongs to.
     *
     * When this implication was made, then \p level == \p m_chosen_vars.size()
     */
    level_t level = 0;
  };
  struct conflict_analysis_algo;
  friend struct conflict_analysis_algo;

  /**
   * @brief Propagate all clauses to a fix-point.
   *
   * @retval std::nullopt if all is well.
   * @retval clause_handle if this clause is the conflicting-clause, i.e., it couldn't be satisfied.
   */
  [[nodiscard]] std::optional<clause_handle> propagate();
  [[nodiscard]] bool initial_propagate();
  [[nodiscard]] bool make_choice();

  /**
   * @brief Analyze the current propagation-conflict
   *
   * @param conflicting_clause  The clause which was unsatisfied during the propagation.
   * @retval std::nullopt if the algorithm detects that the problem is unsatisfiable.
   * @retval level,clause_handle  \p clause_handle is the learned conflict-clause, and \p level is the
   *                              earliest level at which \p clause_handle will make an implication to avert a similar
   *                              failure.
   */
  [[nodiscard]] std::optional<std::pair<level_t, clause_handle>> analyze_conflict(clause_handle conflicting_clause);

  /**
   * @brief Backtrack to the given level.
   *
   * Undo all implication and decisions done after the provided level, but keep all learned conflict-clauses clauses.
   * All decisions and implications done before this requested level are kept intact.
   *
   * @param backtrack_level  The level to backtrack-to.
   */
  void backtrack(level_t backtrack_level);
  [[nodiscard]] std::optional<variable_handle> find_free_var(variable_handle search_start) const;
  void validate_all_singletons() const;

  uint64_t m_max_backtracks;
  bool m_debug = false;
  bool m_inside_solve = false;

  /**
   * @brief The domains of the CNF
   *
   * Positive literal values indicate \p true literals, and negatives indicate \p false literals.
   *
   * That's why, domain index 0 is unused, to make it easier to distinguish positive and negative literals.
   */
  std::vector<binary_domain> m_domains{ binary_domain{} };
  /**
   * @brief The implications and decisions of the solver.
   *
   * m_implications[var] are the implications of the given variable.
   *
   * @invariant forall i,var in enumerate(m_implied_vars): m_implications[var].implication_depth
   */
  std::vector<implication> m_implications{ implication{} };
  std::array<std::vector<watch_container>, 2> m_watches;
  std::deque<variable_handle> m_dirty_variables;

  std::vector<clause> m_clauses;
  std::vector<variable_handle> m_implied_vars;
  std::vector<variable_handle> m_chosen_vars;
};


class cdcl_sat::clause
{
public:
  using literal_index_t = uint32_t;
  struct propagation_context
  {
    cdcl_sat &solver;
    clause_handle clause;
  };

  clause() = default;
  void reserve(literal_index_t num_literals) { m_literals.reserve(num_literals); }
  void add_literal(variable_handle var_num, bool is_positive)
  {
    m_literals.push_back(is_positive ? static_cast<int>(var_num) : -static_cast<int>(var_num));
  }
  [[nodiscard]] solve_status initial_propagate(propagation_context propagation);
  [[nodiscard]] solve_status propagate(propagation_context propagation, variable_handle triggering_var);
  [[nodiscard]] variable_handle get_variable(literal_index_t literal_num) const
  {
    assert(literal_num < m_literals.size());// NOLINT
    int literal = m_literals[literal_num];
    return literal > 0 ? static_cast<variable_handle>(literal) : static_cast<variable_handle>(-literal);
  }
  [[nodiscard]] bool is_positive_literal(literal_index_t literal_num) const
  {
    assert(literal_num < m_literals.size());// NOLINT
    return m_literals[literal_num] > 0;
  }
  [[nodiscard]] literal_index_t size() const
  {
    assert(m_literals.size() <= std::numeric_limits<literal_index_t>::max());// NOLINT
    return static_cast<literal_index_t>(m_literals.size());
  }

  [[nodiscard]] std::string to_string() const
  {
    std::string ret = "{ ";
    for (unsigned i = 0; i != m_literals.size(); ++i) {
      if (i != 0) { ret += ", "; }
      ret += std::to_string(m_literals[i]);
      if (i == m_watched_literals[0] || i == m_watched_literals[1]) { ret += '*'; }
    }
    ret += '}';
    return ret;
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
  std::vector<int> m_literals;
  std::array<literal_index_t, 2> m_watched_literals = { 0, 0 };
};

inline cdcl_sat::cdcl_sat(uint64_t max_backtracks) : m_max_backtracks(max_backtracks) {}

inline void cdcl_sat::reserve_clauses(unsigned int clause_count) { m_clauses.reserve(clause_count); }

inline cdcl_sat::clause &cdcl_sat::add_clause() { return m_clauses.emplace_back(); }

/**
 * @brief Create a variables for the solver
 *
 * @param solver  The solver to work on
 * @param num_vars  The number of variables to create.
 * @return The created variables
 */
std::vector<cdcl_sat::variable_handle> create_variables(cdcl_sat &solver, unsigned num_vars);

}// namespace solver
#endif
