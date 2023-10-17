#ifndef SAT_TYPES_HPP
#define SAT_TYPES_HPP

#include <cinttypes>

namespace solver {

enum class solve_status : int8_t { SAT, UNSAT, UNKNOWN };

/**
 * @brief Literal types used for
 *
 */
struct literal_type
{
  bool is_positive = false;
  uint32_t variable = 1;
};

}// namespace solver

#endif
