#pragma once

#include "concat_tostr.hh"
#include "file_path.hh"
#include "inplace_buff.hh"
#include "utilities.hh"

#include <atomic>

class Logger {
private:
	FILE* f_;
	std::atomic<bool> opened_ {false}, label_ {true};

	void close() noexcept {
		if (opened_.load()) {
			opened_.store(false);
			fclose(f_);
		}
	}

	// Lock the file
	bool lock() noexcept {
		if (f_ == nullptr)
			return false;

		flockfile(f_);
		return true;
	}

	// Unlock the file
	void unlock() noexcept { funlockfile(f_); }

public:
	// Like open()
	explicit Logger(FilePath filename);

	// Like use(), it accept nullptr for which a dummy logger is created
	explicit Logger(FILE* stream) noexcept : f_(stream) {}

	Logger(const Logger&) = delete;
	Logger& operator=(const Logger&) = delete;

	/**
	 * @brief Opens file @p filename in append mode as log file, if fopen()
	 *   error occurs exception is thrown and f_ (inner stream) is unchanged
	 *
	 * @param filename file to open
	 *
	 * @errors Throws an exception std::runtime_error if an fopen() error
	 *   occurs
	 */
	void open(FilePath filename);

	/// Sets @p stream as log stream, nullptr is acceptable for the logger
	/// becomes a dummy
	void use(FILE* stream) noexcept {
		close();
		f_ = stream;
	}

	/// Sets @p stream as log stream and returns current log stream
	FILE* exchange_log_stream(FILE* stream) noexcept {
		return std::exchange(f_, stream);
	}

	/// Returns file descriptor which is used internally by Logger (to log to
	/// it)
	int fileno() const noexcept { return ::fileno(f_); }

	bool label() const noexcept {
		return label_.load(std::memory_order_relaxed);
	}

	bool label(bool add_label) noexcept { return label_.exchange(add_label); }

	class Appender {
	private:
		friend class Logger;

		Logger& logger_;
		bool flushed_ = true;
		// *label_ are useful with flush_no_nl() to avoid adding label in the
		// middle of the line
		bool orig_label_;
		bool label_;
		InplaceBuff<8192> buff_;

		explicit Appender(Logger& logger) : logger_(logger) {}

		template <class... Args,
		          std::enable_if_t<(is_string_argument<Args> and ...), int> = 0>
		explicit Appender(Logger& logger, Args&&... args)
		   : logger_(logger), orig_label_(logger.label()), label_(orig_label_) {
			operator()(std::forward<Args>(args)...);
		}

		/// Deeply integrated with flush() and flush_no_nl()
		void flush_impl(const char* format1, const char* format2,
		                const char* format3) noexcept;

	public:
		Appender(const Appender&) = delete;

		Appender(Appender&& app)
		   : logger_(app.logger_), flushed_(app.flushed_),
		     orig_label_(app.orig_label_), label_(app.label_),
		     buff_(std::move(app.buff_)) {
			app.flushed_ = true;
		}

		Appender& operator=(const Appender&) = delete;
		Appender& operator=(Appender&&) = delete;

		template <class T>
		Appender& operator<<(T&& x) {
			return operator()(std::forward<T>(x));
		}

		template <class... Args,
		          std::enable_if_t<(is_string_argument<Args> and ...), int> = 0>
		Appender& operator()(Args&&... args) {
			buff_.append(std::forward<Args>(args)...);
			flushed_ = false;
			return *this;
		}

		void flush() noexcept {
			flush_impl("[ %s ] %.*s\n", "[ unknown time ] %.*s\n", "%.*s\n");
			label_ = orig_label_;
		}

		/// Like flush() but does not append the '\n' to the log
		void flush_no_nl() noexcept {
			flush_impl("[ %s ] %.*s", "[ unknown time ] %.*s", "%.*s");
			label_ = false;
		}

		~Appender() { flush(); }
	};

	template <class T>
	Appender operator<<(T&& x) noexcept {
		return Appender(*this, std::forward<T>(x));
	}

	template <class... Args,
	          std::enable_if_t<(is_string_argument<Args> and ...), int> = 0>
	Appender operator()(Args&&... args) noexcept {
		return Appender(*this, std::forward<Args>(args)...);
	}

	Appender get_appender() noexcept { return Appender(*this); }

	~Logger() {
		if (opened_)
			fclose(f_);
	}
};

// By default both write to stderr
inline Logger stdlog(stderr); // Standard (default) log
inline Logger errlog(stderr); // Error log

// Logs to string and Logger simultaneously
template <class StrType>
class DoubleAppender {
	Logger::Appender app_;
	StrType& str_;

public:
	template <class... Args,
	          std::enable_if_t<(is_string_argument<Args> and ...), int> = 0>
	DoubleAppender(Logger& logger, StrType& str, Args&&... args)
	   : app_(logger(args...)), str_(str) {
		back_insert(str_, std::forward<Args>(args)...);
	}

	DoubleAppender(const DoubleAppender&) = delete;

	DoubleAppender(DoubleAppender&& dl)
	   : app_(std::move(dl.app_)), str_(dl.str_) {}

	DoubleAppender& operator=(const DoubleAppender&) = delete;
	DoubleAppender& operator=(DoubleAppender&&) = delete;

	template <class T>
	DoubleAppender& operator<<(T&& x) {
		return operator()(std::forward<T>(x));
	}

	template <class... Args,
	          std::enable_if_t<(is_string_argument<Args> and ...), int> = 0>
	DoubleAppender& operator()(Args&&... args) {
		back_insert(str_, args...);
		app_(std::forward<Args>(args)...);
		return *this;
	}

	void flush_no_nl() noexcept { app_.flush_no_nl(); }

	void flush() {
		back_insert(str_, '\n');
		app_.flush();
	}

	~DoubleAppender() {
		// Flush inline because app_ will flush itself during destruction
		try {
			back_insert(str_, '\n');
		} catch (...) {
			// Nothing we can do
		}
	}
};
