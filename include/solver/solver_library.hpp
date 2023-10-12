#ifndef SOLVER_LIBRARY_HPP
#define SOLVER_LIBRARY_HPP

#include <solver/solver_library_export.hpp>

[[nodiscard]] SOLVER_LIBRARY_EXPORT int factorial(int) noexcept;

[[nodiscard]] constexpr int factorial_constexpr(int input) noexcept
{
  if (input == 0) { return 1; }

  return input * factorial_constexpr(input - 1);
}

#endif
