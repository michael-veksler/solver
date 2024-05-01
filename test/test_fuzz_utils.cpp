#include "fuzz_utils.hpp"
#include <cstdint>
#include <solver/trivial_sat.hpp>

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <functional>

using namespace solver::fuzzing;


TEST_CASE("empty random_stream", "[fuzz_utils]") // NOLINT
{
  random_stream empty(nullptr, 0);
  REQUIRE(!empty.get<uint8_t>().has_value());
  REQUIRE(!empty.get<bool>().has_value());
}

TEST_CASE("struct stream", "[fuzz_utils]") // NOLINT
{
  struct __attribute__((packed)) test_struct
  {
    uint8_t a;
    uint32_t b;
    uint8_t c;
    uint8_t d;
  };
  test_struct test_data{1, 0x12345678U, 15, 0}; // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
  random_stream data(reinterpret_cast<const uint8_t*>(&test_data), sizeof(test_data)); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
  REQUIRE(data.get<uint8_t>().value() == test_data.a);
  REQUIRE(data.get<uint32_t>().value() == test_data.b);
  REQUIRE(!data.get<uint32_t>().has_value());
  REQUIRE(data.get<bool>().value() == bool(test_data.c));
  REQUIRE(data.get<bool>().value() == bool(test_data.d));
  REQUIRE(!data.get<uint8_t>().has_value());
}
