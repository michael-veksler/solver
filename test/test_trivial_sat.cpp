#include "solver/binary_domain.hpp"
#include <solver/trivial_sat.hpp>

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <functional>

using namespace solver;


TEST_CASE("Trivial tiny problem false", "[trivial_sat]")
{
  trivial_sat sat;
  const unsigned var = sat.add_var();
  sat.add_clause().add_literal(var, false);
  REQUIRE(sat.solve() == solve_status::SAT);
  REQUIRE(!sat.get_value(var));
}

TEST_CASE("Trivial tiny problem true", "[trivial_sat]")
{
  trivial_sat sat;
  const unsigned var = sat.add_var();
  sat.add_clause().add_literal(var, true);
  REQUIRE(sat.solve() == solve_status::SAT);
  REQUIRE(sat.get_value(var));
}

TEST_CASE("Trivial tiny problem unsat", "[trivial_sat]")
{
  trivial_sat sat;
  const unsigned var = sat.add_var();
  sat.add_clause().add_literal(var, false);
  sat.add_clause().add_literal(var, true);
  REQUIRE(sat.solve() == solve_status::UNSAT);
}

TEST_CASE("Trivial implication problem", "[trivial_sat]")
{
  trivial_sat sat;
  const unsigned var1 = sat.add_var();
  const unsigned var2 = sat.add_var();
  const unsigned var3 = sat.add_var();
  trivial_sat::clause &implies1_2 = sat.add_clause();
  implies1_2.add_literal(var1, false);
  implies1_2.add_literal(var2, true);
  trivial_sat::clause &implies2_3 = sat.add_clause();
  implies2_3.add_literal(var2, false);
  implies2_3.add_literal(var3, true);

  sat.add_clause().add_literal(var1, true);
  REQUIRE(sat.solve() == solve_status::SAT);
  REQUIRE((sat.get_value(var1) && sat.get_value(var2) && sat.get_value(var3)));
}

namespace {
struct one_hot_int
{
  std::vector<unsigned> vars;
};
struct all_different_problem
{
  explicit all_different_problem(unsigned num_ints, unsigned num_vals)
  {
    integer_values.reserve(num_ints);
    std::generate_n(std::back_inserter(integer_values), num_ints, [num_vals, this] { return make_one_hot(num_vals); });
    for (one_hot_int &value : integer_values) {
      constrain_at_least_one(value);
      constrain_at_most_one(value);
    }
    constrain_all_different();
  }
  one_hot_int make_one_hot(unsigned num_vals)
  {
    one_hot_int ret;
    ret.vars.reserve(num_vals);
    std::generate_n(std::back_inserter(ret.vars), num_vals, [this] { return solver.add_var(); });
    return ret;
  }
  void constrain_at_least_one(const one_hot_int &integer_value)
  {
    trivial_sat::clause &at_least_one = solver.add_clause();
    for (const unsigned var : integer_value.vars) { at_least_one.add_literal(var, true); }
  }
  void constrain_at_most_one(const one_hot_int &integer_value)
  {
    for (unsigned i = 0; i != integer_value.vars.size(); ++i) {
      for (unsigned j = i + 1; j != integer_value.vars.size(); ++j) {
        add_any_false(integer_value.vars[i], integer_value.vars[j]);
      }
    }
  }

  void add_any_false(unsigned left, unsigned right)
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
  for (one_hot_int &integer_value : problem.integer_values) {
    bool found_bit_in_value = false;
    for (unsigned i = 0; i != integer_value.vars.size(); ++i) {
      const bool bit_value = problem.solver.get_value(integer_value.vars[i]);
      REQUIRE_FALSE((found_bit[i] && bit_value));
      found_bit[i] = bit_value || found_bit[i];

      REQUIRE_FALSE((found_bit_in_value && bit_value));
      found_bit_in_value = found_bit_in_value || bit_value;
    }
    REQUIRE(found_bit_in_value);
  }
}
