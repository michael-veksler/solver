#include "solver/binary_domain.hpp"
#include "solver/sat_types.hpp"
#include <solver/cdcl_sat.hpp>
#include <solver/state_saver.hpp>

#include "spdlog/spdlog.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <tuple>

namespace solver {

#define ENABLE_LOG_CDCL_SAT

#ifdef ENABLE_LOG_CDCL_SAT
#define LOG_CDCL_SAT_INFO spdlog::info
#else
#define LOG_CDCL_SAT_INFO(...)
#endif

std::vector<cdcl_sat::variable_handle> create_variables(cdcl_sat &solver, unsigned num_vars)
{
  std::vector<cdcl_sat::variable_handle> variables;// NOLINT
  variables.reserve(num_vars);
  std::generate_n(std::back_inserter(variables), num_vars, [&solver] { return solver.add_var(); });
  return variables;
}

void cdcl_sat::set_domain(variable_handle var, binary_domain domain)
{
  LOG_CDCL_SAT_INFO("Setting var{} := {}", var, domain.to_string());
  if (m_domains[var] != domain) {
    m_domains[var] = domain;
    if (m_inside_solve) {
      m_dirty_variables.push_back(var);
      m_changed_var_by_order.push_back(var);
    }
  }
}


solve_status cdcl_sat::solve()
{
  state_saver inside_solve_saver(m_inside_solve);
  m_inside_solve = true;
  if (!initial_propagate()) { return solve_status::UNSAT; }
  unsigned backtracks = 0;
  while (true) {
    if (propagate()) {
      if (!make_choice()) {
#ifdef ENABLE_LOG_CDCL_SAT
        std::string all;
        for (unsigned i = 1; i != m_domains.size(); ++i) {
          all += "V" + std::to_string(i) + '=' + std::to_string(static_cast<int>(m_domains[i].contains(true))) + ", ";
        }
        LOG_CDCL_SAT_INFO("solution: {}", all);
#endif
        validate_all_singletons();
        return solve_status::SAT;
      }
    } else {
      if (get_level() == 0) {
        LOG_CDCL_SAT_INFO("Backtrack level{}: no chosen variable", get_level());
        return solve_status::UNSAT;
      }
      if (backtracks == m_max_backtracks) { return solve_status::UNKNOWN; }

      backtrack();
      ++backtracks;
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
    m_chosen_var_by_order.empty() ? find_free_var(1) : find_free_var(m_chosen_var_by_order.back());

  if (!chosen) {
    LOG_CDCL_SAT_INFO("Nothing to choose");
    return false;
  }
  m_chosen_var_by_order.push_back(*chosen);
  set_domain(*chosen, binary_domain(false));
  LOG_CDCL_SAT_INFO("Chosen var{} = false", *chosen);
  return true;
}


bool cdcl_sat::initial_propagate()
{
  for (auto &signed_watches : m_watches) {
    signed_watches.clear();
    signed_watches.resize(m_domains.size());
  }
  for (clause_handle handle = 0; handle != m_clauses.size(); ++handle) {
    const solve_status status = m_clauses[handle].initial_propagate(*this, handle);
    if (status == solve_status::UNSAT) { return false; }
  }
  return propagate();
}

bool cdcl_sat::propagate()
{
  while (!m_dirty_variables.empty()) {
    const variable_handle var = m_dirty_variables.front();
    m_dirty_variables.pop_front();
    assert(get_current_domain(var).is_singleton());// NOLINT
    const bool positive_erased = !get_current_domain(var).contains(true);
    std::vector<clause_handle> &dirty_clauses = m_watches.at(static_cast<unsigned>(positive_erased))[var];
    for (size_t current = 0; current != dirty_clauses.size();) {
      const clause_handle dirty_clause = dirty_clauses[current];
      const solve_status status =
        m_clauses[dirty_clause].propagate(*this, { .this_clause = dirty_clause, .triggering_var = var });
      const bool watch_was_replaced = (status == solve_status::UNKNOWN);
      if (watch_was_replaced) {
        dirty_clauses[current] = dirty_clauses.back();
        dirty_clauses.pop_back();
      } else if (status == solve_status::UNSAT) {
        return false;
      } else {
        ++current;
      }
    }
  }
  return true;
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

solve_status cdcl_sat::clause::initial_propagate(cdcl_sat &solver, clause_handle this_clause)
{
  m_watched_literals = { 0, size() - 1 };
  m_watched_literals[0] = linear_find_free_literal(solver, { 0U, m_literals.size() });
  if (m_watched_literals[0] == m_literals.size()) { return solve_status::UNSAT; }
  m_watched_literals[1] = linear_find_free_literal(solver, { m_watched_literals[0] + 1, m_literals.size() });
  if (m_watched_literals[1] == m_literals.size()) { return unit_propagate(solver, m_watched_literals[0]); }
  for (auto watch : m_watched_literals) {
    solver.watch_value_removal(this_clause, get_variable(watch), is_positive_literal(watch));
  }
  assert(m_watched_literals[0] < m_watched_literals[1] && m_watched_literals[1] < size());// NOLINT
  return solve_status::UNKNOWN;
}

auto cdcl_sat::clause::find_different_watch(cdcl_sat &solver, unsigned watch_index) const -> literal_index_t
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


solve_status cdcl_sat::clause::propagate(cdcl_sat &solver, propagation_trigger trigger)
{
  assert(m_watched_literals[0] < m_watched_literals[1] && m_watched_literals[1] < size());// NOLINT
  LOG_CDCL_SAT_INFO("propagating {} {}", trigger.this_clause, to_string());

  const unsigned watch_index = get_variable(m_watched_literals[0]) == trigger.triggering_var ? 0 : 1;

  const literal_index_t next_watched_literal = find_different_watch(solver, watch_index);
  if (next_watched_literal != size()) {
    LOG_CDCL_SAT_INFO("updating a watch of {} from {} to {}",
      trigger.this_clause,
      m_watched_literals.at(watch_index),
      next_watched_literal);
    solver.watch_value_removal(
      trigger.this_clause, get_variable(next_watched_literal), is_positive_literal(next_watched_literal));
    m_watched_literals.at(watch_index) = next_watched_literal;
    if (m_watched_literals[0] > m_watched_literals[1]) { std::swap(m_watched_literals[0], m_watched_literals[1]); }
    return solve_status::UNKNOWN;
  }
  switch (literal_state(solver, m_watched_literals.at(1 - watch_index))) {
  case solve_status::UNSAT:
    return solve_status::UNSAT;
  case solve_status::SAT:
    return solve_status::SAT;
  case solve_status::UNKNOWN:
    return unit_propagate(solver, m_watched_literals.at(1 - watch_index));
  }
  abort();
}

solve_status cdcl_sat::clause::unit_propagate(cdcl_sat &solver, literal_index_t literal_num) const
{
  const variable_handle var = get_variable(literal_num);
  const bool is_positive = is_positive_literal(literal_num);
  const binary_domain domain = solver.get_current_domain(var);
  if (!domain.contains(is_positive)) {
    LOG_CDCL_SAT_INFO("conflicting literal {}", literal_num);

    return solve_status::UNSAT;
  }
  if (domain == binary_domain(is_positive)) {
    LOG_CDCL_SAT_INFO("Trivially SAT literal {}", literal_num);
    return solve_status::SAT;
  }
  solver.set_domain(var, binary_domain(is_positive));
  LOG_CDCL_SAT_INFO("Propagating literal {} <-- {}", literal_num, is_positive);
  return solve_status::SAT;
}

void cdcl_sat::backtrack()
{
  assert(get_level() > 0);
  variable_handle var = m_chosen_var_by_order.back();
  LOG_CDCL_SAT_INFO("Backtrack level{}: reset from variable: {} = false", m_chosen_var_by_order.size(), var);
  m_chosen_var_by_order.pop_back();
  while (!m_changed_var_by_order.empty() && m_changed_var_by_order.back() != var) {
    m_domains[m_changed_var_by_order.back()] = binary_domain();
    m_changed_var_by_order.pop_back();
  }
  m_dirty_variables.clear();
  assert(!m_changed_var_by_order.empty());
  const binary_domain complement(m_domains[var] != binary_domain(true));
  set_domain(var, complement);
}

}// namespace solver
