#include <simlib/logger.hh>
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

void Logger::Appender::flush_impl(
    const char* format1, const char* format2, const char* format3
) noexcept {
    if (flushed_) {
        return;
    }

    if (logger_.lock()) {
        if (label_) {
            try {
                (void)fprintf(
                    logger_.f_,
                    format1,
                    mysql_localdate().c_str(),
                    static_cast<int>(buff_.size),
                    buff_.data()
                );
            } catch (const std::exception&) {
                (void)fprintf(logger_.f_, format2, static_cast<int>(buff_.size), buff_.data());
            }
        } else {
            (void)fprintf(logger_.f_, format3, static_cast<int>(buff_.size), buff_.data());
        }

        (void)fflush(logger_.f_);
        logger_.unlock();
    }

    flushed_ = true;
    buff_ = "";
}
