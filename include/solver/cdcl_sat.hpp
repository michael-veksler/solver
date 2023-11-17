#ifndef CDCL_SAT_HPP
#define CDCL_SAT_HPP

#include "sat_types.hpp"
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

template<typename T>
concept cdcl_sat_strategy = requires(T t) {
                              typename T::domain_type;
                              requires domain_concept<typename T::domain_type>;
                            };

template<typename DomainType> struct domain_strategy
{
  using domain_type = DomainType;
};


/**
 * @brief A Conflict-Driven Clause-Learning SAT solver.
 *
 */
template<cdcl_sat_strategy Strategy> SOLVER_LIBRARY_EXPORT class cdcl_sat
{
private:
  static constexpr uint64_t default_max_backtracks = static_cast<uint64_t>(1) << 32U;
  using clause_handle = uint32_t;
  using watch_container = std::vector<clause_handle>;

public:
  SOLVER_LIBRARY_EXPORT class clause;
  using variable_handle = uint32_t;
  using level_t = variable_handle;
  using domain_type = typename Strategy::domain_type;
  SOLVER_LIBRARY_EXPORT explicit cdcl_sat(uint64_t max_backtracks = default_max_backtracks);
  void set_debug(bool is_debug) { m_debug = is_debug; }
  [[nodiscard]] bool get_debug() const { return m_debug; }

  [[nodiscard]] uint64_t get_max_backtracks() const { return m_max_backtracks; }

  void reserve_vars(unsigned var_count) { m_domains.reserve(var_count); }
  [[nodiscard]] size_t num_vars() const { return m_domains.size(); }
  [[nodiscard]] variable_handle add_var(domain_type domain = { 0, 1 })
  {
    m_domains.push_back(std::move(domain));
    return static_cast<variable_handle>(m_domains.size() - 1);
  }

  void reserve_clauses(unsigned clause_count);

  [[nodiscard]] clause &add_clause();

  solve_status solve();

  [[nodiscard]] bool get_variable_value(variable_handle var) const { return get_value(m_domains[var]); }

  [[nodiscard]] size_t count_variables() const { return m_domains.size(); }

  [[nodiscard]] const domain_type &get_current_domain(variable_handle var) const { return m_domains[var]; }
  /**
   * @brief Set the domain of a variable.
   *
   * During solving, this also updates the historical implication-data, used for conflict analysis.
   *
   * @param var   The variable to update.
   * @param domain  The domain to set to the variable.
   * @param by_clause If set, this is the trigger of the update (either a decision of a clause).
   */
  void set_domain(variable_handle var, domain_type domain, clause_handle by_clause = implication::DECISION);

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
  std::vector<domain_type> m_domains{ domain_type() };
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


template<cdcl_sat_strategy Strategy> class cdcl_sat<Strategy>::clause
{
public:
  using literal_index_t = uint32_t;
  struct propagation_context
  {
    cdcl_sat &solver;
    clause_handle clause;
  };

  clause() = default;
  clause(const clause &) = delete;
  clause(clause &&) noexcept = default;
  clause &operator=(const clause &) = delete;
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
  if (get_debug()) {
    if (by_clause == implication::DECISION) {
      spdlog::info( "L{}: Setting var{} := {} by DECISION", get_level(), var, to_string(domain));
    } else {
      spdlog::info( "L{}: Setting var{} := {} by clause={}", get_level(), var, to_string(domain), by_clause);
    }
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
template<cdcl_sat_strategy Strategy> struct cdcl_sat<Strategy>::conflict_analysis_algo
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

template<cdcl_sat_strategy Strategy>
cdcl_sat<Strategy>::conflict_analysis_algo::conflict_analysis_algo(cdcl_sat &solver_in,
  clause_handle conflicting_clause_handle)
  : solver(solver_in)
{
  const clause &conflicting_clause = solver.m_clauses.at(conflicting_clause_handle);
  if (solver.get_debug()) {
    spdlog::info(
      "initiating conflict analysis with conflicting_clause {}={}",
      conflicting_clause_handle,
      conflicting_clause.to_string());
  }
  for (typename clause::literal_index_t literal_num = 0; literal_num != conflicting_clause.size(); ++literal_num) {
    const variable_handle var = conflicting_clause.get_variable(literal_num);
    const variable_handle implication_depth = solver.m_implications[var].implication_depth;
    if (implication_depth == 0) { continue; }
    [[maybe_unused]] const auto [literal_iter, was_inserted] =
      conflict_literals.emplace(var, conflicting_clause.is_positive_literal(literal_num));
    assert(was_inserted);// NOLINT
    implication_depth_to_var.emplace(implication_depth, var);
  }
  if (solver.get_debug()) { spdlog::info( "cl={}", to_string()); }
}

template<cdcl_sat_strategy Strategy> void cdcl_sat<Strategy>::conflict_analysis_algo::resolve(variable_handle pivot_var)
{
  const implication imp = solver.m_implications.at(pivot_var);
  const clause &prev_clause = solver.m_clauses.at(imp.implication_cause);
  if (solver.get_debug()) { spdlog::info( "Resolving with {}={}", imp.implication_cause, prev_clause.to_string()); }
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
  if (solver.get_debug()) { spdlog::info( "cl={}", to_string()); }
}

template<cdcl_sat_strategy Strategy> std::string cdcl_sat<Strategy>::conflict_analysis_algo::to_string() const
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

template<cdcl_sat_strategy Strategy>
auto cdcl_sat<Strategy>::analyze_conflict(clause_handle conflicting_clause)
  -> std::optional<std::pair<level_t, clause_handle>>
{
  conflict_analysis_algo algo(*this, conflicting_clause);
  while (true) {
    algo.resolve(algo.get_latest_implied_var());
    if (algo.empty() || algo.size() == 1 || algo.is_unit()) { break; }
  }
  if (get_debug()) { spdlog::info( "conflict clause={}", algo.to_string()); }
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
      log_info(*this, "Trivially UNSAT clause {} = {}", handle, m_clauses[handle].to_string());
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

template<cdcl_sat_strategy Strategy>
auto cdcl_sat<Strategy>::clause::linear_find_free_literal(const cdcl_sat<Strategy> &solver,
  std::pair<literal_index_t, literal_index_t> literal_range) const -> literal_index_t
{
  assert(literal_range.first <= literal_range.second);// NOLINT

  for (literal_index_t literal_num = literal_range.first; literal_num != literal_range.second; ++literal_num) {
    const variable_handle var = get_variable(literal_num);
    const bool is_positive = is_positive_literal(literal_num);
    const domain_type &domain = solver.get_current_domain(var);
    if (domain.contains(is_positive)) { return literal_num; }
  }
  return literal_range.second;
}

template<cdcl_sat_strategy Strategy>
solve_status cdcl_sat<Strategy>::clause::literal_state(const cdcl_sat &solver, literal_index_t literal_num) const
{
  const variable_handle var = get_variable(literal_num);
  const bool is_positive = is_positive_literal(literal_num);
  const domain_type &domain = solver.get_current_domain(var);
  if (domain.is_singleton() && get_value(domain) == (is_positive ? 1 : 0)) {
    return solve_status::SAT;
  } else if (domain.is_singleton() && get_value(domain) == (is_positive ? 0 : 1)) {
    return solve_status::UNSAT;
  } else {
    return solve_status::UNKNOWN;
  }
}

template<cdcl_sat_strategy Strategy> bool cdcl_sat<Strategy>::clause::remove_duplicate_variables()
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

template<cdcl_sat_strategy Strategy>
solve_status cdcl_sat<Strategy>::clause::initial_propagate(propagation_context propagation)
{
  if (!remove_duplicate_variables()) { return solve_status::SAT; }
  m_watched_literals = { 0, size() - 1 };
  m_watched_literals[0] =
    linear_find_free_literal(propagation.solver, { 0U, static_cast<literal_index_t>(m_literals.size()) });
  if (m_watched_literals[0] == m_literals.size()) {
    if (propagation.solver.get_debug()) {
      spdlog::info("Trivially UNSAT clause {} = {}", propagation.clause, to_string());
    }
    return solve_status::UNSAT;
  }
  m_watched_literals[1] = linear_find_free_literal(
    propagation.solver, { m_watched_literals[0] + 1, static_cast<literal_index_t>(m_literals.size()) });
  if (m_watched_literals[1] == m_literals.size()) { return unit_propagate(propagation, m_watched_literals[0]); }
  for (auto watch : m_watched_literals) {
    propagation.solver.watch_value_removal(propagation.clause, get_variable(watch), is_positive_literal(watch));
  }
  assert(m_watched_literals[0] < m_watched_literals[1] && m_watched_literals[1] < size());// NOLINT
  return solve_status::UNKNOWN;
}

template<cdcl_sat_strategy Strategy>
auto cdcl_sat<Strategy>::clause::find_different_watch(const cdcl_sat &solver, unsigned watch_index) const
  -> literal_index_t
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

template<cdcl_sat_strategy Strategy>
solve_status cdcl_sat<Strategy>::clause::propagate(propagation_context propagation, variable_handle triggering_var)
{
  assert(m_watched_literals[0] < m_watched_literals[1] && m_watched_literals[1] < size());// NOLINT
  if (propagation.solver.get_debug()) {
    spdlog::info("propagating {} {}", propagation.clause, to_string());
  }

  const unsigned watch_index = get_variable(m_watched_literals[0]) == triggering_var ? 0 : 1;

  const literal_index_t next_watched_literal = find_different_watch(propagation.solver, watch_index);
  if (next_watched_literal != size()) {
    log_info(propagation.solver,
      "updating a watch of {} from {} to {}",
      propagation.clause,
      m_watched_literals.at(watch_index),
      next_watched_literal);
    propagation.solver.watch_value_removal(
      propagation.clause, get_variable(next_watched_literal), is_positive_literal(next_watched_literal));
    m_watched_literals.at(watch_index) = next_watched_literal;
    if (m_watched_literals[0] > m_watched_literals[1]) { std::swap(m_watched_literals[0], m_watched_literals[1]); }
    return solve_status::UNKNOWN;
  }
  return unit_propagate(propagation, m_watched_literals.at(1 - watch_index));
}

template<cdcl_sat_strategy Strategy>
solve_status cdcl_sat<Strategy>::clause::unit_propagate(propagation_context propagation,
  literal_index_t literal_num) const
{
  const variable_handle var = get_variable(literal_num);
  const bool is_positive = is_positive_literal(literal_num);
  const domain_type &domain = propagation.solver.get_current_domain(var);
  if (!domain.contains(is_positive)) {
    log_info(
      propagation.solver, "conflicting literal {}", is_positive ? static_cast<int>(var) : -static_cast<int>(var));

    return solve_status::UNSAT;
  }
  if (domain.is_singleton() && domain.contains(is_positive)) {
    log_info(propagation.solver, "Trivially SAT literal {}", literal_num);
    return solve_status::SAT;
  }
  propagation.solver.set_domain(var, domain_type(is_positive), propagation.clause);

  log_info(propagation.solver, "Propagating literal {} <-- {}", literal_num, is_positive);
  return solve_status::SAT;
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
