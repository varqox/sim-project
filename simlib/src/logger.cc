#include "../include/debug.h"
#include "../include/logger.h"
#include "../include/time.h"

using std::string;

Logger stdlog(stderr), errlog(stderr);

Logger::Logger(const CStringView& filename)
	: f_(fopen(filename.c_str(), "a")), opened_(true)
{
	if (f_ == nullptr)
		THROW("fopen('", filename, "') failed", error(errno));
}

void Logger::open(const CStringView& filename) {
	FILE *f = fopen(filename.c_str(), "a");
	if (f == nullptr)
		THROW("fopen('", filename, "') failed", error(errno));

	close();
	f_ = f;
}

void Logger::Appender::flush() noexcept {
	if (flushed_)
		return;

	if (logger_.lock()) {
		if (logger_.label())
			fprintf(logger_.f_, "[ %s ] %s\n",
				localdate("%Y-%m-%d %H:%M:%S").c_str(), buff_.c_str());
		else
			fprintf(logger_.f_, "%s\n", buff_.c_str());

		fflush(logger_.f_);
		logger_.unlock();
	}

	flushed_ = true;
	buff_.clear();
}
