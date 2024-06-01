#include "fuzz_utils.hpp"
#include <catch2/internal/catch_test_registry.hpp>
#include <cstdint>
#include <solver/trivial_sat.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_all.hpp>
#include <type_traits>

using namespace solver::fuzzing;
using solver::literal_type;

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

template <typename T, typename LambdaFunc> requires std::is_invocable_r_v<bool, LambdaFunc, T>
// Meed to suppress exception-escape due to some issue in either cppcheck or Catch2.
class LambdaMatcher  : public Catch::Matchers::MatcherBase<T> // cppcheck-suppress (bugprone-exception-escape)
{
public:
  explicit LambdaMatcher(LambdaFunc && lambda) noexcept // cppcheck-suppress (bugprone-exception-escape)
    : m_lambda(std::move(lambda)) {}
  explicit LambdaMatcher(const LambdaFunc & lambda)
    : m_lambda(lambda) {}
  bool match(const T & value) const override
  {
    return m_lambda(value);
  }
  [[nodiscard]] std::string describe() const override {
    return "matches lambda expression";
  }

private:
  LambdaFunc m_lambda;
};

template <typename T, typename LambdaFunc> requires std::is_invocable_r_v<bool, LambdaFunc, T>
LambdaMatcher<T, LambdaFunc> Matches(LambdaFunc && lambda) {
  return { LambdaMatcher<T, std::remove_cvref_t<LambdaFunc>>(std::forward<LambdaFunc>(lambda)) };
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
      REQUIRE(literal.value().variable == 0);
      REQUIRE_FALSE(literal.value().value);
      break;
    }
  }
  // creating random_stream with 128 bytes of data with all bits set
  const std::vector<uint8_t> all_ones_data(128, 0xFF);
  random_stream all_ones_stream(all_ones_data.data(), all_ones_data.size());
  const auto all_ones_literal = generator.generate_literal(all_ones_stream, num_vars);
  REQUIRE(all_ones_literal.has_value());
  REQUIRE(all_ones_literal.value_or(literal_type<bool>{false, num_vars}).variable < num_vars);
  REQUIRE(all_ones_literal.value_or(literal_type<bool>{false, num_vars}).value);
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

TEST_CASE("generate_literals out_of_range variable bool", "[fuzz_utils]") // NOLINT(*cognitive-complexity)
{
  csp_generator<bool> generator({false, true}, true);

  static constexpr unsigned num_vars = 5;
  static constexpr size_t max_size = 16 * 1024ULL;
  std::vector<uint8_t> periodic_data;
  periodic_data.reserve(max_size);
  for (size_t i = 0; i < max_size; ++i) {
    periodic_data.push_back(uint8_t(i));
  }


  random_stream periodic_stream(periodic_data.data(), periodic_data.size());
  std::vector<literal_type<bool>> literals;
  while (const auto literal = generator.generate_literal(periodic_stream, num_vars)) {
    literals.push_back(literal.value());
  }
  REQUIRE_THAT(literals, Catch::Matchers::AnyMatch(Matches<literal_type<bool>>([] (const auto& literal) { return literal.variable >= num_vars; })));
  REQUIRE_THAT(literals, Catch::Matchers::AnyMatch(Matches<literal_type<bool>>([](const auto& literal) { return literal.variable < num_vars; })));
  REQUIRE_THAT(literals, Catch::Matchers::AnyMatch(Matches<literal_type<bool>>([](const auto& literal) { return literal.value; })));
  REQUIRE_THAT(literals, Catch::Matchers::AnyMatch(Matches<literal_type<bool>>([](const auto& literal) { return !literal.value; })));
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
      REQUIRE(literal.value().variable < num_vars);
      REQUIRE(literal.value().value == 0);
      break;
    }
  }
  // create random_stream with 128 bytes of data with all bits set
  const std::vector<uint8_t> all_ones_data(128, 0xFF);
  random_stream all_ones_stream(all_ones_data.data(), all_ones_data.size());
  const auto all_ones_literal = generator.generate_literal(all_ones_stream, num_vars);
  REQUIRE(all_ones_literal.has_value());
  REQUIRE(all_ones_literal.value_or(literal_type<uint16_t>{0, num_vars}).variable < num_vars);
  REQUIRE(all_ones_literal.value_or(literal_type<uint16_t>{0, num_vars}).value == std::numeric_limits<uint16_t>::max());
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


TEST_CASE("describe matchers", "[fuzz_utils]") // NOLINT(*cognitive-complexity)
{
  const auto is_between = IsBetween(1, 10);
  REQUIRE(is_between.describe() == "is between 1 and 10");

  const auto lambda_matcher = Matches<int>([](int value) { return value > 0; });
  REQUIRE(lambda_matcher.describe() == "matches lambda expression");
}
