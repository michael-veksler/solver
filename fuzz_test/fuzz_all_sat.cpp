#include "fuzz_utils.hpp"
#include "solver/sat_types.hpp"
#include <algorithm>
#include <bits/ranges_util.h>
#include <cstdint>
#include <numeric>
#include <optional>
#include <solver/trivial_sat.hpp>
#include <solver/cdcl_sat.hpp>
#include <span>
#include <stdexcept>

using namespace solver;
using solver::fuzzing::random_stream;
using solver::fuzzing::csp_generator;

static void add_clause(auto &solver, const auto & variables, const std::vector<literal_type<bool>> & literals, bool test_out_of_range)
{
  auto &clause = solver.add_clause();
  for (const auto literal : literals) {
    if (literal.variable < variables.size()) {
      clause.add_literal(variables[literal.variable], literal.value);
    } else {
      if (!test_out_of_range) { throw std::out_of_range("Variable index out of range"); }
      uint32_t var = literal.variable;
      // If we add a variable, which is accidentally valid, our database of literals will be out of sync with the solver.
      // The solver may return SAT but, according to our database, we may think it should be unsat.
      // That's why we make sure the variable is invalid, so that it crashes earlier.
      while (const bool is_valid_var = std::ranges::find(variables, var) != variables.end()) {
        ++var;
      }
      clause.add_literal(var, literal.value);
    }
  }
}

static void validate_solution(const auto &solver, const auto & variables, const auto &clauses)
{
  const bool pass = std::ranges::all_of(clauses, [&solver, &variables](auto &clause) {
    return std::ranges::any_of(
      clause, [&solver,&variables](literal_type<bool> literal) { return solver.get_variable_value(variables[literal.variable]) == literal.value; });
  });
  if (!pass) { abort(); }
}

template <class Solver, class Variable>
solve_status solve_and_validate(Solver &solver, const std::vector<Variable> &variables, const std::vector<std::vector<literal_type<bool>>> &clauses, bool test_out_of_range = false)
{
  try {
    const solve_status stat = solver.solve();
    if (stat == solve_status::SAT) {
      validate_solution(solver, variables, clauses);
    }
    return stat;
  } catch (const std::out_of_range &except) {
    if (test_out_of_range) {
      return solve_status::UNKNOWN;
    }
    throw;
  }
}

// Fuzzer that attempts to invoke undefined behavior for signed integer overflow
// cppcheck-suppress unusedFunction symbolName=LLVMFuzzerTestOneInput
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size)
{
  random_stream random_data(Data, Size);

  static constexpr unsigned MAX_VARS = 10;
  static constexpr unsigned VAR_RATIO = 16;

  const auto num_vars = static_cast<uint16_t>(
    std::min<size_t>(random_data.get<uint16_t>().value_or(1), random_data.data_span.size() / VAR_RATIO) % (MAX_VARS) + 1);
  const bool test_out_of_range = random_data.get<uint8_t>().value_or(0) % 2 == 0;
  trivial_sat trivial_solver;
  cdcl_sat<domain_strategy<binary_domain>> cdcl_solver;
  const auto trivial_variables = create_variables(trivial_solver, num_vars);
  const auto cdcl_variables = create_variables(cdcl_solver, num_vars);
  std::vector<std::vector<literal_type<bool>>> clauses;
  csp_generator<bool> generator({false, true}, test_out_of_range);
  while (true) {
    const std::vector<literal_type<bool>> &literals = clauses.emplace_back(generator.generate_literals(random_data, num_vars));
    if (literals.empty()) {
      clauses.pop_back();
      break;
    }
    add_clause(trivial_solver, trivial_variables, literals, test_out_of_range);
    add_clause(cdcl_solver, cdcl_variables, literals, test_out_of_range);
  }

  const solve_status trivial_stat = solve_and_validate(trivial_solver, trivial_variables, clauses, test_out_of_range);
  const solve_status cdcl_stat = solve_and_validate(trivial_solver, cdcl_variables, clauses, test_out_of_range);
  if (cdcl_stat != trivial_stat) { abort(); }
  return 0;
}
