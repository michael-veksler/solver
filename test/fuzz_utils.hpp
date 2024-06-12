#ifndef FUZZ_UTILS_HPP_
#define FUZZ_UTILS_HPP_

#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <cstring>
#include <type_traits>
#include <vector>
#include <concepts>

#include <solver/sat_types.hpp>

namespace solver::fuzzing {

struct random_stream
{
  random_stream(const uint8_t *data, size_t size) : data_span(data, size) {}

  template<typename T>  std::optional<T> get()
  {
    if (data_span.size() < sizeof(T)) { return std::nullopt; }
    T ret = 0;
    if constexpr (std::is_same_v<T, bool>) {
      ret = data_span[0] % 2 != 0;
    } else {
      std::memcpy(&ret, data_span.data(), sizeof(T));
    }
    data_span = data_span.subspan(sizeof(T));
    return ret;
  }
  std::span<const uint8_t> data_span;
}; // struct random_stream


template <std::integral Domain>
class csp_generator
{
public:
  explicit csp_generator(std::pair<Domain, Domain> bounds, bool test_out_of_range)
    : m_min_val(bounds.first), m_max_val(bounds.second), m_test_out_of_range(test_out_of_range) {}
  csp_generator() = default;

  std::optional<literal_type<Domain>>
  generate_literal(random_stream &random_data, unsigned num_vars)
  {
    const std::optional<Domain> value = random_data.get<Domain>();
    if (!value) { return std::nullopt; }
    const std::optional variable_index = generate_variable_index(random_data, num_vars);
    if (variable_index) {
      return literal_type<Domain>{ .value = *value, .variable = *variable_index};
    } else {
      return std::nullopt;
    }
  }

  std::optional<uint32_t>
  generate_variable_index(random_stream &random_data, unsigned num_vars)
  {
    const bool invalid_var = m_test_out_of_range && random_data.get<uint8_t>().value_or(1) == 0;

    std::optional<uint32_t> variable_index = random_data.get<uint32_t>();
    if (!variable_index) { return std::nullopt; }

    if (invalid_var) {
      while (variable_index && *variable_index < num_vars) {
        variable_index = random_data.get<uint16_t>();
      }
    } else {
      *variable_index %= num_vars;
    }
    return variable_index;
  }

  std::vector<literal_type<Domain>> generate_literals(random_stream &random_data, unsigned num_vars)
  {
    const std::optional<unsigned> num_literals_source = random_data.get<uint16_t>();
    if (!num_literals_source) { return {}; }
    const unsigned num_literals = *num_literals_source % num_vars + 1;
    std::vector<literal_type<Domain>> literals;
    literals.reserve(num_literals);
    for (unsigned i = 0; i != num_literals; ++i) {
      std::optional<literal_type<Domain>> literal = generate_literal(random_data, num_vars);
      if (!literal) { break; }
      literals.push_back(*literal);
    }
    return literals;
  }

  private:
    Domain m_min_val = std::numeric_limits<Domain>::min();
    Domain m_max_val  = std::numeric_limits<Domain>::max();
    bool m_test_out_of_range = false;
};



} // namespace solver::fuzzing
#endif // FUZZ_UTILS_HPP_
