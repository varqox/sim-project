#pragma once

#include "../filesystem.h"
#include "simfile.h"

namespace sim {

// Helps with converting a package to a Sim package
class Conver {
	TemporaryDirectory tmp_dir_;
	std::string package_path_;
	bool verbose_ = false;

public:
	bool getVerbosity() const { return verbose_; }

	void setVerbosity(bool verbosity) { verbose_ = verbosity; }

	/**
	 * @brief Extracts package from @p package_path to internal temporary
	 *   directory. Drops one level deeper if package holds exactly one
	 *   directory.
	 * @details Allowed formats of the package (if not a directory - directory
	 *   is also allowed):
	 *     EXTENSION -> archive type
	 *     .zip -> zip
	 *     .tgz -> Tar Gzip
	 *     .tar.gz -> Tar Gzip
	 *
	 * @param package_path path of the package
	 *
	 * @errors If any error is encountered then an std::runtime_error is thrown
	 *   with a proper message
	 */
	void extractPackage(const std::string& package_path);

	struct Options {
		std::string name; // Leave empty to detect it from the Simfile in the
		                  // package
		std::string shortname; // Leave empty to detect it from the Simfile in
		                  // the package
		uint64_t memory_limit = 0; // [KB] Set to a non-zero value to overwrite
		                           // memory limit of every test with this value
		uint64_t global_time_limit = 0; // [microseconds] If non-zero then every
		                                // test will get this as time limit
		uint64_t max_time_limit = 60e6; // [microseconds] Maximum allowed time
		                                // limit on the test. If
		                                // global_time_limit != 0 this option is
		                                // ignored
		bool ignore_simfile = false; // If true, Simfile in the package will be
		                             // ignored
		bool force_auto_time_limits_setting =
			false; // If global_time_limit != 0 this option is ignored
		uint64_t compilation_time_limit = 30e6; // [microseconds] for checker
		                                        // and solution compilation
		uint64_t compilation_errors_max_length = 32 << 10; // [bytes]
		std::string proot_path = "proot"; // Path to PRoot (passed to
		                                  // Simfile::compile* methods)

		Options() = default;
	};

	/**
	 * @brief Constructs Simfile based on extracted package and options @p opts
	 * @details Constructs full and valid Simfile. See description to each filed
	 *   in Option to learn more.
	 *
	 * @param opts options that parameterize Conver behavior
	 *
	 * @return A valid Simfile
	 *
	 * @errors If any error is encountered then an exception of type
	 *   std::runtime_error is thrown with a proper message
	 */
	Simfile constructFullSimfile(const Options& opts);

	/// Returns path to unpacked package (with trailing '/')
	const std::string& unpackedPackagePath() const { return package_path_; }
};

} // namespace sim
