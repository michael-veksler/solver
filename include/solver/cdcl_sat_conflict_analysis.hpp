#ifndef CDCL_SAT_CONFLICT_ANALYSIS_HPP
#define CDCL_SAT_CONFLICT_ANALYSIS_HPP

#include "solver/cdcl_sat_class.hpp"
#include "solver/cdcl_sat_clause_class.hpp"
#include "solver/domain_utils.hpp"
#include <boost/numeric/conversion/cast.hpp>

#include <map>

namespace solver {

/**
 * @brief Data-structure and algorithm used for conflict-analysis,
 *
 * It contains the currently generated clause, in the form of literals.
 */
template<cdcl_sat_strategy Strategy> struct cdcl_sat_conflict_analysis_algo
{
  using literal_index_t = typename Strategy::literal_index_t;
  using clause_handle = typename cdcl_sat<Strategy>::clause_handle;
  using variable_handle = typename cdcl_sat<Strategy>::variable_handle;
  using cdcl_sat = solver::cdcl_sat<Strategy>;
  using level_t = typename solver::cdcl_sat<Strategy>::level_t;
  using clause = cdcl_sat_clause<Strategy>;
  /**
   * @brief Construct a new conflict analysis algo object
   *
   * @param solver_in  A reference to the solver that had this conflict.
   * @param conflicting_clause_handle  The clause that detected the conflict.
   */
  cdcl_sat_conflict_analysis_algo(const cdcl_sat &solver_in, clause_handle conflicting_clause_handle);

  /**
   * @brief Analyze the conflict and generate the clause.
   *
   * @return true if the conflict was resolved, false if the problem is UNSAT.
   */
  [[nodiscard]] bool analyze_conflict();

  [[nodiscard]] std::string to_string() const;

  /**
   * @brief Get the decision level of the latest nt-th literal in the generated clause.
   *
   *
   * @param distance_from_latest The i-th oldest literal in the data-structure.
   *                             0 is the oldest, 1 is the almost oldest, etc.
   * @return The decision level of the i-th oldest literal.
   */
  [[nodiscard]] level_t get_level(unsigned distance_from_latest = 0) const
  {
    if (distance_from_latest >= implication_depth_to_var.size()) { return 0; }
    const variable_handle var = std::next(implication_depth_to_var.rbegin(), distance_from_latest)->second;
    return solver.get_var_decision_level(var);
  }

  template <std::invocable<variable_handle, bool> ApplyFunc>
  void foreach_conflict_literal(ApplyFunc &&func) const
  {
    for (const auto &[var, literal] : conflict_literals) { func(var, literal); }
  }

private:
  /**
   * @brief Is this a unit clause?
   *
   * A unit-clause is a clause that will perform an implication, i.e., reduce a domain, if we backtrack to the correct
   * level.
   *
   * @retval true if this is a unit-clause.
   */
  [[nodiscard]] bool is_unit() const
  {
    if (implication_depth_to_var.size() <= 1) { return true; }
    return get_level() != get_level(1);
  }
  /**
   * @brief Is this the empty clause, i.e., has no literals and implies the problem is UNSAT.
   *
   * @retval true if this is the empty clause.
   */
  [[nodiscard]] bool empty() const { return conflict_literals.empty(); }
  /**
   * @brief Get the number of literals in the currently generated clause.
   *
   * @return the number of literals
   */
  [[nodiscard]] size_t size() const { return conflict_literals.size(); }

  /**
   * @brief Perform a binary-clause resolution.
   *
   * Given the pivot_var, find which clause has implied its value, and resolve with that constraint, over this
   * pivot_var.
   *
   * @param pivot_var The pivot-var whose reducer will be resolved with the current literals.
   */
  void resolve(variable_handle pivot_var);

  /**
   * @brief Get the variable with the latest literal in this data-structure .
   *
   * @return The handle of the latest implied-variable.
   */
  [[nodiscard]] variable_handle get_latest_implied_var() const { return implication_depth_to_var.rbegin()->second; }

  const cdcl_sat &solver;
  std::map<variable_handle, bool> conflict_literals;
  std::map<variable_handle, variable_handle> implication_depth_to_var;
};

}// namespace solver
#endif
