#ifndef SAT_TYPES_HPP
#define SAT_TYPES_HPP

#include <string>
#include <concepts>

namespace solver {

enum class solve_status : int8_t { SAT, UNSAT, UNKNOWN };
inline std::string to_string(solve_status status)
{
  switch (status) {
  case solve_status::SAT:
    return "SAT";
  case solve_status::UNSAT:
    return "UNSAT";
  case solve_status::UNKNOWN:
    return "UNKNOWN";
  }
  return "invalid(" + std::to_string(static_cast<int8_t>(status)) + ")";
}
/**
 * @brief Literal types used for parsing the input CNF file
 *
 */
template <std::integral Domain>
struct literal_type
{
  Domain value{};
  uint32_t variable = 1;
};

}// namespace solver

#endif
