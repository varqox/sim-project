#include "../include/logger.h"
#include "../include/time.h"

using std::string;

Logger stdlog(stderr), error_log(stderr);

Logger::Logger(const string& filename) : f_(fopen(filename.c_str(), "a")),
		opened_(true) {
	if (f_ == nullptr)
		throw std::runtime_error(concat("fopen() failed", error(errno)));
}

void Logger::open(const string& filename) {
	FILE *f = fopen(filename.c_str(), "a");
	if (f == nullptr)
		throw std::runtime_error(concat("fopen() failed", error(errno)));

	close();
	f_ = f;
}

void Logger::Appender::flush() noexcept {
	if (flushed_)
		return;

	if (logger_.lock()) {
		fprintf(logger_.f_, "[ %s ] %s\n", date("%Y-%m-%d %H:%M:%S").c_str(),
			buff_.c_str());
		fflush(logger_.f_);
		logger_.unlock();
	}

	flushed_ = true;
	buff_.clear();
}
