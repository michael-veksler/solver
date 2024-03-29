#include <array>
#include <catch2/catch_test_macros.hpp>

#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_exception.hpp>
#include <catch2/matchers/catch_matchers_range_equals.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <sstream>
#include <string>
#include <string_view>

#include "solver/dimacs_parser.hpp"
#include "spdlog/sinks/ostream_sink.h"
#include "spdlog/spdlog.h"
#include "test_utils.hpp"

using namespace solver;
using Catch::Matchers::ContainsSubstring;
using Catch::Matchers::RangeEquals;
using namespace solver::testing;

namespace {


/**
 * @brief Helper class to hold one parsing case.
 *
 * It holds the output of the logger, and the result of the parsing.
 *
 */
struct dimacs_parse_case
{
  void parse(const std::string &text)
  {
    const log_redirect redirect{ log_stream };
    std::istringstream text_stream{ text };
    auto construct_problem = [this](unsigned read_vars, unsigned read_clauses) {
      this->n_variables = read_vars;
      this->n_clauses = read_clauses;
    };
    auto register_clause = [this](const std::vector<int> &read_clauses) { clauses.push_back(read_clauses); };
    auto getline = [&text_stream](std::string &line) {
      std::getline(text_stream, line);
      return bool(text_stream);
    };
    dimacs_parser parser(getline);
    parser.parse(construct_problem, register_clause);
  }
  std::vector<std::vector<int>> clauses;///< The resulting clause
  std::ostringstream log_stream;
  unsigned n_clauses = 0;
  unsigned n_variables = 0;
};
}// namespace

TEST_CASE("dimacs empty input", "[dimacs_parser]")
{
  REQUIRE_THROWS_MATCHES(dimacs_parse_case().parse(""),
    std::runtime_error,
    Catch::Matchers::Message("Invalid dimacs input format - all lines are either empty or commented out"));
}

TEST_CASE("dimacs bad header prefix", "[dimacs_parser]")
{
  dimacs_parse_case tester;
  REQUIRE_THROWS_MATCHES(
    tester.parse("p cn 2 3"), std::runtime_error, Catch::Matchers::Message("Invalid DIMACS header"));
  spdlog::info("actual message=<<<{}>>>", tester.log_stream.str());
  REQUIRE_THAT(tester.log_stream.str(),
    ContainsSubstring("<<<1: Invalid dimacs input format, expecting a line prefix 'p cnf ' but got 'p cn 2 3'>>>"));
}

TEST_CASE("dimacs header prefix numbers", "[dimacs_parser]")
{
  dimacs_parse_case tester;
  REQUIRE_THROWS_MATCHES(tester.parse(R"(c foo
                                           p cnf -3 2)"),
    std::runtime_error,
    Catch::Matchers::Message("Invalid DIMACS header"));
  REQUIRE_THAT(tester.log_stream.str(),
    ContainsSubstring("<<<2: Invalid dimacs input format, expecting a header 'p cnf <variables: unsigned int> "
                      "<clauses: unsigned int>' but got 'p cnf -3 2'>>>"));
}


TEST_CASE("dimacs junk at header end", "[dimacs_parser]")
{
  REQUIRE_THROWS_MATCHES(dimacs_parse_case().parse(R"(p cnf 2 3 4
                                                        1 2 0)"),
    std::runtime_error,
    Catch::Matchers::Message("1: Invalid dimacs input format, junk after header '4'"));
}

TEST_CASE("dimacs n_variables overflow", "[dimacs_parser]")
{
  dimacs_parse_case tester;
  REQUIRE_THROWS_MATCHES(tester.parse(R"(p cnf 2147483648 3
                                            1 2 0)"),
    std::runtime_error,
    Catch::Matchers::Message("Invalid DIMACS header"));
  REQUIRE_THAT(tester.log_stream.str(),
    ContainsSubstring("<<<1: Invalid dimacs input format, expecting a header 'p cnf <variables: unsigned int> "
                      "<clauses: unsigned int>' but got 'p cnf 2147483648 3'>>>"));
}

TEST_CASE("dimacs n_variables almost overflow", "[dimacs_parser]")
{
  dimacs_parse_case tester;
  tester.parse(R"(p cnf 2147483647 3
                    1 2 0)");
  REQUIRE(tester.n_variables == 2147483647U);
}

TEST_CASE("dimacs invalid 0 in clause middle", "[dimacs_parser]")
{
  dimacs_parse_case tester;
  REQUIRE_THROWS_MATCHES(tester.parse(R"(
                                           p cnf 10 20
                                           1 -2 0
                                           2 0 3 0)"),
    std::runtime_error,
    Catch::Matchers::Message("More than one 0 per-line"));
  REQUIRE_THAT(
    tester.log_stream.str(), ContainsSubstring("<<<4: 0 should be only at the end for the line '2 0 3 0'>>>"));
}

TEST_CASE("dimacs missing 0 at clause end", "[dimacs_parser]")
{
  dimacs_parse_case tester;
  REQUIRE_THROWS_MATCHES(tester.parse(R"(p      cnf  10  20
                                           1 -2 3
                                           2 2 3 0)"),
    std::runtime_error,
    Catch::Matchers::Message("Missing 0 at the end of the line"));
  REQUIRE_THAT(
    tester.log_stream.str(), ContainsSubstring("<<<2: Missing 0 at the end of the line for line '1 -2 3'>>>"));
}


TEST_CASE("dimacs parse", "[dimacs_parser]")
{
  dimacs_parse_case tester;
  tester.parse(R"(
        p cnf 4 5
        1 -2 3 0
        2 3 0
        -1 2 -3 4 0
        1 -2 -3 -4 0
    )");
  REQUIRE(tester.n_variables == 4);
  REQUIRE(tester.n_clauses == 5);
  REQUIRE_THAT(tester.clauses,
    RangeEquals(std::vector<std::vector<int>>{ { 1, -2, 3 }, { 2, 3 }, { -1, 2, -3, 4 }, { 1, -2, -3, -4 } }));
}
