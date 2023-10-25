#include <array>
#include <cassert>
#include <cstdio>
#include <fmt/format.h>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <solver/dimacs_parser.hpp>
#include "spdlog/spdlog.h"

#define ENABLE_LOG_PARSER
#ifdef ENABLE_LOG_PARSER
#define LOG_PARSER_INFO spdlog::info
#else
#define LOG_PARSER_INFO(...)
#endif

namespace solver {



void dimacs_parser::parse_header(const std::function<void(unsigned, unsigned)> &construct_problem)
{
    const std::optional<std::string_view> line = get_nonempty_line();
    if (!line) {
        spdlog::error("Invalid dimacs input format - all lines are either empty or commented out");
        throw std::runtime_error("Invalid dimacs input format - all lines are either empty or commented out");
    }

    std::istringstream line_stream{ std::string(*line) };

    std::string cmd;
    std::string format;
    line_stream >> cmd >> format;
    if (!line_stream || cmd != "p" || format != "cnf") {
        spdlog::error("{}: Invalid dimacs input format, expecting a line prefix 'p cnf ' but got '{}'", m_current_line_num, *line);
        throw std::runtime_error("Invalid DIMACS header");
    }

    int variables = 0;
    int clauses = 0;
    line_stream >> variables >> clauses;
    if (!line_stream || variables < 0 || clauses < 0) {
        spdlog::error("{}: Invalid dimacs input format, expecting a header 'p cnf <variables: unsigned int> "
                        "<clauses: unsigned int>' but got '{}'", m_current_line_num, *line);
        throw std::runtime_error("Invalid DIMACS header");
    }

    std::string tail;
    line_stream >> tail;
    if (!tail.empty()) {
        throw std::runtime_error(
            fmt::format("{}: Invalid dimacs input format, junk after header '{}'", m_current_line_num, tail));
    }
    construct_problem(static_cast<unsigned>(variables), static_cast<unsigned>(clauses));
}

void dimacs_parser::parse(const std::function<void(unsigned, unsigned)> & construct_problem,
                          const std::function<void(const std::vector<int> &)> & register_clause)
{
    parse_header(construct_problem);
    while (std::optional<std::string_view> line = get_nonempty_line()) {
        std::istringstream str{ std::string(*line) };
        std::vector<int> literals;
        bool found_zero = false;
        while (true) {
            int literal = 0;
            if (!(str >> literal)) {
                if (found_zero) {
                    break;
                }
                spdlog::error("{}: Missing 0 at the end of the line for line '{}'", m_current_line_num, *line);
                throw std::runtime_error("Missing 0 at the end of the line");
            }
            if (found_zero) {
                spdlog::error("{}: 0 should be only at the end for the line '{}'", m_current_line_num, *line);
                throw std::runtime_error("More than one 0 per-line");
            }
            if (literal == 0) {
                found_zero = true;
                continue;
            }
            literals.push_back(literal);
        }
        register_clause(literals);
    }
}
}// namespace solver