#ifndef CDCL_SAT_HPP
#define CDCL_SAT_HPP

#include "solver/cdcl_sat_class.hpp"
#include "solver/cdcl_sat_clause_class.hpp"
#include "solver/cdcl_sat_clause_impl.hpp"
#include "solver/domain_utils.hpp"
#include "solver/sat_types.hpp"
#include <boost/numeric/conversion/cast.hpp>
#include <solver/solver_library_export.hpp>
#include <solver/state_saver.hpp>

#include <algorithm>
#include <array>
#include <cassert>
#include <cinttypes>
#include <cstdint>
#include <deque>
#include <limits>
#include <map>
#include <optional>
#include <ranges>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <variant>
#include <vector>

#include "spdlog/spdlog.h"

namespace solver {
template<cdcl_sat_strategy Strategy>
inline cdcl_sat<Strategy>::cdcl_sat(uint64_t max_backtracks) : m_max_backtracks(max_backtracks)
{}

template<cdcl_sat_strategy Strategy> inline void cdcl_sat<Strategy>::reserve_clauses(unsigned int clause_count)
{
  m_clauses.reserve(clause_count);
}

template<cdcl_sat_strategy Strategy> inline auto cdcl_sat<Strategy>::add_clause() -> cdcl_sat::clause &
{
  return m_clauses.emplace_back();
}

template<cdcl_sat_strategy Strategy, typename... Args>
void inline log_info(const cdcl_sat<Strategy> &solver, spdlog::format_string_t<Args...> fmt, Args &&...args)
{
  if (solver.get_debug()) { spdlog::info(fmt, std::forward<Args>(args)...); }
}

/**
 * @brief Create a variables for the solver
 *
 * @param solver  The solver to work on
 * @param num_vars  The number of variables to create.
 * @return The created variables
 */

template<cdcl_sat_strategy Strategy>
std::vector<typename cdcl_sat<Strategy>::variable_handle> create_variables(cdcl_sat<Strategy> &solver,
  unsigned num_vars)
{
  std::vector<typename cdcl_sat<Strategy>::variable_handle> variables;// NOLINT
  variables.reserve(num_vars);
  std::generate_n(std::back_inserter(variables), num_vars, [&solver] { return solver.add_var(); });
  return variables;
}

template<cdcl_sat_strategy Strategy>
void cdcl_sat<Strategy>::set_domain(variable_handle var, domain_type domain, clause_handle by_clause)
{
  if (by_clause == implication::DECISION) {
    log_info(*this,  "L{}: Setting var{} := {} by DECISION", get_level(), var, domain);
  } else {
    log_info(*this,  "L{}: Setting var{} := {} by clause={}", get_level(), var, domain, by_clause);
  }
  if (m_domains[var] != domain) {
    m_domains[var] = std::move(domain);
    if (m_inside_solve) {
      m_dirty_variables.push_back(var);
      m_implied_vars.push_back(var);
      m_implications[var] = { .implication_cause = by_clause,
        .implication_depth = boost::numeric_cast<variable_handle>(m_implied_vars.size()),
        .level = get_level() };
    }
  }
}

/**
 * @brief Data-structure and algorithm used for conflict-analysis,
 *
 * It contains the currently generated clause, in the form of literals.
 */
template<cdcl_sat_strategy Strategy> struct cdcl_sat_conflict_analysis_algo
{
  using clause_handle = typename cdcl_sat<Strategy>::clause_handle;
  using variable_handle = typename cdcl_sat<Strategy>::variable_handle;
  using cdcl_sat = solver::cdcl_sat<Strategy>;
  using level_t = typename solver::cdcl_sat<Strategy>::level_t;
  using clause = cdcl_sat_clause<Strategy>;
  using implication = typename solver::cdcl_sat<Strategy>::implication;
  /**
   * @brief Construct a new conflict analysis algo object
   *
   * @param solver_in  A reference to the solver that had this conflict.
   * @param conflicting_clause_handle  The clause that detected the conflict.
   */
  cdcl_sat_conflict_analysis_algo(cdcl_sat &solver_in, clause_handle conflicting_clause_handle);

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
    const variable_handle var = std::next(implication_depth_to_var.rbegin(), distance_from_latest)->second;
    return solver.m_implications[var].level;
  }
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
   * @brief Add and populate a clause with the current list of literals.
   *
   * @return clause_handle The handle of the newly created clause.
   */
  [[nodiscard]] clause_handle create_clause() const
  {
    const auto ret = boost::numeric_cast<clause_handle>(solver.m_clauses.size());
    clause &added = solver.add_clause();
    for (auto [var_num, is_positive] : conflict_literals) { added.add_literal(var_num, is_positive); }
    return ret;
  }

  /**
   * @brief Get the variable with the latest literal in this data-structure .
   *
   * @return The handle of the latest implied-variable.
   */
  [[nodiscard]] variable_handle get_latest_implied_var() const { return implication_depth_to_var.rbegin()->second; }
  [[nodiscard]] std::string to_string() const;

  cdcl_sat &solver;
  std::map<variable_handle, bool> conflict_literals;
  std::map<variable_handle, variable_handle> implication_depth_to_var;
};

template<cdcl_sat_strategy Strategy> std::string cdcl_sat_conflict_analysis_algo<Strategy>::to_string() const
{
  std::string ret = "{";
  const char *sep = "";
  for (auto [var, is_positive] : conflict_literals) {
    ret += sep;
    const level_t level = solver.m_implications[var].level;
    ret += is_positive ? std::to_string(var) : '-' + std::to_string(var);
    ret += '@' + std::to_string(level);
    sep = ", ";
  }
  ret += '}';
  return ret;
}

} // namespace solver


template<solver::cdcl_sat_strategy Strategy> struct fmt::formatter<solver::cdcl_sat_conflict_analysis_algo<Strategy>> : fmt::formatter<std::string> {
  template<typename FormatContext>
  auto format(const solver::cdcl_sat_conflict_analysis_algo<Strategy> &algo, FormatContext &ctx)
  {
    return formatter<std::string>::format(algo.to_string(), ctx);
  }
};


namespace solver {

template<cdcl_sat_strategy Strategy>
cdcl_sat_conflict_analysis_algo<Strategy>::cdcl_sat_conflict_analysis_algo(cdcl_sat &solver_in,
  clause_handle conflicting_clause_handle)
  : solver(solver_in)
{
  const clause &conflicting_clause = solver.m_clauses.at(conflicting_clause_handle);
  log_info(
    solver,
    "initiating conflict analysis with conflicting_clause {}={}",
    conflicting_clause_handle,
    conflicting_clause);
  for (typename clause::literal_index_t literal_num = 0; literal_num != conflicting_clause.size(); ++literal_num) {
    const variable_handle var = conflicting_clause.get_variable(literal_num);
    const variable_handle implication_depth = solver.m_implications[var].implication_depth;
    if (implication_depth == 0) { continue; }
    [[maybe_unused]] const auto [literal_iter, was_inserted] =
      conflict_literals.emplace(var, conflicting_clause.is_positive_literal(literal_num));
    assert(was_inserted);// NOLINT
    implication_depth_to_var.emplace(implication_depth, var);
  }
  log_info(solver, "cl={}", *this);
}

template<cdcl_sat_strategy Strategy> void cdcl_sat_conflict_analysis_algo<Strategy>::resolve(variable_handle pivot_var)
{
  const implication imp = solver.m_implications.at(pivot_var);
  const clause &prev_clause = solver.m_clauses.at(imp.implication_cause);
  log_info(solver, "Resolving with {}={}", imp.implication_cause, prev_clause);
  for (typename clause::literal_index_t literal_num = 0; literal_num != prev_clause.size(); ++literal_num) {
    const bool is_positive = prev_clause.is_positive_literal(literal_num);
    const variable_handle var = prev_clause.get_variable(literal_num);
    const variable_handle implication_depth = solver.m_implications[var].implication_depth;
    if (implication_depth == 0) { continue; }
    if (var == pivot_var) {
      assert(conflict_literals.at(var) != is_positive);
      conflict_literals.erase(var);
      implication_depth_to_var.erase(implication_depth);
    } else {
      auto [insert_iter, was_inserted] = conflict_literals.try_emplace(var, is_positive);
      if (was_inserted) {
        implication_depth_to_var.emplace(implication_depth, var);
      } else {
        assert(insert_iter->second == is_positive);
      }
    }
  }
  log_info(solver, "cl={}", *this);
}

template<cdcl_sat_strategy Strategy>
auto cdcl_sat<Strategy>::analyze_conflict(clause_handle conflicting_clause)
  -> std::optional<std::pair<level_t, clause_handle>>
{
  conflict_analysis_algo algo(*this, conflicting_clause);
  while (true) {
    algo.resolve(algo.get_latest_implied_var());
    if (algo.empty() || algo.size() == 1 || algo.is_unit()) { break; }
  }
  log_info(*this,  "conflict clause={}", algo);
  if (algo.size() == 1) {
    return { { 0, algo.create_clause() } };
  } else if (algo.is_unit()) {
    return { { algo.get_level(1), algo.create_clause() } };
  }
  return std::nullopt;
}

template<cdcl_sat_strategy Strategy> solve_status cdcl_sat<Strategy>::solve()
{
  const state_saver inside_solve_saver(m_inside_solve);
  m_inside_solve = true;
  if (!initial_propagate()) { return solve_status::UNSAT; }
  unsigned backtracks = 0;
  while (true) {
    std::optional<clause_handle> conflicting_clause = propagate();
    if (conflicting_clause) {
      if (get_level() == 0) {
        log_info(*this, "Failed at level 0, no solution possible");
        return solve_status::UNSAT;
      }
      std::optional<std::pair<level_t, clause_handle>> conflict_info = analyze_conflict(*conflicting_clause);
      if (!conflict_info) {
        log_info(*this, "Conflict analysis detected the empty clause, no solution possible");
        return solve_status::UNSAT;
      }
      auto [level, learned_clause] = *conflict_info;
      log_info(*this, "Backtrack to level {}, generated clause={}", level, learned_clause);
      if (backtracks == m_max_backtracks) { return solve_status::UNKNOWN; }

      backtrack(level);
      const auto conflict_clause = boost::numeric_cast<clause_handle>(m_clauses.size() - 1);
      [[maybe_unused]] const solve_status status =
        m_clauses.back().initial_propagate({ .solver = *this, .clause = conflict_clause });
      assert(status == solve_status::SAT);
      ++backtracks;
    } else {
      if (!make_choice()) {
        validate_all_singletons();
        return solve_status::SAT;
      }
    }
  }
}

template<cdcl_sat_strategy Strategy> void cdcl_sat<Strategy>::validate_all_singletons() const
{
  const auto not_singleton_iter = std::find_if(
    std::next(m_domains.begin()), m_domains.end(), [](const domain_type &domain) { return !domain.is_singleton(); });
  if (not_singleton_iter != m_domains.end()) {
    throw std::runtime_error("Bug: var=" + std::to_string(std::distance(m_domains.begin(), not_singleton_iter))
                             + " should be singleton at a SAT solution");
  }
}

template<cdcl_sat_strategy Strategy>
auto cdcl_sat<Strategy>::find_free_var(variable_handle search_start) const -> std::optional<variable_handle>
{
  for (variable_handle var = search_start; var != m_domains.size(); ++var) {
    if (!m_domains[var].is_singleton()) { return var; }
  }
  if (search_start > 1) {
    for (variable_handle var = 1; var != search_start; ++var) {
      if (!m_domains[var].is_singleton()) { return var; }
    }
  }
  return std::nullopt;
}

template<cdcl_sat_strategy Strategy> bool cdcl_sat<Strategy>::make_choice()
{
  const std::optional<variable_handle> chosen =
    m_chosen_vars.empty() ? find_free_var(1) : find_free_var(m_chosen_vars.back());

  if (!chosen) {
    log_info(*this, "Nothing to choose");
    return false;
  }
  m_chosen_vars.push_back(*chosen);
  set_domain(*chosen, domain_type(false), implication::DECISION);
  assert(m_implications[*chosen].level == get_level());
  return true;
}

template<cdcl_sat_strategy Strategy> bool cdcl_sat<Strategy>::initial_propagate()
{
  m_dirty_variables.clear();
  m_implications.clear();
  m_implications.resize(m_domains.size());
  m_implied_vars.clear();
  m_implied_vars.clear();
  for (auto &signed_watches : m_watches) {
    signed_watches.clear();
    signed_watches.resize(m_domains.size());
  }
  for (clause_handle handle = 0; handle != m_clauses.size(); ++handle) {
    const solve_status status = m_clauses[handle].initial_propagate({ .solver = *this, .clause = handle });
    if (status == solve_status::UNSAT) {
      log_info(*this, "Trivially UNSAT clause {} = {}", handle, m_clauses[handle]);
      return false;
    }
  }
  const std::optional<clause_handle> conflicting_clause = propagate();
  return !conflicting_clause;
}

template<cdcl_sat_strategy Strategy> auto cdcl_sat<Strategy>::propagate() -> std::optional<clause_handle>
{
  while (!m_dirty_variables.empty()) {
    const variable_handle var = m_dirty_variables.front();
    m_dirty_variables.pop_front();
    assert(get_current_domain(var).is_singleton());// NOLINT
    const bool positive_erased = !get_current_domain(var).contains(true);
    std::vector<clause_handle> &dirty_clauses = m_watches.at(static_cast<unsigned>(positive_erased))[var];
    for (size_t dirty_clauses_index = 0; dirty_clauses_index != dirty_clauses.size();) {
      const clause_handle dirty_clause = dirty_clauses[dirty_clauses_index];
      const solve_status status = m_clauses[dirty_clause].propagate({ .solver = *this, .clause = dirty_clause }, var);
      const bool watch_was_replaced = (status == solve_status::UNKNOWN);
      if (watch_was_replaced) {
        dirty_clauses[dirty_clauses_index] = dirty_clauses.back();
        dirty_clauses.pop_back();
      } else if (status == solve_status::UNSAT) {
        return dirty_clause;
      } else {
        ++dirty_clauses_index;
      }
    }
  }
  return std::nullopt;
}

template<cdcl_sat_strategy Strategy> void cdcl_sat<Strategy>::backtrack(level_t backtrack_level)
{
  assert(get_level() > 0);
  log_info(*this, "Backtrack to level {}", backtrack_level);
  while (!m_implied_vars.empty()) {
    const variable_handle reset_var = m_implied_vars.back();
    if (m_implications[reset_var].level <= backtrack_level) { break; }
    log_info(*this, "Resetting var{}", reset_var);
    m_implied_vars.pop_back();
    m_domains[reset_var] = domain_type();
    m_implications[reset_var] = implication();
  }
  m_chosen_vars.resize(backtrack_level);
  m_dirty_variables.clear();
}

}// namespace solver
#endif
