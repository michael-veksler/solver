#include <algorithm>
#include <cstdint>
#include <solver/discrete_domain.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_all.hpp>

#include <iterator>
#include <ranges>
#include <span>
#include <vector>
#include <numeric>
#include <random>

using namespace solver;

using Catch::Matchers::Equals;

using uint8_domain = discrete_domain<uint8_t>;
// NOLINTNEXTLINE(cert-err58-cpp)
static const uint8_domain empty = [] {
  uint8_domain ret;
  ret.clear();
  return ret;
}();

static const uint8_domain zero(0); // NOLINT(cert-err58-cpp)
static const uint8_domain one(1);// NOLINT(cert-err58-cpp)
static const uint8_domain universal;// NOLINT(cert-err58-cpp)

// NOLINTNEXTLINE(cert-err58-cpp)
static const std::vector<uint8_t> uint8_full = [] {
  constexpr auto bits_in_value = static_cast<unsigned>(std::numeric_limits<uint8_t>::digits);
  std::vector<uint8_t> ret(1U << bits_in_value);
  std::iota(ret.begin(), ret.end(), static_cast<uint8_t>(0));
  return ret;
}();

template <typename ValueType>
static void domain_shuffled_insert(discrete_domain<ValueType> & domain,
                                                    const std::vector<ValueType> & values, unsigned seed)  {
  auto shuffled_values = values;
  std::mt19937 random(seed);
  std::shuffle(shuffled_values.begin(), shuffled_values.end(), random);
  for (const uint8_t val: shuffled_values) {
    domain.insert(val);
  }
}

// NOLINTNEXTLINE(cert-err58-cpp)
TEST_CASE("Universal domain", "[int8_domain]")
{
  REQUIRE(uint8_domain().is_universal());
  REQUIRE(!uint8_domain().is_singleton());
  REQUIRE(!uint8_domain().empty());
  REQUIRE((uint8_domain().contains(true) && uint8_domain().contains(false)));
  REQUIRE((min(uint8_domain()) == false && max(uint8_domain()) == true));
}


TEST_CASE("Empty domain", "[int8_domain]")
{
  REQUIRE(!empty.is_universal());
  REQUIRE(!empty.is_singleton());
  REQUIRE(empty.empty());
  REQUIRE(!empty.contains(true));
  REQUIRE(!empty.contains(false));
}

TEST_CASE("Zero domain", "[int8_domain]")
{
  REQUIRE(!zero.is_universal());
  REQUIRE(zero.is_singleton());
  REQUIRE(!zero.empty());
  REQUIRE((zero.contains(false) && !zero.contains(true)));
  REQUIRE((min(zero) == false && max(zero) == false && !get_value(zero)));
}

TEST_CASE("One domain", "[int8_domain]")
{
  REQUIRE(!one.is_universal());
  REQUIRE(one.is_singleton());
  REQUIRE(!one.empty());
  REQUIRE((!one.contains(false) && one.contains(true)));
  REQUIRE((min(one) == true && max(one) == true && get_value(one)));
}

TEST_CASE("Domain equality", "[int8_domain]")
{
  REQUIRE((empty != zero && empty != one && empty != uint8_domain() && empty == empty));
  REQUIRE((zero != one && zero != uint8_domain() && zero == zero));
  REQUIRE((one != uint8_domain() && one == one));
  REQUIRE(uint8_domain() == uint8_domain());
}

TEST_CASE("Domain insertion", "[int8_domain]")
{
  uint8_domain entry = empty;
  entry.insert(0);
  REQUIRE(entry == zero);
  entry.insert(0);
  REQUIRE(entry == zero);

  entry.insert(1);
  REQUIRE(entry != uint8_domain());
  domain_shuffled_insert(entry, uint8_full, 2);
  entry.insert(1);
  entry.insert(0);
  REQUIRE(entry == uint8_domain());

  entry = empty;
  entry.insert(1);
  REQUIRE(entry == 1);
  entry.insert(0);
  domain_shuffled_insert(entry, uint8_full, 1);  // NOLINT(cert-msc32-c,cert-msc51-cpp)

  REQUIRE(entry == uint8_domain());
}

TEST_CASE("Domain forward iteration", "[int8_domain]")
{
  REQUIRE(empty.begin() == empty.end());
  REQUIRE(one.begin() != one.end());
  REQUIRE_THAT(std::vector(zero.begin(), zero.end()), Equals(std::vector<uint8_t>{ 0 }));
  REQUIRE_THAT(std::vector(one.begin(), one.end()), Equals(std::vector<uint8_t>{ 1 }));
  REQUIRE_THAT(std::vector(universal.begin(), universal.end()), Equals(uint8_full));
}

static std::vector<uint8_t> get_reverse(const uint8_domain & dom)
{
  std::vector<uint8_t> ret;
  for (auto iter = dom.end(); iter != dom.begin();) {
    --iter;
   ret.push_back(*iter);
  }
  return ret;
}

TEST_CASE("Domain backward iteration", "[int8_domain]")
{
  uint8_domain::iterator iter;
  iter = zero.begin();
  REQUIRE(*iter == 0);
  REQUIRE_THAT(get_reverse(zero), Equals(std::vector<uint8_t>{ 0 }));
  REQUIRE_THAT(get_reverse(one), Equals(std::vector<uint8_t>{ 1 }));
  auto reverse_full = uint8_full;
  std::reverse(reverse_full.begin(), reverse_full.end());
  REQUIRE_THAT(get_reverse(universal), Equals(reverse_full));
}

TEST_CASE("Domain assignment", "[int8_domain]")
{
  REQUIRE((zero == uint8_domain(0) && one == uint8_domain(1)));
  uint8_domain domain;
  domain = 0;
  REQUIRE(domain == zero);
  domain = 1;
  REQUIRE(domain == one);
  domain = 0;
  REQUIRE(domain == zero);
}
