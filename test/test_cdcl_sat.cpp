#include "solver/binary_domain.hpp"
#include "solver/discrete_domain.hpp"
#include <solver/cdcl_sat.hpp>
#include <test_utils.hpp>

#include "fmt/core.h"
#include <algorithm>
#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <functional>
#include <sstream>

using namespace solver;
using namespace solver::testing;
using Catch::Matchers::ContainsSubstring;


// NOLINTNEXTLINE(cert-err58-cpp)
TEMPLATE_TEST_CASE("Initially set problem", "[cdcl_sat]", binary_domain, discrete_domain<uint8_t>)
{
  using solver_type = cdcl_sat<domain_strategy<TestType>>;
  solver_type sat;
  const typename solver_type::variable_handle var = sat.add_var(TestType(true));
  REQUIRE(sat.solve() == solve_status::SAT);
  REQUIRE(sat.get_variable_value(var));
}

// NOLINTNEXTLINE(cert-err58-cpp)
TEMPLATE_TEST_CASE("cdcl tiny problem false", "[cdcl_sat]", binary_domain, discrete_domain<uint8_t>)
{
  using solver_type = cdcl_sat<domain_strategy<TestType>>;
  solver_type sat;
  const typename solver_type::variable_handle var = sat.add_var();
  sat.add_clause().add_literal(var, false);
  REQUIRE(sat.solve() == solve_status::SAT);
  REQUIRE(!sat.get_variable_value(var));
}

// NOLINTNEXTLINE(cert-err58-cpp)
TEMPLATE_TEST_CASE("cdcl tiny problem true", "[cdcl_sat]", binary_domain, discrete_domain<uint8_t>)
{
  using solver_type = cdcl_sat<domain_strategy<TestType>>;
  solver_type sat;
  const typename solver_type::variable_handle var = sat.add_var();
  sat.add_clause().add_literal(var, true);
  REQUIRE(sat.solve() == solve_status::SAT);
  REQUIRE(sat.get_variable_value(var));
}

// NOLINTNEXTLINE(cert-err58-cpp)
TEMPLATE_TEST_CASE("cdcl tiny problem unsat", "[cdcl_sat]", binary_domain, discrete_domain<uint8_t>)
{
  using solver_type = cdcl_sat<domain_strategy<TestType>>;
  solver_type sat;
  const typename solver_type::variable_handle var = sat.add_var();
  sat.add_clause().add_literal(var, false);
  sat.add_clause().add_literal(var, true);
  REQUIRE(sat.solve() == solve_status::UNSAT);
}

// NOLINTNEXTLINE(cert-err58-cpp)
TEMPLATE_TEST_CASE("cdcl implication problem", "[cdcl_sat]", binary_domain, discrete_domain<uint8_t>)
{
  using solver_type = cdcl_sat<domain_strategy<TestType>>;
  solver_type sat;
  constexpr unsigned NUM_VARS = 3;
  const std::vector<typename solver_type::variable_handle> vars = create_variables(sat, NUM_VARS);
  typename solver_type::clause &implies0_1 = sat.add_clause();
  implies0_1.add_literal(vars[0], false);
  implies0_1.add_literal(vars[1], true);
  typename solver_type::clause &implies1_2 = sat.add_clause();
  implies1_2.add_literal(vars[1], false);
  implies1_2.add_literal(vars[2], true);

  sat.add_clause().add_literal(vars[0], true);
  REQUIRE(sat.solve() == solve_status::SAT);
  REQUIRE((sat.get_variable_value(vars[0]) && sat.get_variable_value(vars[1]) && sat.get_variable_value(vars[2])));
}

namespace {
template<typename DomainType> struct one_hot_int
{
  std::vector<typename cdcl_sat<domain_strategy<DomainType>>::variable_handle> vars;
};

/**
 * @brief A container and constructor for all_different problem
 *
 * 1. Integers are emulated using the one-hot representation, i.e., a vector X of N binary-variables,
 *    where X[i]==true indicates that we represent the integer \p i. Note that exactly one bit of this X vector
 *    is true.
 * 2. Each such one-hot integer has constraints to make it a valid integer:
 *    - There is at least one bit set to true.
 *    - There is at most one bit set to true.
 * 3. We have N integers represented using this one-hot representation.
 * 4. We add constraints to make sure that every integer is different from all others.
 *
 * Note: It is known that CDCL solvers, that use only clause-resolution rule in its conflict analysis,
 *       require exponential time to reach UNSAT if there are more integers than legal values.
 */
template<typename DomainType> struct all_different_problem
{
  using variable_handle = typename cdcl_sat<domain_strategy<DomainType>>::variable_handle;

  all_different_problem(unsigned num_ints, unsigned num_vals)
  {
    integer_values.reserve(num_ints);
    std::generate_n(std::back_inserter(integer_values), num_ints, [num_vals, this] { return make_one_hot(num_vals); });
    for (const one_hot_int<DomainType> &value : integer_values) {
      constrain_at_least_one(value);
      constrain_at_most_one(value);
    }
    constrain_all_different();
  }
  one_hot_int<DomainType> make_one_hot(unsigned num_vals) { return { .vars = create_variables(solver, num_vals) }; }
  void constrain_at_least_one(const one_hot_int<DomainType> &integer_value)
  {
    typename cdcl_sat<domain_strategy<DomainType>>::clause &at_least_one = solver.add_clause();
    for (const auto var : integer_value.vars) { at_least_one.add_literal(var, true); }
  }
  void constrain_at_most_one(const one_hot_int<DomainType> &integer_value)
  {
    for (unsigned i = 0; i != integer_value.vars.size(); ++i) {
      for (unsigned j = i + 1; j != integer_value.vars.size(); ++j) {
        add_any_false({ integer_value.vars[i], integer_value.vars[j] });
      }
    }
  }

  void add_any_false(std::pair<variable_handle, variable_handle> vars)
  {
    typename cdcl_sat<domain_strategy<DomainType>>::clause &any_false = solver.add_clause();
    any_false.add_literal(vars.first, false);
    any_false.add_literal(vars.second, false);
  }
  void constrain_all_different()
  {
    for (unsigned bit = 0; bit != integer_values[0].vars.size(); ++bit) {
      for (unsigned i = 0; i != integer_values.size(); ++i) {
        for (unsigned j = i + 1; j != integer_values.size(); ++j) {
          add_any_false({ integer_values[i].vars[bit], integer_values[j].vars[bit] });
        }
      }
    }
  }

  std::vector<one_hot_int<DomainType>> integer_values;
  cdcl_sat<domain_strategy<DomainType>> solver;
};
}// namespace

// NOLINTNEXTLINE(cert-err58-cpp)
TEMPLATE_TEST_CASE("pigeon hole problem", "[cdcl_sat]", binary_domain, discrete_domain<uint8_t>)
{
  constexpr unsigned NUM_INTS = 6;
  all_different_problem<TestType> problem(NUM_INTS, NUM_INTS - 1);

  REQUIRE(problem.solver.solve() == solve_status::UNSAT);
}

// NOLINTNEXTLINE(cert-err58-cpp,readability-function-cognitive-complexity)
TEMPLATE_TEST_CASE("all_diff problem", "[cdcl_sat]", binary_domain, discrete_domain<uint8_t>)
{
  constexpr unsigned NUM_INTS = 6;
  all_different_problem<TestType> problem(NUM_INTS, NUM_INTS);

  REQUIRE(problem.solver.solve() == solve_status::SAT);
  std::vector<bool> found_bit(problem.integer_values[0].vars.size(), false);
  for (one_hot_int<TestType> &integer_value : problem.integer_values) {
    bool found_bit_in_value = false;
    for (unsigned i = 0; i != integer_value.vars.size(); ++i) {
      const bool bit_value = problem.solver.get_variable_value(integer_value.vars[i]);
      REQUIRE_FALSE((found_bit[i] && bit_value));
      found_bit[i] = bit_value || found_bit[i];

      REQUIRE_FALSE((found_bit_in_value && bit_value));
      found_bit_in_value = found_bit_in_value || bit_value;
    }
    REQUIRE(found_bit_in_value);
  }
}


namespace {

template<typename DomainType> struct all_literal_combinations
{
  static constexpr unsigned NUM_VARS = 10;
  using solver_type = cdcl_sat<domain_strategy<DomainType>>;

  explicit all_literal_combinations(unsigned max_attempts)
    : solver(max_attempts), vars(create_variables(solver, NUM_VARS))
  {
    for (uint32_t literal_bits = 0; (literal_bits >> NUM_VARS) == 0; literal_bits++) { add_all_literals(literal_bits); }
  }

  void add_all_literals(uint32_t literal_bits)
  {
    typename solver_type::clause &clause = solver.add_clause();
    for (unsigned literal_index = 0; literal_index != NUM_VARS; ++literal_index) {
      const bool literal = ((literal_bits >> literal_index) & 1U) == 1;
      clause.add_literal(vars[literal_index], literal);
    }
  }

  cdcl_sat<domain_strategy<DomainType>> solver;
  std::vector<typename solver_type::variable_handle> vars;
};
}// namespace

TEMPLATE_TEST_CASE("max attempts", "[cdcl_sat]", binary_domain, discrete_domain<uint8_t>)// NOLINT(cert-err58-cpp)
{
  const unsigned backtracks_required = (1U << (all_literal_combinations<TestType>::NUM_VARS - 1)) - 1;
  SECTION("unsat")
  {
    all_literal_combinations<TestType> expected_unsat(backtracks_required);
    REQUIRE(expected_unsat.solver.solve() == solve_status::UNSAT);
  }
  SECTION("unknown")
  {
    all_literal_combinations<TestType> expected_unknown(backtracks_required - 1);
    REQUIRE(expected_unknown.solver.solve() == solve_status::UNKNOWN);
  }
}

// NOLINTNEXTLINE(cert-err58-cpp)
TEST_CASE("log trivial fail", "[cdcl_sat]")
{
  cdcl_sat<domain_strategy<binary_domain>> solver;
  solver.set_debug(true);
  const auto zero_var = solver.add_var(binary_domain{ false });
  solver.add_clause().add_literal(zero_var, true);

  REQUIRE_THAT(log_capture([&solver] { solver.solve(); }).str(), ContainsSubstring("Trivially UNSAT clause 0"));
}

TEST_CASE("solve_status::to_string", "[cdcl_sat]")// NOLINT(cert-err58-cpp)
{
  REQUIRE(to_string(solve_status::SAT) == "SAT");
  REQUIRE(to_string(solve_status::UNSAT) == "UNSAT");
  REQUIRE(to_string(solve_status::UNKNOWN) == "UNKNOWN");
  REQUIRE(to_string(static_cast<solve_status>(5)) == "invalid(5)");
}

TEST_CASE("out_of_range literal", "[cdcl_sat]")
{
  cdcl_sat<domain_strategy<binary_domain>> solver;
  using variable_handle = typename decltype(solver)::variable_handle;
  auto max_signed = []{
    if constexpr (std::is_same_v<variable_handle, uint32_t>) {
      return static_cast<uint32_t>(std::numeric_limits<int32_t>::max());
    } else if constexpr (std::is_same_v<variable_handle, uint16_t>) {
      return static_cast<uint32_t>(std::numeric_limits<uint16_t>::max());
    }
  }();
  REQUIRE_THROWS_AS(solver.add_clause().add_literal(max_signed+1, true), std::out_of_range);
  REQUIRE_THROWS_AS(solver.add_clause().add_literal(static_cast<variable_handle>(-1LL), true), std::out_of_range);

}
