#ifndef DIMACS_PARSER_HPP
#define DIMACS_PARSER_HPP
#include <functional>
#include <iosfwd>
#include <optional>
#include <string>
#include <string_view>

namespace solver {
class dimacs_parser
{
public:
  explicit dimacs_parser(std::function<bool(std::string &)> get_line) : m_get_line(std::move(get_line)) {}
  void parse(const std::function<void(unsigned, unsigned)> &construct_problem,
    const std::function<void(const std::vector<int> &)> &register_clause);

private:
  static std::string_view lstrip(std::string_view view)
  {
    view.remove_prefix(std::min(view.find_first_not_of("\t "), view.size()));
    return view;
  }
  void parse_header(const std::function<void(unsigned, unsigned)> &construct_problem);
  [[nodiscard]] std::optional<std::string_view> get_line()
  {
    ++m_current_line_num;
    return m_get_line(m_buffer) ? std::optional{ lstrip(m_buffer) } : std::nullopt;
  }
  [[nodiscard]] std::optional<std::string_view> get_nonempty_line()
  {
    while (true) {
      const std::optional<std::string_view> line = get_line();
      if (!line) { return std::nullopt; }
      if (!line->empty() && (*line)[0] != 'c') { return line; }
    }
  }

  const std::function<bool(std::string &)> m_get_line;
  unsigned m_current_line_num = 0;
  std::string m_buffer;
};

}// namespace solver


#endif
