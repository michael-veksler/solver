#include <algorithm>
#include <cstdint>
#include <numeric>
#include <optional>
#include <solver/trivial_sat.hpp>
#include <solver/cdcl_sat.hpp>
#include <span>

using namespace solver;

namespace {

struct random_stream
{
  random_stream(const uint8_t *data, size_t size) : data_span(data, size) {}

  template<typename T>  std::optional<T> get()
  {
    if (data_span.size() < sizeof(T)) { return std::nullopt; }
    const T *ret = static_cast<const T *>(static_cast<const void *>(data_span.data()));
    data_span = data_span.subspan(sizeof(T));
    return *ret;
  }
  std::span<const uint8_t> data_span;

};
}

static std::optional<literal_type>
  generate_literal(random_stream &random_data, std::vector<unsigned> &available_var_idx)
{
  const std::optional<unsigned> coded_literal = random_data.get<uint16_t>();
  if (!coded_literal) { return std::nullopt; }
  const unsigned variable_index = available_var_idx[(*coded_literal >> 1U) % available_var_idx.size()];
  return literal_type{ .is_positive = (*coded_literal & 1U) == 1, .variable = variable_index };
}

static std::vector<literal_type> generate_literals(random_stream &random_data, size_t num_vars)
{
  const std::optional<unsigned> num_literals_source = random_data.get<uint16_t>();
  if (!num_literals_source) { return {}; }
  const unsigned num_literals = *num_literals_source % num_vars + 1;
  std::vector<literal_type> literals;
  literals.reserve(num_literals);
  std::vector<unsigned> available_var_idx(num_vars);
  std::iota(available_var_idx.begin(), available_var_idx.end(), 0U);
  for (unsigned i = 0; i != num_literals; ++i) {
    std::optional<literal_type> literal = generate_literal(random_data, available_var_idx);
    if (!literal) { break; }
    literals.push_back(*literal);
  }
  return literals;
}


static void add_clause(auto &solver, const auto & variables, const std::vector<literal_type> & literals)
{
  auto &clause = solver.add_clause();
  for (const auto literal : literals) { clause.add_literal(variables[literal.variable], literal.is_positive); }
}

static void validate_solution(const auto &solver, const auto & variables, const auto &clauses)
{
  const bool pass = std::ranges::all_of(clauses, [&solver, &variables](auto &clause) {
    return std::ranges::any_of(
      clause, [&solver,&variables](literal_type literal) { return solver.get_variable_value(variables[literal.variable]) == literal.is_positive; });
  });
  if (!pass) { abort(); }
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
  trivial_sat trivial_solver;
  cdcl_sat<domain_strategy<binary_domain>> cdcl_solver;
  const auto trivial_variables = create_variables(trivial_solver, num_vars);
  const auto cdcl_variables = create_variables(cdcl_solver, num_vars);
  std::vector<std::vector<literal_type>> clauses;
  while (true) {
    const std::vector<literal_type> &literals = clauses.emplace_back(generate_literals(random_data, num_vars));
    if (literals.empty()) {
      clauses.pop_back();
      break;
    }
    add_clause(trivial_solver, trivial_variables, literals);
    add_clause(cdcl_solver, cdcl_variables, literals);
  }

  const solve_status trivial_stat = trivial_solver.solve();
  if (trivial_stat == solve_status::SAT) { validate_solution(trivial_solver, trivial_variables, clauses); }
  const solve_status cdcl_stat = cdcl_solver.solve();
  if (cdcl_stat == solve_status::SAT) { validate_solution(cdcl_solver, cdcl_variables, clauses); }
  if (cdcl_stat != trivial_stat) { abort(); }

  return 0;
}
