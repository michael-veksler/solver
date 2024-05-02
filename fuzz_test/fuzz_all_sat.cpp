#include "fuzz_utils.hpp"
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

static std::optional<literal_type>
  generate_literal(random_stream &random_data, std::vector<unsigned> &available_var_idx, bool test_out_of_range)
{
  const bool invalid_var = test_out_of_range && random_data.get<uint8_t>().value_or(1) == 0;

  const std::optional<unsigned> coded_literal = random_data.get<uint16_t>();
  if (!coded_literal) { return std::nullopt; }
  const unsigned variable_index = invalid_var ? (*coded_literal >> 1U) :
    available_var_idx[(*coded_literal >> 1U) % available_var_idx.size()];
  return literal_type{ .is_positive = (*coded_literal & 1U) == 1, .variable = variable_index };
}

static std::vector<literal_type> generate_literals(random_stream &random_data, size_t num_vars, bool test_out_of_range)
{
  const std::optional<unsigned> num_literals_source = random_data.get<uint16_t>();
  if (!num_literals_source) { return {}; }
  const unsigned num_literals = *num_literals_source % num_vars + 1;
  std::vector<literal_type> literals;
  literals.reserve(num_literals);
  std::vector<unsigned> available_var_idx(num_vars);
  std::iota(available_var_idx.begin(), available_var_idx.end(), 0U);
  for (unsigned i = 0; i != num_literals; ++i) {
    std::optional<literal_type> literal = generate_literal(random_data, available_var_idx, test_out_of_range);
    if (!literal) { break; }
    literals.push_back(*literal);
  }
  return literals;
}


static void add_clause(auto &solver, const auto & variables, const std::vector<literal_type> & literals, bool test_out_of_range)
{
  auto &clause = solver.add_clause();
  for (const auto literal : literals) {
    if (literal.variable < variables.size()) {
      clause.add_literal(variables[literal.variable], literal.is_positive);
    } else {
      if (!test_out_of_range) { throw std::out_of_range("Variable index out of range"); }
      uint32_t var = literal.variable;
      // If we add a variable, which is accidentally valid, our database of literals will be out of sync with the solver.
      // The solver may return SAT but, according to our database, we may think it should be unsat.
      // That's why we make sure the variable is invalid, so that it crashes earlier.
      while (const bool is_valid_var = std::ranges::find(variables, var) != variables.end()) {
        ++var;
      }
      clause.add_literal(var, literal.is_positive);
    }
  }
}

static void validate_solution(const auto &solver, const auto & variables, const auto &clauses)
{
  const bool pass = std::ranges::all_of(clauses, [&solver, &variables](auto &clause) {
    return std::ranges::any_of(
      clause, [&solver,&variables](literal_type literal) { return solver.get_variable_value(variables[literal.variable]) == literal.is_positive; });
  });
  if (!pass) { abort(); }
}

template <class Solver, class Variable>
solve_status solve_and_validate(Solver &solver, const std::vector<Variable> &variables, const std::vector<std::vector<literal_type>> &clauses, bool test_out_of_range = false)
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
  std::vector<std::vector<literal_type>> clauses;
  while (true) {
    const std::vector<literal_type> &literals = clauses.emplace_back(generate_literals(random_data, num_vars, test_out_of_range));
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
