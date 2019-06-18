#include "../include/logger.h"
#include "../include/debug.h"
#include "../include/time.h"

using std::string;

Logger stdlog(stderr), errlog(stderr);

Logger::Logger(FilePath filename) : f_(fopen(filename, "abe")), opened_(true) {
	if (f_ == nullptr)
		THROW("fopen('", filename, "') failed", errmsg());
}

void Logger::open(FilePath filename) {
	FILE* f = fopen(filename, "abe");
	if (f == nullptr)
		THROW("fopen('", filename, "') failed", errmsg());

	close();
	f_ = f;
}

void Logger::Appender::flush_impl(const char* s1, const char* s2,
                                  const char* s3) noexcept {
	if (flushed_)
		return;

	if (logger_.lock()) {
		if (label_) {
			try {
				fprintf(logger_.f_, s1, mysql_localdate().c_str(),
				        (int)buff_.size, buff_.data());
			} catch (const std::exception&) {
				fprintf(logger_.f_, s2, (int)buff_.size, buff_.data());
			}
		} else
			fprintf(logger_.f_, s3, (int)buff_.size, buff_.data());

		fflush(logger_.f_);
		logger_.unlock();
	}

	flushed_ = true;
	buff_ = "";
}
