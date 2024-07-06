#ifndef INT_TO_BOOL_VARS
#define INT_TO_BOOL_VARS
#include "solver/discrete_domain.hpp"
#include <concepts>
#include <optional>
#include <solver/cdcl_sat.hpp>
#include <solver/binary_domain.hpp>
#include <unistd.h>

namespace solver {


template <std::integral ValueType, cdcl_sat_strategy BinaryStrategy>
class int_to_bool_vars {
public:
    using value_type = ValueType;
    using strategy_type = BinaryStrategy;
    using binary_solver_type = cdcl_sat<strategy_type>;
    using binary_variable_handle = typename binary_solver_type::variable_handle;
    using variable_handle = size_t;
    explicit int_to_bool_vars(binary_solver_type * solver): m_solver(solver) {}

    template <domain_class_concept Domain>
    [[nodiscard]] variable_handle add_var(const Domain & domain) {
        std::map<value_type, value_vars> & int_var_values = add_bool_vars(domain);
        m_num_one_hot_vars += domain.size();
        values_are_ordered(int_var_values);
        at_least_one_true(int_var_values);
        at_most_one_true(int_var_values);
        return m_vars.size() - 1;
    }
    std::vector<typename binary_solver_type::variable_handle> get_one_hot_variables() const {
        std::vector<typename binary_solver_type::variable_handle> one_hot_vars;
        one_hot_vars.reserve(m_num_one_hot_vars);
        for (const auto& value_to_vars : m_vars) {
            for (const auto& [_, binary_vars] : value_to_vars) {
                one_hot_vars.push_back(binary_vars.one_hot_variable);
            }
        }
        return one_hot_vars;
    }

    template <domain_class_concept Domain>
    Domain get_current_domain(variable_handle var) const  {
        Domain ret;
        ret.clear();
        for (auto & [value, vars]: m_vars.at(var)) {
            if (m_solver->get_variable_value(vars.one_hot_variable)) {
                ret.insert(value);
            }
        }
        return ret;
    }
    value_type get_value(variable_handle var) const {
        const auto domain= get_current_domain<discrete_domain<value_type>>(var);
        if (domain.size() > 1 || domain.is_universal()) {
            throw std::runtime_error("Multiple values");
        }
        if (domain.empty()) {
            throw std::runtime_error("No value");
        }
        return min(domain);
    }

private:
    struct value_vars {
        /**
         * @brief A variable that gets true iff the signed-variable gets assigned with value.
         */
        typename binary_solver_type::variable_handle one_hot_variable = 0;

        /**
         * @brief A variable that gets true if the signed-variable gets assigned with >= value.
         *
         * Note that this variable is not needed in the two boundaries of the domain because:
         * - If the signed-variable gets assigned with == domain.min(), order_variable is trivially true,
         *   which we are not needed to hold explicitly.
         * - If the signed-variable gets assigned with == domain.max(), order_variable is trivially
         *   equal to the value of one_hot_variable, which is also not needed to hold explicitly.
         */
        std::optional<typename binary_solver_type::variable_handle> order_variable;
    };

    template <domain_class_concept Domain>
    [[nodiscard]] std::map<value_type, value_vars> &  add_bool_vars(const Domain & domain) {
        std::map<value_type, value_vars> & int_var_values = m_vars.emplace_back();
        if (domain.empty()) {
            return int_var_values;
        }
        const value_type min_value = min(domain);
        const value_type max_value = max(domain);
        for (value_type value: domain) {
            value_vars & vars = int_var_values.try_emplace(value).first->second;
            vars.one_hot_variable = m_solver->add_var();
            if (value == max_value) {
                vars.order_variable = vars.one_hot_variable;
            } else  if (value != min_value) {
                vars.order_variable = m_solver->add_var();
            }
            int_var_values[value] = vars;
        }
        return int_var_values;
    }
    void values_are_ordered(const std::map<value_type, value_vars>& int_var_values) {
        std::optional<binary_variable_handle> prev_order_variable;
        for (const auto & [_, vars]: int_var_values) {
            if (!vars.order_variable.has_value()) {
                continue;
            }
            if (prev_order_variable.has_value()) {
                add_implies(vars.order_variable.value(), prev_order_variable.value());
            }
            prev_order_variable = vars.order_variable;
        }
    }
    void at_least_one_true(const std::map<value_type, value_vars>& int_var_values) {
        auto & new_clause = m_solver->add_clause();
        for (const auto & [_, vars]: int_var_values) {
            new_clause.add_literal(vars.one_hot_variable, true);
        }
    }
    void at_most_one_true(const std::map<value_type, value_vars>& int_var_values) {
        std::optional<binary_variable_handle> prev_one_hot_variable;
        for (const auto & [_, vars]: int_var_values) {
            if (vars.order_variable.has_value()) {
                binary_variable_handle order_variable = vars.order_variable.value();
                add_implies(vars.one_hot_variable, order_variable);
                if (prev_one_hot_variable.has_value()) {
                    add_implies_not(prev_one_hot_variable.value(), order_variable);
                }
            }
            prev_one_hot_variable = vars.one_hot_variable;
        }
    }
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    void add_implies(binary_variable_handle pre, binary_variable_handle post) {
        if (pre == post) {
            return;
        }
        auto & imply_clause = m_solver->add_clause();
        imply_clause.add_literal(pre, false);
        imply_clause.add_literal(post, true);
    }
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    void add_implies_not(binary_variable_handle pre, binary_variable_handle post) {
        auto & imply_clause = m_solver->add_clause();
        imply_clause.add_literal(pre, false);
        imply_clause.add_literal(post, false);
    }

    std::vector<std::map<value_type, value_vars>> m_vars;
    size_t m_num_one_hot_vars = 0;
    binary_solver_type * m_solver = nullptr;
};

}
#endif // INT_TO_BOOL_VARS