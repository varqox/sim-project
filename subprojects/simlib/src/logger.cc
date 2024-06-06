#include <simlib/errmsg.hh>
#include <simlib/logger.hh>
#include <simlib/macros/throw.hh>
#include <simlib/time.hh>

using std::string;

Logger::Logger(FilePath filename) : f_(fopen(filename, "abe")), opened_(true) {
    if (f_ == nullptr) {
        THROW("fopen('", filename, "') failed", errmsg());
    }
}

void Logger::open(FilePath filename) {
    FILE* f = fopen(filename, "abe");
    if (f == nullptr) {
        THROW("fopen('", filename, "') failed", errmsg());
    }

    close();
    f_ = f;
}

void Logger::Appender::flush_impl(const char* newline_or_empty_str) noexcept {
    if (flushed_) {
        return;
    }

    if (logger_.lock()) {
        if (label_) {
            try {
                (void)fprintf(
                    logger_.f_,
                    "[ %s ] %.*s%s",
                    local_mysql_datetime().c_str(),
                    static_cast<int>(buff_.size),
                    buff_.data(),
                    newline_or_empty_str
                );
            } catch (const std::exception&) {
                (void)fprintf(
                    logger_.f_,
                    "[ unknown time ] %.*s%s",
                    static_cast<int>(buff_.size),
                    buff_.data(),
                    newline_or_empty_str
                );
            }
        } else {
            (void)fprintf(
                logger_.f_,
                "%.*s%s",
                static_cast<int>(buff_.size),
                buff_.data(),
                newline_or_empty_str
            );
        }

        (void)fflush(logger_.f_);
        logger_.unlock();
    }

    flushed_ = true;
    buff_ = "";
}
