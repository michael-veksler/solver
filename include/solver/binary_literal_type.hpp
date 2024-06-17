#ifndef binary_literal_type_hpp
#define binary_literal_type_hpp

#include <cassert>
#include <cstdint>
#include <cmath>
#include <stdexcept>
#include <iostream>
namespace solver {
class binary_literal_type {
public:
    using value_type = bool;
    using variable_index_type = uint32_t;
    binary_literal_type() : m_value(1) {}
    binary_literal_type(variable_index_type variable_index, value_type literal_value) {
        if (variable_index > static_cast<variable_index_type>(std::numeric_limits<int32_t>::max())) {
            throw std::out_of_range("Value out of bounds");
        }
        m_value = static_cast<int32_t>(variable_index);
        if (!literal_value) {
            m_value = -m_value;
        }
    }
    [[nodiscard]] value_type get_value() const {
        assert(m_value != 0 && "Can't get variable of 0");
        return m_value > 0;
    }
    [[nodiscard]] variable_index_type get_variable() const {
        assert(m_value != std::numeric_limits<int32_t>::min() && "Can't negate INT32_MIN");
        assert(m_value != 0 && "Can't get variable of 0");
        return static_cast<variable_index_type>(m_value < 0 ? -m_value : m_value);
    }
    friend auto operator<=>(const binary_literal_type&, const binary_literal_type&) = default;
    // an ostream operator that prints m_value
    friend std::ostream& operator<<(std::ostream& out, const binary_literal_type& literal) {
        return out << literal.m_value;
    }
private:
    int32_t m_value;
};
} // namespace solver

#endif // binary_literal_type_hpp
