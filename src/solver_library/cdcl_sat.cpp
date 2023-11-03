#include "solver/binary_domain.hpp"
#include "solver/sat_types.hpp"
#include <boost/numeric/conversion/cast.hpp>
#include <solver/cdcl_sat.hpp>
#include <solver/state_saver.hpp>

#include <algorithm>
#include <cstdint>
#include <map>
#include <optional>
#include <ranges>
#include <set>
#include <stdexcept>
#include <tuple>

#include "spdlog/spdlog.h"

namespace solver {

template<typename... Args>
void inline log_info(const cdcl_sat & solver, spdlog::format_string_t<Args...> fmt, Args &&...args)
{
  if (solver.get_debug())
  {
    spdlog::info(fmt, std::forward<Args>(args)...);
  }
}

std::vector<cdcl_sat::variable_handle> create_variables(cdcl_sat &solver, unsigned num_vars)
{
  std::vector<cdcl_sat::variable_handle> variables;// NOLINT
  variables.reserve(num_vars);
  std::generate_n(std::back_inserter(variables), num_vars, [&solver] { return solver.add_var(); });
  return variables;
}

void cdcl_sat::set_domain(variable_handle var, binary_domain domain, clause_handle by_clause)
{
  spdlog::info("L{}: Setting var{} := {} by clause={}", get_level(), var, domain.to_string(), by_clause);
  if (m_domains[var] != domain) {
    m_domains[var] = domain;
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
struct cdcl_sat::conflict_analysis_algo
{
  /**
   * @brief Construct a new conflict analysis algo object
   *
   * @param solver_in  A reference to the solver that had this conflict.
   * @param conflicting_clause_handle  The clause that detected the conflict.
   */
  conflict_analysis_algo(cdcl_sat &solver_in, clause_handle conflicting_clause_handle);

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

cdcl_sat::conflict_analysis_algo::conflict_analysis_algo(cdcl_sat &solver_in, clause_handle conflicting_clause_handle)
  : solver(solver_in)
{
  const clause &conflicting_clause = solver.m_clauses.at(conflicting_clause_handle);
  log_info(solver, "initiating conflict analysis with conflict_clause {}={}",
    conflicting_clause_handle,
    conflicting_clause.to_string());
  for (clause::literal_index_t literal_num = 0; literal_num != conflicting_clause.size(); ++literal_num) {
    const variable_handle var = conflicting_clause.get_variable(literal_num);
    const variable_handle implication_depth = solver.m_implications[var].implication_depth;
    if (implication_depth == 0) { continue; }
    [[maybe_unused]] const auto [literal_iter, was_inserted] =
      conflict_literals.emplace(var, conflicting_clause.is_positive_literal(literal_num));
    assert(was_inserted);// NOLINT
    implication_depth_to_var.emplace(implication_depth, var);
  }
  log_info(solver, "cl={}", to_string());
}

void cdcl_sat::conflict_analysis_algo::resolve(variable_handle pivot_var)
{
  const implication imp = solver.m_implications.at(pivot_var);
  const clause &prev_clause = solver.m_clauses.at(imp.implication_cause);
  log_info(solver, "Resolving with {}={}", imp.implication_cause, prev_clause.to_string());
  for (clause::literal_index_t literal_num = 0; literal_num != prev_clause.size(); ++literal_num) {
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
  log_info(solver, "cl={}", to_string());
}

std::string cdcl_sat::conflict_analysis_algo::to_string() const
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


[[nodiscard]] auto cdcl_sat::analyze_conflict(clause_handle conflicting_clause)
  -> std::optional<std::pair<level_t, clause_handle>>
{
  conflict_analysis_algo algo(*this, conflicting_clause);
  while (true) {
    algo.resolve(algo.get_latest_implied_var());
    if (algo.empty()) {
      return std::nullopt;
    } else if (algo.size() == 1) {
      return { { 0, algo.create_clause() } };
    } else if (algo.is_unit()) {
      return { { algo.get_level(1), algo.create_clause() } };
    }
  }
}

[[nodiscard]] solve_status cdcl_sat::solve()
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
#ifdef ENABLE_LOG_CDCL_SAT
        std::string all;
        for (unsigned i = 1; i != m_domains.size(); ++i) {
          all += "V" + std::to_string(i) + '=' + std::to_string(static_cast<int>(m_domains[i].contains(true))) + ", ";
        }
        log_info(*this, "solution: {}", all);
#endif
        validate_all_singletons();
        return solve_status::SAT;
      }
    }
  }
}

void cdcl_sat::validate_all_singletons() const
{
  const auto not_singleton_iter = std::find_if(
    std::next(m_domains.begin()), m_domains.end(), [](binary_domain domain) { return !domain.is_singleton(); });
  if (not_singleton_iter != m_domains.end()) {
    throw std::runtime_error("Bug: var=" + std::to_string(std::distance(m_domains.begin(), not_singleton_iter))
                             + " should be singleton at a SAT solution");
  }
}

auto cdcl_sat::find_free_var(variable_handle search_start) const -> std::optional<variable_handle>
{
  for (variable_handle var = search_start; var != m_domains.size(); ++var) {
    if (m_domains[var].is_universal()) { return var; }
  }
  if (search_start > 1) {
    for (variable_handle var = 1; var != search_start; ++var) {
      if (m_domains[var].is_universal()) { return var; }
    }
  }
  return std::nullopt;
}

bool cdcl_sat::make_choice()
{
  const std::optional<variable_handle> chosen =
    m_chosen_vars.empty() ? find_free_var(1) : find_free_var(m_chosen_vars.back());

  if (!chosen) {
    log_info(*this, "Nothing to choose");
    return false;
  }
  m_chosen_vars.push_back(*chosen);
  set_domain(*chosen, binary_domain(false), implication::DECISION);
  assert(m_implications[*chosen].level == get_level());
  log_info(*this, "Chosen var{} := 0", *chosen);
  return true;
}


bool cdcl_sat::initial_propagate()
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
    if (status == solve_status::UNSAT) { return false; }
  }
  const std::optional<clause_handle> conflicting_clause = propagate();
  return !conflicting_clause;
}

auto cdcl_sat::propagate() -> std::optional<clause_handle>
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

auto cdcl_sat::clause::linear_find_free_literal(const cdcl_sat &solver,
  std::pair<literal_index_t, literal_index_t> literal_range) const -> literal_index_t
{
  assert(literal_range.first <= literal_range.second);// NOLINT

  for (literal_index_t literal_num = literal_range.first; literal_num != literal_range.second; ++literal_num) {
    const variable_handle var = get_variable(literal_num);
    const bool is_positive = is_positive_literal(literal_num);
    const binary_domain domain = solver.get_current_domain(var);
    if (domain.contains(is_positive)) { return literal_num; }
  }
  return literal_range.second;
}

solve_status cdcl_sat::clause::literal_state(const cdcl_sat &solver, literal_index_t literal_num) const
{
  const variable_handle var = get_variable(literal_num);
  const bool is_positive = is_positive_literal(literal_num);
  const binary_domain domain = solver.get_current_domain(var);
  if (domain == binary_domain(is_positive)) {
    return solve_status::SAT;
  } else if (domain == binary_domain(!is_positive)) {
    return solve_status::UNSAT;
  } else {
    return solve_status::UNKNOWN;
  }
}

bool cdcl_sat::clause::remove_duplicate_variables()
{
  std::set<int> encountered_literals;
  std::vector<int> replacement_literals;
  for (auto iter = m_literals.begin(); iter != m_literals.end(); ++iter) {
    if (encountered_literals.contains(*iter)) {
      if (replacement_literals.empty()) {
        replacement_literals.insert(replacement_literals.end(), m_literals.begin(), iter);
      }
      continue;
    }
    if (encountered_literals.contains(-*iter)) { return false; }
    encountered_literals.insert(*iter);
    if (!replacement_literals.empty()) { replacement_literals.push_back(*iter); }
  }
  if (!replacement_literals.empty()) { m_literals = std::move(replacement_literals); }
  return true;
}

solve_status cdcl_sat::clause::initial_propagate(propagation_context propagation)
{
  if (!remove_duplicate_variables()) { return solve_status::SAT; }
  m_watched_literals = { 0, size() - 1 };
  m_watched_literals[0] =
    linear_find_free_literal(propagation.solver, { 0U, static_cast<literal_index_t>(m_literals.size()) });
  if (m_watched_literals[0] == m_literals.size()) { return solve_status::UNSAT; }
  m_watched_literals[1] = linear_find_free_literal(
    propagation.solver, { m_watched_literals[0] + 1, static_cast<literal_index_t>(m_literals.size()) });
  if (m_watched_literals[1] == m_literals.size()) { return unit_propagate(propagation, m_watched_literals[0]); }
  for (auto watch : m_watched_literals) {
    propagation.solver.watch_value_removal(propagation.clause, get_variable(watch), is_positive_literal(watch));
  }
  assert(m_watched_literals[0] < m_watched_literals[1] && m_watched_literals[1] < size());// NOLINT
  return solve_status::UNKNOWN;
}

auto cdcl_sat::clause::find_different_watch(const cdcl_sat &solver, unsigned watch_index) const -> literal_index_t
{
  const literal_index_t watched_literal = m_watched_literals.at(watch_index);
  const literal_index_t other_watched_literal = m_watched_literals.at(1 - watch_index);
  assert(literal_state(solver, watched_literal) == solve_status::UNSAT);
  for (literal_index_t literal_num = watched_literal + 1; literal_num != size(); ++literal_num) {
    if (literal_num != other_watched_literal && literal_state(solver, literal_num) != solve_status::UNSAT) {
      return literal_num;
    }
  }
  for (literal_index_t literal_num = 0; literal_num != watched_literal; ++literal_num) {
    if (literal_num != other_watched_literal && literal_state(solver, literal_num) != solve_status::UNSAT) {
      return literal_num;
    }
  }
  return size();
}


solve_status cdcl_sat::clause::propagate(propagation_context propagation, variable_handle triggering_var)
{
  assert(m_watched_literals[0] < m_watched_literals[1] && m_watched_literals[1] < size());// NOLINT
  log_info(propagation.solver, "propagating {} {}", propagation.clause, to_string());

  const unsigned watch_index = get_variable(m_watched_literals[0]) == triggering_var ? 0 : 1;

  const literal_index_t next_watched_literal = find_different_watch(propagation.solver, watch_index);
  if (next_watched_literal != size()) {
    log_info(propagation.solver, "updating a watch of {} from {} to {}",
      propagation.clause,
      m_watched_literals.at(watch_index),
      next_watched_literal);
    propagation.solver.watch_value_removal(
      propagation.clause, get_variable(next_watched_literal), is_positive_literal(next_watched_literal));
    m_watched_literals.at(watch_index) = next_watched_literal;
    if (m_watched_literals[0] > m_watched_literals[1]) { std::swap(m_watched_literals[0], m_watched_literals[1]); }
    return solve_status::UNKNOWN;
  }
  switch (literal_state(propagation.solver, m_watched_literals.at(1 - watch_index))) {
  case solve_status::UNSAT:
    return solve_status::UNSAT;
  case solve_status::SAT:
    return solve_status::SAT;
  case solve_status::UNKNOWN:
    return unit_propagate(propagation, m_watched_literals.at(1 - watch_index));
  }
  abort();
}

solve_status cdcl_sat::clause::unit_propagate(propagation_context propagation, literal_index_t literal_num) const
{
  const variable_handle var = get_variable(literal_num);
  const bool is_positive = is_positive_literal(literal_num);
  const binary_domain domain = propagation.solver.get_current_domain(var);
  if (!domain.contains(is_positive)) {
    log_info(propagation.solver, "conflicting literal {}", literal_num);

    return solve_status::UNSAT;
  }
  if (domain == binary_domain(is_positive)) {
    log_info(propagation.solver, "Trivially SAT literal {}", literal_num);
    return solve_status::SAT;
  }
  propagation.solver.set_domain(var, binary_domain(is_positive), propagation.clause);

  log_info(propagation.solver, "Propagating literal {} <-- {}", literal_num, is_positive);
  return solve_status::SAT;
}

void cdcl_sat::backtrack(level_t backtrack_level)
{
  assert(get_level() > 0);
  log_info(*this, "Backtrack to level {}", backtrack_level);
  while (!m_implied_vars.empty()) {
    const variable_handle reset_var = m_implied_vars.back();
    if (m_implications[reset_var].level <= backtrack_level) { break; }
    log_info(*this, "Resetting var{}", reset_var);
    m_implied_vars.pop_back();
    m_domains[reset_var] = binary_domain();
    m_implications[reset_var] = implication();
  }
  m_chosen_vars.resize(backtrack_level);
  m_dirty_variables.clear();
}

}// namespace solver
