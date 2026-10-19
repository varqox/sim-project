#pragma once

#include <cstdint>
#include <optional>
#include <simlib/config_file.hh>
#include <simlib/macros/throw.hh>
#include <simlib/utilities.hh>
#include <string>
#include <unistd.h>

namespace sim {

struct CommunicationConfig {
    static constexpr size_t max_size = 4096;
    static constexpr uint32_t max_instances = 32;
    static constexpr uint32_t max_concurrent_instances = 8;
    static constexpr uint64_t default_manager_memory_in_bytes = 256 << 20;
    static constexpr uint64_t default_session_memory_in_bytes = 2ULL << 30;
    static constexpr uint64_t relay_memory_in_bytes = 16 << 20;

    std::string manager;
    uint32_t instances = 2;
    std::optional<uint32_t> min_instances;
    uint32_t max_concurrent = 1;

    static void validate_memory_limits(
        uint32_t concurrent,
        uint64_t program_memory,
        uint64_t manager_memory = default_manager_memory_in_bytes,
        uint64_t session_memory = default_session_memory_in_bytes
    ) {
        static const auto page_size = sysconf(_SC_PAGESIZE);
        if (page_size <= 0 or concurrent == 0 or concurrent > max_concurrent_instances or
            program_memory == 0 or manager_memory == 0 or session_memory < relay_memory_in_bytes)
        {
            THROW("Invalid communication memory limits");
        }
        auto available = session_memory - relay_memory_in_bytes;
        if (manager_memory > available or
            static_cast<uint64_t>(page_size) > available - manager_memory)
        {
            THROW("Communication manager exceeds session memory limit");
        }
        available -= manager_memory + page_size;
        if (program_memory > available / concurrent or
            static_cast<uint64_t>(page_size) > available / concurrent - program_memory)
        {
            THROW("Communication instances exceed session memory limit");
        }
    }

    explicit CommunicationConfig(const ConfigFile& config) {
        if (not config["manager"].is_set()) {
            THROW("Missing communication setting: manager");
        }
        for (auto key : {"manager", "instances", "min_instances", "max_concurrent"}) {
            if (config[key].is_array()) {
                THROW("Communication setting must be a scalar: ", key);
            }
            if (config[key].as_string().size() > max_size) {
                THROW("Communication setting is too long: ", key);
            }
        }
        manager = config["manager"].as_string();
        auto read_number = [&](const char* key) {
            auto value = config[key].as<uint32_t>();
            if (not value) {
                THROW("Invalid communication setting: ", key);
            }
            return *value;
        };
        if (config["instances"].is_set()) {
            instances = read_number("instances");
        }
        if (config["min_instances"].is_set()) {
            min_instances = read_number("min_instances");
        }
        if (config["max_concurrent"].is_set()) {
            max_concurrent = read_number("max_concurrent");
        }
        if (instances == 0 or instances > max_instances) {
            THROW("Communication instances must be between 1 and ", max_instances);
        }
        if (min_instances and (*min_instances == 0 or *min_instances > instances)) {
            THROW("Communication min_instances must be between 1 and instances");
        }
        if (max_concurrent == 0 or max_concurrent > instances or
            max_concurrent > max_concurrent_instances)
        {
            THROW("Invalid communication max_concurrent");
        }
        if (not has_prefix(manager, "manager/") or manager.size() == 8 or manager.back() == '/') {
            THROW("Communication manager must be inside manager/");
        }
        for (size_t beg = 0; beg < manager.size();) {
            auto end = manager.find('/', beg);
            if (end == std::string::npos) {
                end = manager.size();
            }
            auto component = manager.substr(beg, end - beg);
            if (component.empty() or component == "." or component == ".." or
                component.find('\0') != std::string::npos)
            {
                THROW("Invalid communication manager path");
            }
            beg = end + 1;
        }
    }

    [[nodiscard]] std::string dump_settings() const {
        return concat_tostr(
            "manager: ",
            ConfigFile::escape_string(manager),
            "\ninstances: ",
            instances,
            min_instances ? concat_tostr("\nmin_instances: ", *min_instances) : "",
            "\nmax_concurrent: ",
            max_concurrent,
            '\n'
        );
    }
};

} // namespace sim
