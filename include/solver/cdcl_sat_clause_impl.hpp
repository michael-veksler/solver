#ifndef CDCL_SAT_CLAUSE_IMPL_HPP
#define CDCL_SAT_CLAUSE_IMPL_HPP

#include "solver/cdcl_sat_class.hpp"
#include "solver/cdcl_sat_clause_class.hpp"
#include "solver/domain_utils.hpp"
#include <map>

namespace solver {

template<cdcl_sat_strategy Strategy, sat_literal_type LiteralType>
auto cdcl_sat_clause<Strategy, LiteralType>::linear_find_free_literal(const cdcl_sat &solver,
  std::pair<literal_index_t, literal_index_t> literal_range) const -> literal_index_t
{
  assert(literal_range.first <= literal_range.second);// NOLINT

  for (literal_index_t literal_num = literal_range.first; literal_num != literal_range.second; ++literal_num) {
    const variable_handle var = get_variable(literal_num);
    const auto value = get_literal_value(literal_num);
    const domain_type &domain = solver.get_current_domain(var);
    if (domain.contains(value)) { return literal_num; }
  }
  return literal_range.second;
}

template<cdcl_sat_strategy Strategy, sat_literal_type LiteralType>
solve_status cdcl_sat_clause<Strategy,LiteralType>::literal_state(const cdcl_sat &solver, literal_index_t literal_num) const
{
  const variable_handle var = get_variable(literal_num);
  const auto value = get_literal_value(literal_num);
  const domain_type &domain = solver.get_current_domain(var);
  if (!domain.is_singleton()) {
    return solve_status::UNKNOWN;
  }
  return get_value(domain) == value_type(value) ? solve_status::SAT : solve_status::UNSAT;
}

template<cdcl_sat_strategy Strategy, sat_literal_type LiteralType>
bool cdcl_sat_clause<Strategy, LiteralType>::remove_duplicate_variables()
{
  std::map<uint32_t, this_literal_type> encountered_literals;
  std::vector<this_literal_type> replacement_literals;
  for (auto iter = m_literals.begin(); iter != m_literals.end(); ++iter) {
    auto [encountered_iter, inserted] = encountered_literals.try_emplace(iter->get_variable(), *iter);
    if (inserted) {
      if (!replacement_literals.empty()) { replacement_literals.push_back(*iter); }
    } else {
      if (encountered_iter->second != *iter) { return false; }
      if (replacement_literals.empty()) {
        replacement_literals.insert(replacement_literals.end(), m_literals.begin(), iter);
      }
    }
  }
  if (!replacement_literals.empty()) { m_literals = std::move(replacement_literals); }
  return true;
}

template<cdcl_sat_strategy Strategy, sat_literal_type LiteralType>
solve_status cdcl_sat_clause<Strategy, LiteralType>::initial_propagate(propagation_context propagation)
{
  if (!remove_duplicate_variables()) { return solve_status::SAT; }
  m_watched_literals = { 0, size() - 1 };
  m_watched_literals[0] =
    linear_find_free_literal(propagation.solver, { 0U, static_cast<literal_index_t>(m_literals.size()) });
  if (m_watched_literals[0] == m_literals.size()) {
    log_info(propagation.solver, "Trivially UNSAT clause {} = {}", propagation.clause, *this);
    return solve_status::UNSAT;
  }
  m_watched_literals[1] = linear_find_free_literal(
    propagation.solver, { m_watched_literals[0] + 1, static_cast<literal_index_t>(m_literals.size()) });
  if (m_watched_literals[1] == m_literals.size()) { return unit_propagate(propagation, m_watched_literals[0]); }
  for (auto watch : m_watched_literals) {
    propagation.solver.watch_value_removal(propagation.clause, get_variable(watch), get_literal_value(watch));
  }
  assert(m_watched_literals[0] < m_watched_literals[1] && m_watched_literals[1] < size());// NOLINT
  return solve_status::UNKNOWN;
}

template<cdcl_sat_strategy Strategy, sat_literal_type LiteralType>
auto cdcl_sat_clause<Strategy, LiteralType>::find_different_watch(const cdcl_sat &solver, unsigned watch_index) const
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

template<cdcl_sat_strategy Strategy, sat_literal_type LiteralType>
solve_status cdcl_sat_clause<Strategy, LiteralType>::propagate(propagation_context propagation, variable_handle triggering_var)
{
  assert(m_watched_literals[0] < m_watched_literals[1] && m_watched_literals[1] < size());// NOLINT
  log_info(propagation.solver, "propagating {} {}", propagation.clause, *this);

  const unsigned watch_index = get_variable(m_watched_literals[0]) == triggering_var ? 0 : 1;

  const literal_index_t next_watched_literal = find_different_watch(propagation.solver, watch_index);
  if (next_watched_literal != size()) {
    log_info(propagation.solver,
      "updating a watch of {} from {} to {}",
      propagation.clause,
      m_watched_literals.at(watch_index),
      next_watched_literal);
    propagation.solver.watch_value_removal(
      propagation.clause, get_variable(next_watched_literal), get_literal_value(next_watched_literal));
    m_watched_literals.at(watch_index) = next_watched_literal;
    if (m_watched_literals[0] > m_watched_literals[1]) { std::swap(m_watched_literals[0], m_watched_literals[1]); }
    return solve_status::UNKNOWN;
  }
  return unit_propagate(propagation, m_watched_literals.at(1 - watch_index));
}

template<cdcl_sat_strategy Strategy, sat_literal_type LiteralType>
solve_status cdcl_sat_clause<Strategy, LiteralType>::unit_propagate(propagation_context propagation,
  literal_index_t literal_num) const
{
  const variable_handle var = get_variable(literal_num);
  const auto value = get_literal_value(literal_num);
  const domain_type &domain = propagation.solver.get_current_domain(var);
  if (!domain.contains(value)) {
    log_info(
      propagation.solver, "conflicting literal {}", value ? static_cast<int>(var) : -static_cast<int>(var));

    return solve_status::UNSAT;
  }
  if (domain.is_singleton() && domain.contains(value)) {
    log_info(propagation.solver, "Trivially SAT literal {}", literal_num);
    return solve_status::SAT;
  }
  propagation.solver.set_domain(var, domain_type(value), propagation.clause);

  log_info(propagation.solver, "Propagating literal {} <-- {}", literal_num, value);
  return solve_status::SAT;
}

}// namespace solver
#endif
