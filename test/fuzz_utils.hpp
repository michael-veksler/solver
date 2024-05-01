#ifndef FUZZ_UTILS_HPP_
#define FUZZ_UTILS_HPP_

#include <cstdint>
#include <optional>
#include <span>
#include <cstring>
#include <type_traits>

namespace solver::fuzzing {

struct random_stream
{
  random_stream(const uint8_t *data, size_t size) : data_span(data, size) {}

  template<typename T>  std::optional<T> get()
  {
    if (data_span.size() < sizeof(T)) { return std::nullopt; }
    T ret = 0;
    if constexpr (std::is_same_v<T, bool>) {
      ret = data_span[0];
    } else {
      std::memcpy(&ret, data_span.data(), sizeof(T));
    }
    std::memcpy(&ret, data_span.data(), sizeof(T));
    data_span = data_span.subspan(sizeof(T));
    return ret;
  }
  std::span<const uint8_t> data_span;

}; // struct random_stream


} // namespace solver::fuzzing
#endif // FUZZ_UTILS_HPP_
