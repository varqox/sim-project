#include "../include/debug.h"
#include "../include/logger.h"
#include "../include/time.h"

using std::string;

Logger stdlog(stderr), errlog(stderr);

Logger::Logger(CStringView filename)
	: f_(fopen(filename.c_str(), "a")), opened_(true)
{
	if (f_ == nullptr)
		THROW("fopen('", filename, "') failed", error(errno));
}

void Logger::open(CStringView filename) {
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
		if (logger_.label()) {
			try {
				fprintf(logger_.f_, "[ %s ] %.*s\n", localdate().c_str(),
					(int)buff_.size, buff_.data());
			} catch (const std::exception&) {
				fprintf(logger_.f_, "[ unknown time ] %.*s\n", (int)buff_.size,
					buff_.data());
			}
		} else
			fprintf(logger_.f_, "%.*s\n", (int)buff_.size, buff_.data());

		fflush(logger_.f_);
		logger_.unlock();
	}

	flushed_ = true;
	buff_ = "";
}
