#ifndef STATE_SAVER_HPP
#define STATE_SAVER_HPP

namespace solver {
/**
 * @brief A state_saver which can be reset
 *
 * Unlike boost::state_saver, it is possible to reset it so that no value will be restored.
 *
 * @tparam ValueType The type of the saved data
 */
template <class ValueType>
class state_saver
{
public:
  explicit state_saver(ValueType & state) : m_state(&state), m_original_value(state) {}
  state_saver(const state_saver &) = delete;
  state_saver(state_saver &&) = delete;
  state_saver& operator=(const state_saver &) = delete;
  state_saver& operator=(const state_saver &&) = delete;

  void reset() { m_state = nullptr; }

  ~state_saver() {
    if (m_state) {
      *m_state= m_original_value;
    }
  }
private:
  ValueType * m_state;
  const ValueType m_original_value;
};
}
#endif
