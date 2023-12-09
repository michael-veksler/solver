#ifndef CDCL_SAT_IMPL_HPP
#define CDCL_SAT_IMPL_HPP

#include "solver/cdcl_sat_class.hpp"
#include "solver/cdcl_sat_clause_class.hpp"
#include "solver/domain_utils.hpp"
#include "solver/sat_types.hpp"
#include <boost/numeric/conversion/cast.hpp>
#include <solver/solver_library_export.hpp>
#include <solver/state_saver.hpp>

#include <map>

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
template<cdcl_sat_strategy Strategy>
auto cdcl_sat<Strategy>::analyze_conflict(clause_handle conflicting_clause)
  -> std::optional<std::pair<level_t, clause_handle>>
{
  conflict_analysis_algo algo(*this, conflicting_clause);
  while (true) {
    algo.resolve(algo.get_latest_implied_var());
    if (algo.empty() || algo.size() == 1 || algo.is_unit()) { break; }
  }
  log_info(*this, "conflict clause={}", algo);
  if (algo.size() == 1) {
    return { { 0, create_clause(algo) } };
  } else if (algo.is_unit()) {
    return { { algo.get_level(1), create_clause(algo) } };
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

template<cdcl_sat_strategy Strategy>
auto cdcl_sat<Strategy>::create_clause(const cdcl_sat_conflict_analysis_algo<Strategy> &conflict_analysis)
  -> clause_handle
{
  const auto ret = boost::numeric_cast<clause_handle>(m_clauses.size());
  clause &added = add_clause();
  for (auto [var_num, is_positive] : conflict_analysis.conflict_literals) { added.add_literal(var_num, is_positive); }
  return ret;
}

// implement log_clause
template<cdcl_sat_strategy Strategy>
void cdcl_sat<Strategy>::log_clause(clause_handle clause_handle, const std::string_view &prefix_text) const
{
  if (clause_handle >= m_clauses.size()) {
    log_info(*this, "{} {}=invalid", prefix_text, clause_handle);
  } else {
    log_info(*this, "{} {}={}", prefix_text, clause_handle, m_clauses.at(clause_handle));
  }
}

}// namespace solver
#endif
