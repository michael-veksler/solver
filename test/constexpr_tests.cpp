#include <catch2/catch_test_macros.hpp>

#include <solver/solver_library.hpp>

TEST_CASE("Factorials are computed with constexpr", "[factorial]")
{
  STATIC_REQUIRE(factorial_constexpr(0) == 1);        // cppcheck-suppress knownConditionTrueFalse
  STATIC_REQUIRE(factorial_constexpr(1) == 1);        // cppcheck-suppress knownConditionTrueFalse
  STATIC_REQUIRE(factorial_constexpr(2) == 2);        // cppcheck-suppress knownConditionTrueFalse
  STATIC_REQUIRE(factorial_constexpr(3) == 6);        // cppcheck-suppress knownConditionTrueFalse
  STATIC_REQUIRE(factorial_constexpr(10) == 3628800); // cppcheck-suppress knownConditionTrueFalse
}
