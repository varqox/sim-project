#pragma once

#include <cstdio>
#include <memory>
#include <simlib/defer.hh>
#include <simlib/file_contents.hh>
#include <simlib/file_descriptor.hh>
#include <simlib/logger.hh>
#include <simlib/throw_assert.hh>
#include <simlib/unlinked_temporary_file.hh>

template <class LoggingFunc>
std::string intercept_logger(Logger& logger, LoggingFunc&& logging_func) {
    FileDescriptor fd{open_unlinked_tmp_file()};
    throw_assert(fd.is_open());

    std::unique_ptr<FILE, int (*)(FILE*)> stream = {fdopen(fd, "r+"), fclose};
    throw_assert(stream);
    (void)fd.release(); // Now stream owns it

    FILE* logger_stream = stream.get();
    stream.reset(logger.exchange_log_stream(stream.release()));
    bool old_label = logger.label(false);
    Defer guard = [&] {
        logger.use(stream.release());
        logger.label(old_label);
    };

    logging_func();

    // Get logged data
    rewind(logger_stream);
    return get_file_contents(fileno(logger_stream));
}
