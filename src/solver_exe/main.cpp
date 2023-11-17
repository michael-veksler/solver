#include "solver/binary_domain.hpp"
#include "solver/cdcl_sat.hpp"
#include "solver/sat_types.hpp"
#include "solver/trivial_sat.hpp"
#include <CLI/CLI.hpp>
#include <array>
#include <fmt/core.h>
#include <fmt/format.h>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <optional>
#include <ranges>
#include <solver/dimacs_parser.hpp>
#include <spdlog/spdlog.h>

// This file will be generated automatically when cur_you run the CMake
// configuration step. It creates a namespace called `solver`. You can modify
// the source template at `configured_files/config.hpp.in`.
#include <internal_use_only/config.hpp>

namespace {
enum class solver_kind { trivial_sat = 0, cdcl_sat = 1 };
}

namespace {
template<typename solver_type> class solver_main
{
public:
  template<typename... Args> explicit solver_main(Args &&...args) : m_solver(std::forward<Args>(args)...) {}

  void set_debug(bool is_debug) { m_solver.set_debug(is_debug); }

  void solve(const std::string &filename)
  {
    parse(filename);
    const solver::solve_status status = m_solver.solve();
    if (status == solver::solve_status::SAT) {
      fmt::print("SAT");
      // When we upgrade to clang-16, use std::views::drop(1) instead.
      for (unsigned i = 1; i != m_variables.size(); ++i) {
        const auto var = m_variables[i];
        fmt::print(" v{}={}", var, static_cast<int>(m_solver.get_current_domain(var).contains(true)));
      }
      fmt::print("\n");
    } else {
      fmt::println("{}", to_string(status));
    }
  }

private:
  /**
   * @brief Get a lambda that gets literals and constructs clauses.
   *
   * @return A callable that receives literals and adds a clause with those literals to m_solver.
   */
  auto get_clause_constructor()
  {
    return [this](const std::vector<int> &literals) {
      auto &clause = m_solver.add_clause();
      for (const int literal : literals) {
        const bool is_positive = literal > 0;
        const auto var_index = static_cast<unsigned>(is_positive ? literal : -literal);
        const unsigned var = m_variables.at(var_index);
        clause.add_literal(var, is_positive);
      }
    };
  }

  /**
   * @brief Get a lambda that gets num of variables and clauses and initializes m_solver.
   *
   * @return A callable that initializes the solver with variables.
   */
  auto get_problem_constructor()
  {
    return [this](unsigned count_vars, unsigned count_clauses) {
      m_solver.reserve_vars(count_vars);
      m_variables.reserve(count_vars + 1);
      m_variables.push_back(0);

      std::generate_n(std::back_inserter(m_variables), count_vars, [this] { return m_solver.add_var(); });
      m_solver.reserve_clauses(count_clauses);
    };
  }

  void parse(const std::string &filename)
  {
    std::ifstream input_file(filename);
    auto getline = [&input_file](std::string &line) {
      std::getline(input_file, line);
      return bool(input_file);
    };

    solver::dimacs_parser file_parser(getline);

    file_parser.parse(get_problem_constructor(), get_clause_constructor());
  }

  std::vector<typename solver_type::variable_handle> m_variables;
  solver_type m_solver;
};
}// namespace

// NOLINTNEXTLINE(bugprone-exception-escape)
int main(int argc, const char **argv)
{
  try {
    CLI::App app{ fmt::format("{} version {}", solver::cmake::project_name, solver::cmake::project_version) };

    solver_kind requested_solver = solver_kind::cdcl_sat;
    const std::map<std::string, solver_kind> solver_kind_map = { { "trivial_sat", solver_kind::trivial_sat },
      { "cdcl_sat", solver_kind::cdcl_sat } };
    app.add_option("--solver", requested_solver, "The solver to use")
      ->required()
      ->transform(CLI::CheckedTransformer(solver_kind_map, CLI::ignore_case));
    std::string input;
    app.add_option("--input,input", input, "Input file")->required();
    bool is_debug = false;
    app.add_flag("--debug", is_debug, "Enable debug output");

    CLI11_PARSE(app, argc, argv);
    switch (requested_solver) {
    case solver_kind::cdcl_sat: {
      solver_main<solver::cdcl_sat<solver::domain_strategy<solver::binary_domain>>> solver_tester;
      solver_tester.set_debug(is_debug);
      solver_tester.solve(input);
    } break;
    case solver_kind::trivial_sat:
      solver_main<solver::trivial_sat>().solve(input);
      break;
    }


  } catch (CLI::ParseError &e) {
    fmt::print("=== parse error ==== {}", e.what());
    return 1;
  } catch (const std::exception &e) {
    spdlog::error("Unhandled exception in main: {}", e.what());
  }
}
