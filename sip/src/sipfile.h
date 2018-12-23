#pragma once

#include <simlib/config_file.h>

/**
 * @brief Sipfile holds sip package configuration that does not fit into Simfile
 */
class Sipfile {
public:
	uint64_t default_time_limit; // in microseconds

private:
	ConfigFile config;

public:
	Sipfile() = default;

	/**
	 * @brief Loads needed variables from @p sipfile_contents
	 * @details Uses ConfigFile::loadConfigFromString()
	 *
	 * @param sipfile_contents Sipfile file contents
	 *
	 * @errors May throw from ConfigFile::loadConfigFromString()
	 */
	Sipfile(std::string sipfile_contents) {
		config.addVars("default_time_limit", "static", "gen");
		config.loadConfigFromString(std::move(sipfile_contents));
	}

	Sipfile(const Sipfile&) = default;
	Sipfile(Sipfile&&) noexcept = default;
	Sipfile& operator=(const Sipfile&) = default;
	Sipfile& operator=(Sipfile&&) = default;

	~Sipfile() = default;

	const ConfigFile& configFile() const { return config; }

	/**
	 * @brief Loads the default time limit
	 * @details Fields:
	 *   - default_time_limit
	 *
	 * @errors Throws an exception of type std::runtime_error if any
	 *   validation error occurs
	 */
	void loadDefaultTimeLimit();

// TODO: not needed?
//	/**
//	 * @brief Dumps object to string
//	 *
//	 * @return dumped config (which can be placed in a file)
//	 */
//	std::string dump() const;
};
