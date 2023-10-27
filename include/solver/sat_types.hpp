#ifndef SAT_TYPES_HPP
#define SAT_TYPES_HPP

#include <cinttypes>
#include <string>

namespace solver {

enum class solve_status : int8_t { SAT, UNSAT, UNKNOWN };
inline std::string to_string(solve_status status) {
  switch (status) {
    case solve_status::SAT:     return "SAT";
    case solve_status::UNSAT:   return "UNSAT";
    case solve_status::UNKNOWN: return "UNKNOWN";
  }
  return "Unknown";
}
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
