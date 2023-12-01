#ifndef CDCL_SAT_CONFLICT_ANALYSIS_IMPL_HPP
#define CDCL_SAT_CONFLICT_ANALYSIS_IMPL_HPP

#include "solver/cdcl_sat_conflict_analysis.hpp"
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

}// namespace solver

template<solver::cdcl_sat_strategy Strategy> struct fmt::formatter<solver::cdcl_sat_conflict_analysis_algo<Strategy>> : fmt::formatter<std::string> {
  template<typename FormatContext>
  auto format(const solver::cdcl_sat_conflict_analysis_algo<Strategy> &algo, FormatContext &ctx)
  {
    return formatter<std::string>::format(algo.to_string(), ctx);
  }
};


#endif
