#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace communication {

struct Event {
    uint32_t type;
    uint32_t id;
    std::string data;
};

class Manager {
    uint32_t next_id = 1;

    static uint32_t decode(const char* bytes) {
        uint32_t value = 0;
        for (unsigned i = 0; i < 4; ++i) {
            value |= uint32_t{static_cast<unsigned char>(bytes[i])} << (8 * i);
        }
        return value;
    }

    static void encode(char* bytes, uint32_t value) {
        for (unsigned i = 0; i < 4; ++i) {
            bytes[i] = static_cast<char>(value >> (8 * i));
        }
    }

    static void command(uint32_t type, uint32_t id, std::string_view data = {}) {
        std::array<char, 12> header;
        encode(header.data(), type);
        encode(header.data() + 4, id);
        encode(header.data() + 8, data.size());
        std::cout.write(header.data(), header.size());
        std::cout.write(data.data(), data.size());
        std::cout.flush();
        if (not std::cout) {
            throw std::runtime_error("Cannot write manager command");
        }
    }

    static void input(uint32_t type, uint32_t id, std::string_view data) {
        while (not data.empty()) {
            auto size = std::min<size_t>(data.size(), 64 << 10);
            command(type, id, data.substr(0, size));
            data.remove_prefix(size);
        }
    }

public:
    Manager() {
        command(1, 0, std::string_view{"\1\0\0\0", 4});
        auto event = receive();
        if (event.type != 101 || event.id != 0 || event.data.size() != 12 ||
            decode(event.data.data()) != 1)
        {
            throw std::runtime_error("Invalid manager handshake");
        }
    }

    static Event receive() {
        std::array<char, 12> header;
        std::cin.read(header.data(), header.size());
        if (not std::cin) {
            throw std::runtime_error("Cannot read manager event");
        }
        auto size = decode(header.data() + 8);
        if (size > (64 << 10)) {
            throw std::runtime_error("Invalid manager event size");
        }
        Event event{decode(header.data()), decode(header.data() + 4), std::string(size, '\0')};
        std::cin.read(event.data.data(), size);
        if (not std::cin) {
            throw std::runtime_error("Truncated manager event");
        }
        return event;
    }

    template <class Consume>
    void run_batch(std::string_view data, Consume&& consume) {
        auto id = next_id++;
        command(2, id);
        input(3, id, data);
        command(4, id);
        for (;;) {
            auto event = receive();
            if (event.id != id) {
                throw std::runtime_error("Unexpected instance event");
            }
            if (event.type == 103) {
                consume(event.data);
            } else if (event.type == 105) {
                return;
            }
        }
    }

    std::string run_batch(std::string_view data) {
        std::string output;
        run_batch(data, [&](std::string_view chunk) { output += chunk; });
        return output;
    }

    uint32_t start_stream() {
        auto id = next_id++;
        command(5, id);
        return id;
    }

    static void send(uint32_t id, std::string_view data) { input(6, id, data); }

    static void close_input(uint32_t id) { command(7, id); }

    [[noreturn]] static void finish(bool accepted, double percent, std::string_view comment = {}) {
        std::cerr << (accepted ? "OK\n" : "WRONG\n") << percent << '\n' << comment << '\n';
        std::cerr.flush();
        char result = accepted ? 1 : 0;
        command(8, 0, {&result, 1});
        std::exit(0);
    }
};

} // namespace communication

int main(int argc, char** argv) {
    if (argc != 3 || std::string_view{argv[1]} != "--sim-communication") {
        return 1;
    }
    std::ifstream input{argv[2]};
    std::string message;
    if (not std::getline(input, message)) {
        return 1;
    }
    communication::Manager manager;
    auto encoded = manager.run_batch("1\n" + message + "\n");
    if (encoded.empty() || encoded.size() > 1024 || encoded.back() != '\n') {
        manager.finish(false, 0, "Invalid encoded message");
    }
    encoded.pop_back();
    if (encoded.find_first_not_of("abcdefghijklmnopqrstuvwxyz") != std::string::npos) {
        manager.finish(false, 0, "The encoded message must contain lowercase letters");
    }
    auto decoded = manager.run_batch("2\n" + encoded + "\n");
    manager.finish(decoded == message + "\n", 100);
}
