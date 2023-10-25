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

using namespace solver;
using Catch::Matchers::EndsWith;
using Catch::Matchers::RangeEquals;

namespace {
/**
 * @brief A RAII object to redirect the default spdlog logger to a stream.
 *
 * The redirection is undone upon destruction of the object.
 *
 */
class log_redirect
{
public:
  explicit log_redirect(std::ostream &out)
  {
    auto ostream_sink = std::make_shared<spdlog::sinks::ostream_sink_st>(out);
    auto ostream_logger = std::make_shared<spdlog::logger>(logger_name, ostream_sink);
    ostream_logger->set_pattern("%v");
    ostream_logger->set_level(spdlog::level::debug);
    spdlog::set_default_logger(ostream_logger);
  }
  log_redirect(const log_redirect &) = delete;
  log_redirect(log_redirect &&) = delete;
  log_redirect &operator=(const log_redirect &) = delete;
  log_redirect &operator=(log_redirect &&) = delete;

  ~log_redirect()
  {
    spdlog::set_default_logger(std::move(original_logger));
    spdlog::drop(logger_name);
  }

private:
  static constexpr const char *logger_name = "test_logger";

  std::shared_ptr<spdlog::logger> original_logger = spdlog::default_logger();
};
}// namespace

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
  std::vector<std::vector<int>> clauses;
  std::ostringstream log_stream;
  unsigned n_clauses = 0;
  unsigned n_variables = 0;
};

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
    EndsWith("1: Invalid dimacs input format, expecting a line prefix 'p cnf ' but got 'p cn 2 3'\n"));
}

TEST_CASE("dimacs header prefix numbers", "[dimacs_parser]")
{
  dimacs_parse_case tester;
  REQUIRE_THROWS_MATCHES(tester.parse(R"(c foo
                                           p cnf -3 2)"),
    std::runtime_error,
    Catch::Matchers::Message("Invalid DIMACS header"));
  REQUIRE_THAT(tester.log_stream.str(),
    EndsWith("2: Invalid dimacs input format, expecting a header 'p cnf <variables: unsigned int> "
             "<clauses: unsigned int>' but got 'p cnf -3 2'\n"));
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
    EndsWith("1: Invalid dimacs input format, expecting a header 'p cnf <variables: unsigned int> "
             "<clauses: unsigned int>' but got 'p cnf 2147483648 3'\n"));
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
  REQUIRE_THAT(tester.log_stream.str(), EndsWith("4: 0 should be only at the end for the line '2 0 3 0'\n"));
}

TEST_CASE("dimacs missing 0 at clause end", "[dimacs_parser]")
{
  dimacs_parse_case tester;
  REQUIRE_THROWS_MATCHES(tester.parse(R"(p      cnf  10  20
                                           1 -2 3
                                           2 2 3 0)"),
    std::runtime_error,
    Catch::Matchers::Message("Missing 0 at the end of the line"));
  REQUIRE_THAT(tester.log_stream.str(), EndsWith("2: Missing 0 at the end of the line for line '1 -2 3'\n"));
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
