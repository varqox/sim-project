#pragma once

#include <chrono>
#include <cstdint>
#include <set>
#include <simlib/config_file.hh>
#include <simlib/member_comparator.hh>

/**
 * @brief Sipfile holds sip package configuration that does not fit into Simfile
 */
class Sipfile {
public:
    std::chrono::nanoseconds default_time_limit{};
    uint64_t base_seed{};

    struct GenTest {
        InplaceBuff<16> name;
        InplaceBuff<16> generator;
        InplaceBuff<32> generator_args;

        GenTest(StringView n, StringView g, StringView ga)
        : name(n)
        , generator(g)
        , generator_args(ga) {}
    };

private:
    std::optional<std::set<InplaceBuff<16>, std::less<>>> static_tests; // (test_name)
    std::optional<std::set<GenTest, TRANSPARENT_MEMBER_COMPARATOR(GenTest, name)>> gen_tests;

public:
    auto& get_static_tests() {
        return static_tests ? *static_tests : (load_static_tests(), *static_tests);
    }

    auto& get_gen_tests() { return gen_tests ? *gen_tests : (load_gen_tests(), *gen_tests); }

private:
    ConfigFile config;

public:
    Sipfile() = default;

    /**
     * @brief Loads needed variables from @p sipfile_contents
     * @details Uses ConfigFile::loadConfigFromString()
     *
     * @param sipfile_contents Sipfile file contents
     *
     * @errors May throw from ConfigFile::loadConfigFromString()
     */
    explicit Sipfile(std::string sipfile_contents) {
        config.add_vars("default_time_limit", "base_seed", "static", "gen");
        config.load_config_from_string(std::move(sipfile_contents));
    }

    Sipfile(const Sipfile&) = default;
    Sipfile(Sipfile&&) noexcept = default;
    Sipfile& operator=(const Sipfile&) = default;
    Sipfile& operator=(Sipfile&&) = default;

    ~Sipfile() = default;

    const ConfigFile& config_file() const { return config; }

    /**
     * @brief Loads the default time limit
     * @details Fields:
     *   - default_time_limit
     *
     * @errors Throws an exception of type std::runtime_error if any
     *   validation error occurs
     */
    void load_default_time_limit();

    /**
     * @brief Loads the base seed
     * @details Fields:
     *   - base_seed
     *
     * @param warn whether to warn if the base_seed field is missing
     *
     * @errors Throws an exception of type std::runtime_error if any
     *   validation error occurs
     */
    void load_base_seed(bool warn = true);

    /**
     * @brief Loads non-generated test rules (variable "static")
     * @details Fields:
     *   - static_tests
     *
     * @errors Throws an exception of type std::runtime_error if any
     *   validation error occurs
     */
    void load_static_tests();

    /**
     * @brief Loads generated test rules (variable "gen")
     * @details Fields:
     *   - gen_tests
     *
     * @errors Throws an exception of type std::runtime_error if any
     *   validation error occurs
     */
    void load_gen_tests();
};
