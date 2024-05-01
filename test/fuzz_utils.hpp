#ifndef FUZZ_UTILS_HPP_
#define FUZZ_UTILS_HPP_

#include <cstdint>
#include <optional>
#include <span>
#include <cstring>

namespace solver::fuzzing {

struct random_stream
{
  random_stream(const uint8_t *data, size_t size) : data_span(data, size) {}

  template<typename T>  std::optional<T> get()
  {
    if (data_span.size() < sizeof(T)) { return std::nullopt; }
    T ret = 0;
    std::memcpy(&ret, data_span.data(), sizeof(T));
    data_span = data_span.subspan(sizeof(T));
    return ret;
  }
  std::span<const uint8_t> data_span;

}; // struct random_stream

template<> inline std::optional<bool> random_stream::get()
{
  const auto byte_value = get<uint8_t>();
  if (!byte_value) { return std::nullopt; }
  return *byte_value & 1U;
}

} // namespace solver::fuzzing
#endif // FUZZ_UTILS_HPP_
