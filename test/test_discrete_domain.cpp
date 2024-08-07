#include <algorithm>
#include <cstdint>
#include <limits>
#include <solver/discrete_domain.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_all.hpp>

#include <iterator>
#include <numeric>
#include <random>
#include <vector>

using namespace solver;

using Catch::Matchers::Equals;

using uint8_domain = discrete_domain<uint8_t>;
// NOLINTNEXTLINE(cert-err58-cpp)
static const uint8_domain empty = [] {
  uint8_domain ret;
  ret.clear();
  return ret;
}();

static const uint8_domain zero(0);// NOLINT(cert-err58-cpp)
static const uint8_domain one(1);// NOLINT(cert-err58-cpp)
static const uint8_domain biggest(uint8_domain::MAX_VALUE);// NOLINT(cert-err58-cpp)
static const uint8_domain universal;// NOLINT(cert-err58-cpp)

// NOLINTNEXTLINE(cert-err58-cpp)
static const std::vector<uint8_t> uint8_full = [] {
  std::vector<uint8_t> ret(uint8_domain::MAX_VALUE + 1, 0);
  std::iota(ret.begin(), ret.end(), static_cast<uint8_t>(0));
  return ret;
}();

template<typename ValueType>
static void
  domain_shuffled_insert(discrete_domain<ValueType> &domain, const std::vector<ValueType> &values, unsigned seed)
{
  auto shuffled_values = values;
  std::mt19937 random(seed);
  std::shuffle(shuffled_values.begin(), shuffled_values.end(), random);
  for (const uint8_t val : shuffled_values) { domain.insert(val); }
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity,cert-err58-cpp)
TEST_CASE("Universal domain", "[int8_domain]")
{
  REQUIRE(universal.size() == uint8_domain::MAX_VALUE - uint8_domain::MIN_VALUE + 1);
  REQUIRE(universal.is_universal());
  REQUIRE(!universal.is_singleton());
  REQUIRE(!universal.empty());
  REQUIRE(
    (universal.contains(1) && universal.contains(0) && universal.contains(uint8_domain::MAX_VALUE)));
  REQUIRE((min(universal) == 0 && max(universal) == uint8_domain::MAX_VALUE));
  REQUIRE(!universal.contains(std::numeric_limits<uint8_t>::max()));
}


TEST_CASE("Empty domain", "[int8_domain]")
{
  REQUIRE(empty.size() == 0);  // NOLINT(readability-container-size-empty)
  REQUIRE(!empty.is_universal());
  REQUIRE(!empty.is_singleton());
  REQUIRE(empty.empty());
  REQUIRE(!empty.contains(1));
  REQUIRE(!empty.contains(0));
  REQUIRE(!empty.contains(uint8_domain::MAX_VALUE));
}

TEST_CASE("Zero domain", "[int8_domain]")  // NOLINT(readability-function-cognitive-complexity)
{
  REQUIRE(zero.size() == 1);
  REQUIRE(!zero.is_universal());
  REQUIRE(zero.is_singleton());
  REQUIRE(!zero.empty());
  REQUIRE((zero.contains(0) && !zero.contains(1)));
  REQUIRE((min(zero) == 0 && max(zero) == 0 && get_value(zero) == 0));
}

TEST_CASE("One domain", "[int8_domain]")  // NOLINT(readability-function-cognitive-complexity)
{
  REQUIRE(one.size() == 1);
  REQUIRE(!one.is_universal());
  REQUIRE(one.is_singleton());
  REQUIRE(!one.empty());
  REQUIRE((!one.contains(0) && one.contains(1) && !one.contains(uint8_domain::MAX_VALUE)));
  REQUIRE((min(one) == 1 && max(one) == 1 && get_value(one) == 1));
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST_CASE("Biggest domain", "[int8_domain]")
{
  REQUIRE(biggest.size() == 1);
  REQUIRE(!biggest.is_universal());
  REQUIRE(biggest.is_singleton());
  REQUIRE(!biggest.empty());
  REQUIRE((!biggest.contains(0) && !biggest.contains(1) && biggest.contains(uint8_domain::MAX_VALUE)));
  REQUIRE(min(biggest) == uint8_domain::MAX_VALUE);
  REQUIRE(max(biggest) == uint8_domain::MAX_VALUE);
  REQUIRE(get_value(biggest) == uint8_domain::MAX_VALUE);
}

TEST_CASE("Domain equality", "[int8_domain]")
{
  REQUIRE((empty != zero && empty != one && empty != universal && empty == empty));
  REQUIRE((zero != one && zero != universal && zero == zero));
  REQUIRE((one != universal && one == one && biggest == biggest));
  REQUIRE(universal == universal);
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST_CASE("Domain insertion", "[int8_domain]")
{
  uint8_domain entry = empty;
  entry.insert(0);
  REQUIRE(entry == zero);
  entry.insert(0);
  REQUIRE(entry == zero);

  entry.insert(1);
  REQUIRE_THAT(std::vector<uint8_t>(entry.begin(), entry.end()), Equals(std::vector<uint8_t>{ 0, 1 }));
  entry.insert(uint8_domain::MAX_VALUE);
  REQUIRE_THAT(std::vector(entry.begin(), entry.end()), Equals(std::vector<uint8_t>{ 0, 1, uint8_domain::MAX_VALUE }));
  REQUIRE(entry != universal);
  domain_shuffled_insert(entry, uint8_full, 2);
  entry.insert(1);
  entry.insert(0);
  REQUIRE(entry == universal);

  entry = empty;
  entry.insert(1);
  REQUIRE(entry == 1);
  REQUIRE_THROWS_AS(entry = std::numeric_limits<uint8_t>::max(), std::invalid_argument);
  REQUIRE(entry == 1);
  entry.insert(0);
  domain_shuffled_insert(entry, uint8_full, 3);// NOLINT(cert-msc32-c,cert-msc51-cpp)

  REQUIRE(entry == universal);
  REQUIRE_THROWS_AS(entry = std::numeric_limits<uint8_t>::max(), std::invalid_argument);
  REQUIRE(entry == universal);
}

TEST_CASE("Domain forward iteration", "[int8_domain]")
{
  REQUIRE(empty.begin() == empty.end());
  REQUIRE(one.begin() != one.end());
  REQUIRE_THAT(std::vector(zero.begin(), zero.end()), Equals(std::vector<uint8_t>{ 0 }));
  REQUIRE_THAT(std::vector(one.begin(), one.end()), Equals(std::vector<uint8_t>{ 1 }));
  REQUIRE_THAT(std::vector(biggest.begin(), biggest.end()), Equals(std::vector<uint8_t>{ uint8_domain::MAX_VALUE }));
  REQUIRE_THAT(std::vector(universal.begin(), universal.end()), Equals(uint8_full));
}

static std::vector<uint8_t> get_reverse(const uint8_domain &dom)
{
  std::vector<uint8_t> ret;
  std::reverse_copy(dom.begin(), dom.end(), std::back_inserter(ret));
  return ret;
}

TEST_CASE("Domain backward iteration", "[int8_domain]")
{
  uint8_domain::iterator iter;
  iter = zero.begin();
  REQUIRE(*iter == 0);
  REQUIRE_THAT(get_reverse(zero), Equals(std::vector<uint8_t>{ 0 }));
  REQUIRE_THAT(get_reverse(one), Equals(std::vector<uint8_t>{ 1 }));
  REQUIRE_THAT(get_reverse(biggest), Equals(std::vector<uint8_t>{ uint8_domain::MAX_VALUE }));
  auto reverse_full = uint8_full;
  std::reverse(reverse_full.begin(), reverse_full.end());
  REQUIRE_THAT(get_reverse(universal), Equals(reverse_full));
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST_CASE("Domain assignment", "[int8_domain]")
{
  REQUIRE((zero == uint8_domain(0) && one == uint8_domain(1)));
  uint8_domain domain;
  // cppcheck-suppress redundantAssignment
  domain = 0;
  REQUIRE(domain == zero);
  // cppcheck-suppress redundantAssignment
  domain = 1;
  REQUIRE(domain == one);
  // cppcheck-suppress redundantAssignment
  domain = 0;
  REQUIRE(domain == zero);
  // cppcheck-suppress redundantAssignment
  domain = uint8_domain::MAX_VALUE;
  REQUIRE(domain == biggest);
  // cppcheck-suppress redundantAssignment `
  REQUIRE_THROWS_AS(domain = std::numeric_limits<uint8_t>::max(), std::invalid_argument);
  REQUIRE(domain == biggest);
}

// a test case for to_string
TEST_CASE("to_string", "[int8_domain]")// NOLINT(readability-function-cognitive-complexity)
{
  REQUIRE(to_string(empty) == "{}");
  REQUIRE(to_string(zero) == "{0}");
  REQUIRE(to_string(one) == "{1}");
  REQUIRE(to_string(biggest) == "{254}");
  REQUIRE(to_string(universal) == "{*}");
  REQUIRE(to_string(uint8_domain{ 0, 1 }) == "{0, 1}");
  REQUIRE(to_string(uint8_domain{ 2, 20, 254 }) == "{2, 20, 254}");
}

// Write test that formats int8_domain with fmt::format similar to the above to_string test
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST_CASE("fmt::format", "[int8_domain]")
{
  REQUIRE(fmt::format("{}", empty) == "{}");
  REQUIRE(fmt::format("{}", zero) == "{0}");
  REQUIRE(fmt::format("{}", one) == "{1}");
  REQUIRE(fmt::format("{}", biggest) == "{254}");
  REQUIRE(fmt::format("{}", universal) == "{*}");
  REQUIRE(fmt::format("{}", uint8_domain{ 0, 254 }) == "{0, 254}");
  REQUIRE(fmt::format("{}", uint8_domain{ 2, 1, 254 }) == "{1, 2, 254}");
}

TEST_CASE("Domain size", "[int8_domain]")
{
  uint8_domain domain;
  domain.clear();
  REQUIRE(domain.size() == 0); // NOLINT(*-*container-size-empty)
  unsigned expected_size = 0;
  for (unsigned even = 0; even <= uint8_domain::MAX_VALUE ; even += 2) {
    domain.insert(static_cast<uint8_t>(even));
    ++expected_size;
    REQUIRE(domain.size() == expected_size);
  }
  for (unsigned odd = 1; odd <= uint8_domain::MAX_VALUE ; odd += 2) {
    domain.insert(static_cast<uint8_t>(odd));
    ++expected_size;
    REQUIRE(domain.size() == expected_size);
  }
}
