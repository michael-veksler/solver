#include "solver/discrete_domain.hpp"
#include "solver/int_to_bool_vars.hpp"
#include "solver/sat_types.hpp"
#include <cstdint>
#include <random>
#include <solver/cdcl_sat.hpp>
#include <test_utils.hpp>
#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <catch2/generators/catch_generators.hpp>

using namespace solver;
using namespace solver::testing;

class random_binary_domain_strategy
{
public:
    using domain_type = binary_domain;
    using value_type = typename domain_type::value_type;
    using literal_index_t = uint32_t;

    [[nodiscard]] value_type choose_value(const domain_type &domain)
    {
        if (domain.empty()) {
            throw std::runtime_error("Empty domain");
        }
        if (domain.is_singleton()) {
            return min(domain);
        }
        return m_random() % 2 == 1;
    }
    [[nodiscard]] literal_index_t first_var_to_choose(std::optional<literal_index_t> prev_chosen_var [[maybe_unused]])
    {
        size_t num_important = m_important_variables.size();
        while (num_important > 0) {
            // NOLINTNEXTLINE(misc-const-correctness) - overcome clang implementation bug
            std::uniform_int_distribution<literal_index_t> dist(0, static_cast<literal_index_t>(num_important-1));
            const size_t candidate_index = dist(m_random);

            const literal_index_t candidate_var = m_important_variables[candidate_index];
            if (m_is_free_var(candidate_var)) {

                return candidate_var;
            }
            std::swap(m_important_variables[candidate_index], m_important_variables[--num_important]);
        }
        return 1;
    }

    void set_seed(uint32_t seed) {
        m_random.seed(seed);
    }


    void set_important_variables(std::vector<literal_index_t>&& variables) {
        m_important_variables = std::move(variables);
    }
    void set_is_free_var(std::function<bool(literal_index_t)> is_free_var) {
        m_is_free_var = std::move(is_free_var);
    }
private:
    std::mt19937 m_random{std::random_device()()};
    std::vector<literal_index_t> m_important_variables;
    std::function<bool(literal_index_t)> m_is_free_var;
};



TEST_CASE("Empty set domain", "[int_to_bool]")
{
    using strategy = domain_strategy<binary_domain>;
    cdcl_sat<strategy> solver;
    int_to_bool_vars<uint8_t, strategy> convertor(&solver);
    [[maybe_unused]] auto variable_ = convertor.add_var(discrete_domain<uint8_t>(std::initializer_list<uint8_t>{}));
    REQUIRE(solver.solve() == solve_status::UNSAT);
}

TEST_CASE("single value domain", "[int_to_bool]")
{
    using domain_type =discrete_domain<uint8_t>;
    const uint8_t value = GENERATE(domain_type::MIN_VALUE, domain_type::MAX_VALUE, 1, 2, 3, 4, 5, 6, 7, 8, 9);
    using strategy = domain_strategy<binary_domain>;
    cdcl_sat<strategy> solver;
    int_to_bool_vars<uint8_t, strategy> convertor(&solver);
    auto var = convertor.add_var(discrete_domain<uint8_t>(value));
    REQUIRE(solver.solve() == solve_status::SAT);
    REQUIRE(convertor.get_value(var) == value);
}


TEST_CASE("multi value domain", "[int_to_bool]")  // NOLINT(readability-function-cognitive-complexity)
{
    using domain_type =discrete_domain<uint8_t>;
    using value_type = domain_type::value_type;
    const domain_type domain = {0, 1, 2, 10, 11, domain_type::MAX_VALUE};
    std::map<uint8_t, unsigned> counts;

    const size_t num_samples = domain.size() * 100;
    for (unsigned seed = 1; seed <= num_samples; ++seed) {
        using strategy = random_binary_domain_strategy;
        cdcl_sat<strategy> solver;
        int_to_bool_vars<uint8_t, strategy> convertor(&solver);
        solver.get_strategy().set_seed(seed);
        auto var = convertor.add_var(domain);
        solver.get_strategy().set_important_variables(convertor.get_one_hot_variables());
        solver.get_strategy().set_is_free_var([&solver](uint32_t tested_var) {
            const auto & binary_domain =  solver.get_current_domain(tested_var);
            return binary_domain.is_universal() || binary_domain.size() > 1;
        });
        REQUIRE(solver.solve() == solve_status::SAT);
        const value_type value = convertor.get_value(var);

        REQUIRE(domain.contains(value));
        counts[value]++;
    }
    const size_t average_count = num_samples / domain.size();
    const size_t margin_percent = 20;
    const size_t margin = average_count * margin_percent / 100;
    const size_t lb_count = average_count - margin;
    const size_t ub_count = average_count + margin;
    for (const value_type value: domain) {
        REQUIRE(counts[value] >= lb_count);
        REQUIRE(counts[value] <= ub_count);
    }

}
