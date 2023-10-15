#include <solver/trivial_sat.hpp>

#include "solver/binary_domain.hpp"
#include <solver/state_saver.hpp>

#include <algorithm>
#include <ranges>
#include <tuple>

namespace solver {
solve_status trivial_sat::solve()
{
  const uint64_t max_attempts = 0;
  auto [status, num_attempts] = solve_recursive(std::next(m_domains.begin()), max_attempts);
  std::ignore = num_attempts;
  return status;
}

std::pair<solve_status, uint64_t> trivial_sat::solve_recursive(std::vector<binary_domain>::iterator depth,// NOLINT
  uint64_t num_attempts) const
{
  if (has_conflict()) {
    if (num_attempts >= m_max_attempts) {
      return { solve_status::UNKNOWN, num_attempts + 1 };
    } else {
      return { solve_status::UNSAT, num_attempts + 1 };
    }
  }
  for (; depth != m_domains.end(); ++depth) {
    if (depth->is_universal()) {
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
    assert(variable < m_domains.size());
    if (m_domains[variable].contains(is_positive)) { return false; }
  }
  return true;
}

}// namespace solver
