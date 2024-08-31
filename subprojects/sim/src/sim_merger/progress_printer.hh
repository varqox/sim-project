#pragma once

#include <atomic>
#include <chrono>
#include <future>
#include <mutex>
#include <optional>
#include <simlib/logger.hh>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>

template <class Id>
class ProgressPrinterBase {
    std::string msg;
    std::thread printing_thread;
    bool running = false;
    std::future<void> finished;
    std::promise<void> finished_promise;

protected:
    virtual void set_last_noted_id(Id id) = 0;
    virtual Id read_last_noted_id() = 0;

public:
    explicit ProgressPrinterBase(std::string msg) noexcept : msg{std::move(msg)} {}

    ProgressPrinterBase(const ProgressPrinterBase&) = delete;
    ProgressPrinterBase(ProgressPrinterBase&&) = delete;
    ProgressPrinterBase& operator=(const ProgressPrinterBase&) = delete;
    ProgressPrinterBase& operator=(ProgressPrinterBase&&) = delete;

    virtual ~ProgressPrinterBase() { done(); }

    void note_id(Id id) {
        set_last_noted_id(std::move(id));
        if (!running) {
            finished = finished_promise.get_future();
            printing_thread = std::thread{[this] {
                std::optional<Id> last_printed_id;
                auto print = [&] {
                    auto id = read_last_noted_id();
                    if (id != last_printed_id) {
                        stdlog(msg, ' ', id);
                        last_printed_id.emplace(std::move(id));
                    }
                };
                print();
                for (;;) {
                    auto res = finished.wait_for(std::chrono::milliseconds{250});
                    if (res != std::future_status::timeout) {
                        break;
                    }
                    print();
                }
                print();
            }};
            running = true;
        }
    }

    void done() {
        if (running) {
            finished_promise.set_value();
            printing_thread.join();
            running = false;
        }
    }
};

template <class Id, class = void>
class ProgressPrinter;

template <class Id>
class ProgressPrinter<Id, std::enable_if_t<std::is_trivially_copyable_v<Id>>>
: public ProgressPrinterBase<Id> {
    std::atomic<Id> last_noted_id;

protected:
    void set_last_noted_id(Id id) override {
        last_noted_id.store(std::move(id), std::memory_order_relaxed);
    }

    Id read_last_noted_id() override { return last_noted_id.load(std::memory_order_relaxed); }

public:
    explicit ProgressPrinter(std::string msg) noexcept : ProgressPrinterBase<Id>{std::move(msg)} {}

    ProgressPrinter(const ProgressPrinter&) = delete;
    ProgressPrinter(ProgressPrinter&&) = delete;
    ProgressPrinter& operator=(const ProgressPrinter&) = delete;
    ProgressPrinter& operator=(ProgressPrinter&&) = delete;

    ~ProgressPrinter() {
        // Need to call it here, because this object needs to exist during the call, because under
        // the hood, done() causes the other thread to call read_last_noted_id() and it needs this
        // object to exist.
        ProgressPrinterBase<Id>::done();
    }
};

template <class Id>
class ProgressPrinter<Id, std::enable_if_t<!std::is_trivially_copyable_v<Id>>>
: public ProgressPrinterBase<Id> {
    std::mutex mutex;
    Id last_noted_id;

protected:
    void set_last_noted_id(Id id) override {
        auto guard = std::lock_guard{mutex};
        last_noted_id = std::move(id);
    }

    Id read_last_noted_id() override {
        auto guard = std::lock_guard{mutex};
        return last_noted_id;
    }

public:
    explicit ProgressPrinter(std::string msg) noexcept : ProgressPrinterBase<Id>{std::move(msg)} {}

    ProgressPrinter(const ProgressPrinter&) = delete;
    ProgressPrinter(ProgressPrinter&&) = delete;
    ProgressPrinter& operator=(const ProgressPrinter&) = delete;
    ProgressPrinter& operator=(ProgressPrinter&&) = delete;

    ~ProgressPrinter() {
        // Need to call it here, because this object needs to exist during the call, because under
        // the hood, done() causes the other thread to call read_last_noted_id() and it needs this
        // object to exist.
        ProgressPrinterBase<Id>::done();
    }
};
