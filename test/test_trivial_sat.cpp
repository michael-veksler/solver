#include "solver/binary_domain.hpp"
#include <solver/trivial_sat.hpp>

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <functional>

using namespace solver;


TEST_CASE("Initially set problem", "[trivial_sat]") // NOLINT
{
  trivial_sat sat;
  const trivial_sat::variable_handle var = sat.add_var(binary_domain(true));
  REQUIRE(sat.solve() == solve_status::SAT);
  REQUIRE(sat.get_variable_value(var));
}

TEST_CASE("Trivial tiny problem false", "[trivial_sat]")
{
  trivial_sat sat;
  const trivial_sat::variable_handle var = sat.add_var();
  sat.add_clause().add_literal(var, false);
  REQUIRE(sat.solve() == solve_status::SAT);
  REQUIRE(!sat.get_variable_value(var));
}

TEST_CASE("Trivial tiny problem true", "[trivial_sat]")
{
  trivial_sat sat;
  const trivial_sat::variable_handle var = sat.add_var();
  sat.add_clause().add_literal(var, true);
  REQUIRE(sat.solve() == solve_status::SAT);
  REQUIRE(sat.get_variable_value(var));
}

TEST_CASE("Trivial tiny problem unsat", "[trivial_sat]")
{
  trivial_sat sat;
  const trivial_sat::variable_handle var = sat.add_var();
  sat.add_clause().add_literal(var, false);
  sat.add_clause().add_literal(var, true);
  REQUIRE(sat.solve() == solve_status::UNSAT);
}

TEST_CASE("Trivial implication problem", "[trivial_sat]")
{
  trivial_sat sat;
  constexpr unsigned NUM_VARS = 3;
  const std::vector<trivial_sat::variable_handle> vars = create_variables(sat, NUM_VARS);
  trivial_sat::clause &implies0_1 = sat.add_clause();
  implies0_1.add_literal(vars[0], false);
  implies0_1.add_literal(vars[1], true);
  trivial_sat::clause &implies1_2 = sat.add_clause();
  implies1_2.add_literal(vars[1], false);
  implies1_2.add_literal(vars[2], true);

  sat.add_clause().add_literal(vars[0], true);
  REQUIRE(sat.solve() == solve_status::SAT);
  REQUIRE((sat.get_variable_value(vars[0]) && sat.get_variable_value(vars[1]) && sat.get_variable_value(vars[2])));
}

namespace {
struct one_hot_int
{
  std::vector<trivial_sat::variable_handle> vars;
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
struct all_different_problem
{
  explicit all_different_problem(unsigned num_ints, unsigned num_vals)
  {
    integer_values.reserve(num_ints);
    std::generate_n(std::back_inserter(integer_values), num_ints, [num_vals, this] { return make_one_hot(num_vals); });
    for (const one_hot_int &value : integer_values) {
      constrain_at_least_one(value);
      constrain_at_most_one(value);
    }
    constrain_all_different();
  }
  one_hot_int make_one_hot(unsigned num_vals)
  {
    return {.vars = create_variables(solver, num_vals) };
  }
  void constrain_at_least_one(const one_hot_int &integer_value)
  {
    trivial_sat::clause &at_least_one = solver.add_clause();
    for (const trivial_sat::variable_handle var : integer_value.vars) { at_least_one.add_literal(var, true); }
  }
  void constrain_at_most_one(const one_hot_int &integer_value)
  {
    for (unsigned i = 0; i != integer_value.vars.size(); ++i) {
      for (unsigned j = i + 1; j != integer_value.vars.size(); ++j) {
        add_any_false(integer_value.vars[i], integer_value.vars[j]);
      }
    }
  }

  void add_any_false(trivial_sat::variable_handle left, trivial_sat::variable_handle right)
  {
    trivial_sat::clause &any_false = solver.add_clause();
    any_false.add_literal(left, false);
    any_false.add_literal(right, false);
  }
  void constrain_all_different()
  {
    for (unsigned bit = 0; bit != integer_values[0].vars.size(); ++bit) {
      for (unsigned i = 0; i != integer_values.size(); ++i) {
        for (unsigned j = i + 1; j != integer_values.size(); ++j) {
          add_any_false(integer_values[i].vars[bit], integer_values[j].vars[bit]);
        }
      }
    }
  }

  std::vector<one_hot_int> integer_values;
  trivial_sat solver;
};
}// namespace

TEST_CASE("pigeon hole problem", "[trivial_sat]")
{
  constexpr unsigned NUM_INTS = 6;
  all_different_problem problem(NUM_INTS, NUM_INTS - 1);

  REQUIRE(problem.solver.solve() == solve_status::UNSAT);
}

TEST_CASE("all_diff problem", "[trivial_sat]")// NOLINT
{
  constexpr unsigned NUM_INTS = 6;
  all_different_problem problem(NUM_INTS, NUM_INTS);

  REQUIRE(problem.solver.solve() == solve_status::SAT);
  std::vector<bool> found_bit(problem.integer_values[0].vars.size(), false);
  for (const one_hot_int &integer_value : problem.integer_values) {
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
struct all_literal_combinations
{
  static constexpr unsigned NUM_VARS = 4;

  explicit all_literal_combinations(unsigned max_attempts) : solver(max_attempts),
    vars(create_variables(solver, NUM_VARS)) {
    for (uint32_t literal_bits = 0; (literal_bits >> NUM_VARS) == 0; literal_bits++) {
      add_all_literals(literal_bits);
    }

  }

  void add_all_literals(uint32_t literal_bits) {
    trivial_sat::clause & clause = solver.add_clause();
    for (unsigned literal_index=0; literal_index != NUM_VARS; ++literal_index) {
      const bool literal = ((literal_bits >> literal_index) & 1U) == 1;
      clause.add_literal(vars[literal_index],  literal);
    }
  }

  trivial_sat solver;
  std::vector<trivial_sat::variable_handle> vars;
};
}// namespace

TEST_CASE("max attempts", "[trivial_sat]")
{
  all_literal_combinations expected_unsat(1U << all_literal_combinations::NUM_VARS);
  all_literal_combinations expected_unknown((1U << all_literal_combinations::NUM_VARS)-1);
  REQUIRE(expected_unsat.solver.solve() == solve_status::UNSAT);
  REQUIRE(expected_unknown.solver.solve() == solve_status::UNKNOWN);
}

TEST_CASE("Clause ostream operator", "[trivial_sat]") // NOLINT
{
  trivial_sat sat;
  const trivial_sat::variable_handle var1 = sat.add_var();
  const trivial_sat::variable_handle var2 = sat.add_var();
  trivial_sat::clause &clause = sat.add_clause();
  clause.add_literal(var1, true);
  clause.add_literal(var2, false);
  std::ostringstream oss;
  oss << clause;
  REQUIRE(oss.str() == "{1, -2}");
}

TEST_CASE("Clause fmt::format", "[trivial_sat]") // NOLINT
{
  trivial_sat sat;
  const trivial_sat::variable_handle var1 = sat.add_var();
  const trivial_sat::variable_handle var2 = sat.add_var();
  trivial_sat::clause &clause = sat.add_clause();
  clause.reserve(1); // side-effect, check that it works even when the reservation is small.
  clause.add_literal(var1, false);
  clause.add_literal(var2, true);
  REQUIRE(fmt::format("{}", clause) == "{-1, 2}");
}

TEST_CASE("Out of range literal", "[trivial_sat]") // NOLINT
{
  trivial_sat sat;
  const trivial_sat::variable_handle var1 = sat.add_var();
  const trivial_sat::variable_handle var2 = sat.add_var();
  trivial_sat::clause &clause = sat.add_clause();
  clause.add_literal(var1, false);
  clause.add_literal(var2, true);
  REQUIRE_THROWS_AS(clause.get_variable(var2 + 1), std::out_of_range);
  REQUIRE_THROWS_AS(clause.is_positive_literal(var2 + 1), std::out_of_range);
}

// Test that adding an invalid and big variable name throws an exception during sat.solve().
TEST_CASE("Out of range variable", "[trivial_sat]") // NOLINT
{
  trivial_sat sat;
  const trivial_sat::variable_handle var1 = sat.add_var();
  const trivial_sat::variable_handle var2 = sat.add_var();
  trivial_sat::clause &clause = sat.add_clause();
  clause.add_literal(var1, false);
  clause.add_literal(var2, true);
  clause.add_literal(var2 + 1, true);

  REQUIRE_THROWS_AS(sat.solve(), std::out_of_range);
}