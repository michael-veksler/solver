#include <solver/binary_domain.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_all.hpp>

#include <iterator>
#include <ranges>
#include <span>

using namespace solver;

using Catch::Matchers::Equals;

static constexpr binary_domain empty = [] {
  binary_domain ret;
  ret.clear();
  return ret;
}();

static constexpr const binary_domain zero(false);
static constexpr binary_domain one(true);
static constexpr binary_domain universal;


TEST_CASE("Universal domain", "[binary_domain]")
{
  REQUIRE(universal.is_universal());
  REQUIRE(!universal.is_singleton());
  REQUIRE(!universal.empty());
  REQUIRE((universal.contains(true) && universal.contains(false)));
  REQUIRE((min(universal) == false && max(universal) == true));
}


TEST_CASE("Empty domain", "[binary_domain]")
{
  REQUIRE(!empty.is_universal());
  REQUIRE(!empty.is_singleton());
  REQUIRE(empty.empty());
  REQUIRE(!empty.contains(true));
  REQUIRE(!empty.contains(false));
}

TEST_CASE("Zero domain", "[binary_domain]")
{
  REQUIRE(!zero.is_universal());
  REQUIRE(zero.is_singleton());
  REQUIRE(!zero.empty());
  REQUIRE((zero.contains(false) && !zero.contains(true)));
  REQUIRE((min(zero) == false && max(zero) == false && !get_value(zero)));
}

TEST_CASE("One domain", "[binary_domain]")
{
  REQUIRE(!one.is_universal());
  REQUIRE(one.is_singleton());
  REQUIRE(!one.empty());
  REQUIRE((!one.contains(false) && one.contains(true)));
  REQUIRE((min(one) == true && max(one) == true && get_value(one)));
}

TEST_CASE("Domain equality", "[binary_domain]")
{
  REQUIRE((empty != zero && empty != one && empty != universal && empty == empty));
  REQUIRE((zero != one && zero != universal && zero == zero));
  REQUIRE((one != universal && one == one));
  REQUIRE(universal == universal);
}

TEST_CASE("Domain insertion", "[binary_domain]")
{
  binary_domain entry = empty;
  REQUIRE(entry.size() == 0);
  entry.insert(false);
  REQUIRE(entry == zero);
  entry.insert(false);
  REQUIRE(entry == zero);

  entry.insert(true);
  REQUIRE(entry == universal);
  entry.insert(true);
  entry.insert(false);
  REQUIRE(entry == universal);

  entry = empty;
  entry.insert(true);
  REQUIRE(entry == one);
  entry.insert(false);
  REQUIRE(entry.size() == 2);
  REQUIRE(entry == universal);
}

TEST_CASE("Domain forward iteration", "[binary_domain]")
{
  REQUIRE(empty.begin() == empty.end());
  REQUIRE(one.begin() != one.end());
  REQUIRE_THAT(std::vector(zero.begin(), zero.end()), Equals(std::vector{ false }));
  REQUIRE_THAT(std::vector(one.begin(), one.end()), Equals(std::vector{ true }));
  REQUIRE_THAT(std::vector(universal.begin(), universal.end()), Equals(std::vector{ false, true }));
}

static std::vector<bool> get_reverse(binary_domain dom)
{
  std::vector ret(std::reverse_iterator<binary_domain::iterator>{ dom.end() },
    std::reverse_iterator<binary_domain::iterator>{ dom.begin() });
  return ret;
}

TEST_CASE("Domain backward iteration", "[binary_domain]")
{
  binary_domain::iterator iter;
  REQUIRE(iter == binary_domain::iterator() );
  iter = zero.begin();
  REQUIRE(*iter == false);
  REQUIRE_THAT(get_reverse(zero), Equals(std::vector{ false }));
  REQUIRE_THAT(get_reverse(one), Equals(std::vector{ true }));
  REQUIRE_THAT(get_reverse(universal), Equals(std::vector{ true, false }));
}

TEST_CASE("Domain assignment", "[binary_domain]")
{
  REQUIRE((zero == binary_domain(false) && one == binary_domain(true)));
  binary_domain domain;
  REQUIRE(domain.size() == 2);
  domain = false;
  REQUIRE(domain == zero);
  REQUIRE(domain.size() == 1);
  domain = true;
  REQUIRE(domain == one);
  REQUIRE(domain.size() == 1);
  domain = false;
  REQUIRE(domain == zero);
  REQUIRE(domain.size() == 1);
}

TEST_CASE("to_string", "[binary_domain]")
{
  REQUIRE(to_string(empty) == "{}");
  REQUIRE(to_string(zero) == "{0}");
  REQUIRE(to_string(one) == "{1}");
  REQUIRE(to_string(universal) == "{0, 1}");
}

TEST_CASE("format", "[binary_domain]")
{
  REQUIRE(fmt::format("{}", empty) == "{}");
  REQUIRE(fmt::format("{}",zero) == "{0}");
  REQUIRE(fmt::format("{}",one) == "{1}");
  REQUIRE(fmt::format("{}",universal) == "{0, 1}");
}