#ifndef TEST_UTILS_HPP
#define TEST_UTILS_HPP

#include <spdlog/sinks/ostream_sink.h>
#include <spdlog/spdlog.h>
#include <memory>
#include <ostream>
#include <sstream>
#include <string>

namespace solver::testing {
/**
 * @brief A RAII object to redirect the default spdlog logger to a stream.
 *
 * The redirection is undone upon destruction of the object.
 *
 */
class log_redirect
{
public:
  explicit log_redirect(std::ostream &out)
  {
    auto ostream_sink = std::make_shared<spdlog::sinks::ostream_sink_st>(out);
    auto ostream_logger = std::make_shared<spdlog::logger>(logger_name, ostream_sink);
    ostream_logger->set_pattern("<<<%v>>>");
    ostream_logger->set_level(spdlog::level::debug);
    spdlog::set_default_logger(ostream_logger);
  }
  log_redirect(const log_redirect &) = delete;
  log_redirect(log_redirect &&) = delete;
  log_redirect &operator=(const log_redirect &) = delete;
  log_redirect &operator=(log_redirect &&) = delete;

  ~log_redirect()
  {
    spdlog::set_default_logger(std::move(original_logger));
    spdlog::drop(logger_name);
  }

private:
  static constexpr const char *logger_name = "test_logger";

  std::shared_ptr<spdlog::logger> original_logger = spdlog::default_logger();
};

class log_capture
{
public:
    template <std::invocable Func>
    explicit log_capture(const Func & func) {
        func();
    }
    [[nodiscard]] std::string str() const {
        return m_stream.str();
    }
private:
    std::stringstream m_stream;
    log_redirect m_redirect{m_stream};
};

}// namespace solver::testing


#endif