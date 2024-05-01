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

#ifdef __GNUC__
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define PACK( DECLARATION ) DECLARATION __attribute__((__packed__))
#endif

#ifdef _MSC_VER
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define PACK( DECLARATION ) __pragma( pack(push, 1) ) DECLARATION __pragma( pack(pop))
#endif

TEST_CASE("struct stream", "[fuzz_utils]") // NOLINT
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
  REQUIRE(got_a.value() == test_data.a);

  const auto got_b = data.get<uint32_t>();
  REQUIRE(got_b.value() == test_data.b);

  const auto bad_c = data.get<uint32_t>();  
  REQUIRE(!bad_c.has_value());

  const auto got_c = data.get<bool>();
  REQUIRE(got_c.value() == bool(test_data.c));

  const auto got_d = data.get<bool>();
  REQUIRE(got_d.value() == bool(test_data.d));

  const auto bad_end = data.get<uint8_t>();
  REQUIRE(!bad_end.has_value());
}
