#include <solver/trivial_sat.hpp>

#include "solver/binary_domain.hpp"
#include <solver/state_saver.hpp>

#include <algorithm>
#include <ranges>
#include <tuple>

namespace solver {

std::vector<trivial_sat::variable_handle> create_variables(trivial_sat &solver, unsigned num_vars)
{
  std::vector<trivial_sat::variable_handle> variables;
  variables.reserve(num_vars);
  std::generate_n(std::back_inserter(variables), num_vars, [&solver] { return solver.add_var(); });
  return variables;
}


solve_status trivial_sat::solve()
{
  const uint64_t initial_num_attempts = 0;
  validate_clauses();
  auto [status, num_attempts] = solve_recursive(std::next(m_domains.begin()), initial_num_attempts);
  std::ignore = num_attempts;
  return status;
}

void trivial_sat::validate_clauses() const
{
  for (const clause &tested : m_clauses) {
    for (unsigned i = 0; i != tested.size(); ++i) {
      const auto variable = tested.get_variable(i);
      if(variable >= m_domains.size()) {
        throw std::out_of_range(fmt::format("Variable index out of range for clause {}", tested));
      }
    }
    if (has_conflict(tested)) { throw std::logic_error("Clause has conflict"); }
  }
}

std::pair<solve_status, uint64_t> trivial_sat::solve_recursive(std::vector<binary_domain>::iterator depth,// NOLINT
  uint64_t num_attempts) const
{
  if (has_conflict()) {
    const solve_status stat = num_attempts >= m_max_attempts ?
        solve_status::UNKNOWN :
        solve_status::UNSAT;
    return { stat, num_attempts + 1 };
  }
  for (; depth != m_domains.end(); ++depth) {
    if (!depth->is_universal()) { continue; }
    state_saver saved_domain(*depth);
    for (const bool value : binary_domain()) {
    *depth = value;
    auto [status, next_num_attempts] = solve_recursive(std::next(depth), num_attempts);
    num_attempts = next_num_attempts;
    if (status == solve_status::SAT) {
        saved_domain.reset();
        return { solve_status::SAT, num_attempts };
    } else if (status == solve_status::UNKNOWN) {
        return { solve_status::UNKNOWN, num_attempts };
    }
    }
    return { solve_status::UNSAT, num_attempts };
  }
  return { solve_status::SAT, num_attempts };
}

bool trivial_sat::has_conflict() const
{
  return std::any_of(m_clauses.begin(), m_clauses.end(), [this](const clause &tested) { return has_conflict(tested); });
}

bool trivial_sat::has_conflict(const clause &tested) const
{
  for (unsigned i = 0; i != tested.size(); ++i) {
    const unsigned variable = tested.get_variable(i);
    const bool is_positive = tested.is_positive_literal(i);
    assert(variable < m_domains.size()); // NOLINT
    if (m_domains[variable].contains(is_positive)) { return false; }
  }
  return true;
}

}// namespace solver
