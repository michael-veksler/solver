#include "fuzz_utils.hpp"
#include <cstdint>
#include <solver/trivial_sat.hpp>

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <functional>
#include <catch2/matchers/catch_matchers_all.hpp>

using namespace solver::fuzzing;

template <typename T>
class IsBetweenMatcher : public Catch::Matchers::MatcherBase<T> {
    T m_begin, m_end;
public:
    IsBetweenMatcher(T begin, T end) : m_begin(begin), m_end(end) {}

    bool match(T const& value) const override {
        return value >= m_begin && value <= m_end;
    }

    [[nodiscard]] std::string describe() const override {
        return "is between " + std::to_string(m_begin) + " and " + std::to_string(m_end);
    }
};

template <typename T>
IsBetweenMatcher<T> IsBetween(T begin, T end) {
    return { begin, end };
}


TEST_CASE("empty random_stream", "[fuzz_utils]") // NOLINT(cert-err58-cpp)
{
  random_stream empty(nullptr, 0);
  REQUIRE(!empty.get<uint8_t>().has_value());
  REQUIRE(!empty.get<bool>().has_value());
}

#ifdef __GNUC__
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define PACK( DECLARATION ) DECLARATION __attribute__((__packed__))
#endif

#ifdef _MSC_VER
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define PACK( DECLARATION ) __pragma( pack(push, 1) ) DECLARATION __pragma( pack(pop))
#endif

TEST_CASE("struct stream", "[fuzz_utils]") // NOLINT(*cognitive-complexity)
{
  PACK(
    struct test_struct
    {
      uint8_t a;
      uint32_t b;
      uint8_t c;
      uint8_t d;
    }
  );
  test_struct test_data{1, 0x12345678U, 15, 0}; // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
  random_stream data(reinterpret_cast<const uint8_t*>(&test_data), sizeof(test_data)); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
  const auto got_a = data.get<uint8_t>();
  REQUIRE(got_a.has_value());
  REQUIRE(got_a.value_or(0) == test_data.a);

  const auto got_b = data.get<uint32_t>();
  REQUIRE(got_b.has_value());
  REQUIRE(got_b.value_or(0) == test_data.b);

  const auto bad_c = data.get<uint32_t>();
  REQUIRE(!bad_c.has_value());

  const auto got_c = data.get<bool>();
  REQUIRE(got_c.has_value());
  REQUIRE(got_c.value_or(!test_data.c) == bool(test_data.c));

  const auto got_d = data.get<bool>();
  REQUIRE(got_d.has_value());
  REQUIRE(got_d.value_or(!test_data.d) == bool(test_data.d));

  const auto bad_end = data.get<uint8_t>();
  REQUIRE(!bad_end.has_value());
}

TEST_CASE("generate_literal bool", "[fuzz_utils]") // NOLINT(*cognitive-complexity)
{
  csp_generator<bool> generator;

  static constexpr unsigned num_vars = 5;
  std::vector<uint8_t> zero_data;
  static constexpr size_t max_size = 128;
  for (; zero_data.size() < max_size; zero_data.push_back(0)) {
    random_stream zero_stream(zero_data.data(), zero_data.size());
    const auto literal = generator.generate_literal(zero_stream, num_vars);
    if (literal.has_value()) {
      REQUIRE_FALSE(zero_data.empty());
      REQUIRE(literal->variable == 0);
      REQUIRE_FALSE(literal->value);
      break;
    }
  }
  // create random_stream with 128 bytes of data with all bits set
  const std::vector<uint8_t> all_ones_data(128, 0xFF);
  random_stream all_ones_stream(all_ones_data.data(), all_ones_data.size());
  const auto all_ones_literal = generator.generate_literal(all_ones_stream, num_vars);
  REQUIRE(all_ones_literal.has_value());
  // Require that all_ones_literal->variable is one of values in vars. Use REQUIRE_THAT and matchers.
  REQUIRE(all_ones_literal->variable < num_vars);
  REQUIRE(all_ones_literal->value);
}


TEST_CASE("generate_literals all zero bool", "[fuzz_utils]") // NOLINT(*cognitive-complexity)
{
  csp_generator<bool> generator;

  static constexpr unsigned num_vars = 5;
  std::vector<uint8_t> zero_data;
  static constexpr size_t max_size = 128;
  size_t last_size = 0;
  for (; zero_data.size() < max_size; zero_data.push_back(0)) {
    random_stream zero_stream(zero_data.data(), zero_data.size());
    const auto literals = generator.generate_literals(zero_stream, num_vars);
    REQUIRE_THAT(literals.size(), IsBetween(last_size, last_size+1));
    last_size = literals.size();

    for (const auto& literal : literals) {
      REQUIRE(literal.variable < num_vars);
      REQUIRE_FALSE(literal.value);
    }
  }
  REQUIRE(max_size >= 5);
}

// Write test to check that generate_literals returns literals with values set to 1.
TEST_CASE("generate_literals all ones bool", "[fuzz_utils]") // NOLINT(*cognitive-complexity)
{
  csp_generator<bool> generator;

  static constexpr unsigned num_vars = 5;
  std::vector<uint8_t> all_ones_data;
  static constexpr size_t max_size = 128;
  size_t last_size = 0;
  for (; all_ones_data.size() < max_size; all_ones_data.push_back(std::numeric_limits<uint8_t>::max())) {
    random_stream zero_stream(all_ones_data.data(), all_ones_data.size());
    const auto literals = generator.generate_literals(zero_stream, num_vars);
    REQUIRE_THAT(literals.size(), IsBetween(last_size, last_size+1));
    last_size = literals.size();

    for (const auto& literal : literals) {
      REQUIRE(literal.variable < num_vars);
      REQUIRE(literal.value);
    }
  }
  REQUIRE(max_size >= 5);
}

TEST_CASE("generate_literal uint16_t", "[fuzz_utils]") // NOLINT(*cognitive-complexity)
{
  csp_generator<uint16_t> generator;

  static constexpr unsigned num_vars = 5;
  std::vector<uint8_t> zero_data;
  static constexpr size_t max_size = 128;
  for (; zero_data.size() < max_size; zero_data.push_back(0)) {
    random_stream zero_stream(zero_data.data(), zero_data.size());
    const auto literal = generator.generate_literal(zero_stream, num_vars);
    if (literal.has_value()) {
      REQUIRE_FALSE(zero_data.empty());
      REQUIRE(literal->variable < num_vars);
      REQUIRE(literal->value == 0);
      break;
    }
  }
  // create random_stream with 128 bytes of data with all bits set
  const std::vector<uint8_t> all_ones_data(128, 0xFF);
  random_stream all_ones_stream(all_ones_data.data(), all_ones_data.size());
  const auto all_ones_literal = generator.generate_literal(all_ones_stream, num_vars);
  REQUIRE(all_ones_literal.has_value());
  // Require that all_ones_literal->variable is one of values in vars. Use REQUIRE_THAT and matchers.
  REQUIRE(all_ones_literal->variable < num_vars);
  REQUIRE(all_ones_literal->value == std::numeric_limits<uint16_t>::max());
}

TEST_CASE("generate_literals all zero uint16_t", "[fuzz_utils]") // NOLINT(*cognitive-complexity)
{
  csp_generator<uint16_t> generator;

  static constexpr unsigned num_vars = 5;
  std::vector<uint8_t> zero_data;
  static constexpr size_t max_size = 128;
  size_t last_size = 0;
  for (; zero_data.size() < max_size; zero_data.push_back(0)) {
    random_stream zero_stream(zero_data.data(), zero_data.size());
    const auto literals = generator.generate_literals(zero_stream, num_vars);
    REQUIRE_THAT(literals.size(), IsBetween(last_size, last_size+1));
    last_size = literals.size();

    for (const auto& literal : literals) {
      REQUIRE(literal.variable < num_vars);
      REQUIRE(literal.value == 0);
    }
  }
  REQUIRE(max_size >= 5);
}

TEST_CASE("generate_literals all ones uint16_t", "[fuzz_utils]") // NOLINT(*cognitive-complexity)
{
  csp_generator<uint16_t> generator;

  static constexpr unsigned num_vars = 5;
  std::vector<uint8_t> all_ones_data;
  static constexpr size_t max_size = 128;
  size_t last_size = 0;
  for (; all_ones_data.size() < max_size; all_ones_data.push_back(std::numeric_limits<uint8_t>::max())) {
    random_stream zero_stream(all_ones_data.data(), all_ones_data.size());
    const auto literals = generator.generate_literals(zero_stream, num_vars);
    REQUIRE_THAT(literals.size(), IsBetween(last_size, last_size+1));
    last_size = literals.size();

    for (const auto& literal : literals) {
      REQUIRE(literal.variable < num_vars);
      REQUIRE(literal.value == std::numeric_limits<uint16_t>::max());
    }
  }
  REQUIRE(max_size >= 5);
}