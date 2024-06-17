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

template<cdcl_sat_strategy Strategy> bool cdcl_sat_conflict_analysis_algo<Strategy>::analyze_conflict()
{
  do {
    resolve(get_latest_implied_var());
  } while (!empty() && size() != 1 && !is_unit());
  log_info(solver, "conflict clause={}", *this);
  return !empty();
}

template<cdcl_sat_strategy Strategy> std::string cdcl_sat_conflict_analysis_algo<Strategy>::to_string() const
{
  std::string ret = "{";
  const char *sep = "";
  for (auto [var, is_positive] : conflict_literals) {
    ret += sep;
    const level_t level = solver.get_var_decision_level(var);
    ret += is_positive ? std::to_string(var) : '-' + std::to_string(var);
    ret += '@' + std::to_string(level);
    sep = ", ";
  }
  ret += '}';
  return ret;
}

template<cdcl_sat_strategy Strategy>
cdcl_sat_conflict_analysis_algo<Strategy>::cdcl_sat_conflict_analysis_algo(const cdcl_sat &solver_in,
  clause_handle conflicting_clause)
  : solver(solver_in)
{
  solver.log_clause(conflicting_clause, "initiating conflict analysis with conflicting_clause");
  for (literal_index_t literal_num = 0; literal_num != solver.get_clause_size(conflicting_clause); ++literal_num) {
    const variable_handle var = solver.get_literal_variable(conflicting_clause, literal_num);
    const variable_handle implication_depth = solver.get_var_implication_depth(var);
    if (implication_depth == 0) { continue; }
    [[maybe_unused]] const auto [literal_iter, was_inserted] =
      conflict_literals.emplace(var, solver.get_literal_value(conflicting_clause, literal_num));
    assert(was_inserted);// NOLINT
    implication_depth_to_var.emplace(implication_depth, var);
  }
  log_info(solver, "cl={}", *this);
}

template<cdcl_sat_strategy Strategy> void cdcl_sat_conflict_analysis_algo<Strategy>::resolve(variable_handle pivot_var)
{
  const clause_handle prev_clause = solver.get_var_implication_clause(pivot_var);
  solver.log_clause(prev_clause, "Resolving with");
  for (literal_index_t literal_num = 0; literal_num != solver.get_clause_size(prev_clause); ++literal_num) {
    const bool is_positive = solver.get_literal_value(prev_clause, literal_num);
    const variable_handle var = solver.get_literal_variable(prev_clause, literal_num);
    const variable_handle implication_depth = solver.get_var_implication_depth(var);
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
