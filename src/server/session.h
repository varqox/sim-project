#pragma once

#include "sim_base.h"

#include <sim/constants.h>

class Session : virtual protected SimBase {
private:
	bool is_open = false;

protected:
	uint8_t user_type = UTYPE_NORMAL;
	std::string sid; // session id
	std::string user_id, username, data;

	Session() = default;

	Session(const Session&) = delete;
	Session(Session&&) = delete;
	Session& operator=(const Session&) = delete;
	Session& operator=(Session&&) = delete;

	virtual ~Session() { close(); }

	/**
	 * @brief Generates id of length @p length which consist of [a-zA-Z0-9]
	 *
	 * @param length the length of generated id
	 *
	 * @return generated id
	 */
	static std::string generateId(uint length);

	/**
	 * @brief Creates session and opens it
	 * @details First closes old session and then creates and opens new one
	 *
	 * @param _user_id Id of user to which session will belong
	 *
	 * @errors Throws an exception in any case of error
	 */
	void createAndOpen(const std::string& _user_id) noexcept(false);

	/// Destroys session (removes from database, etc.)
	void destroy();

	/// Opens session, returns true if opened successfully, false otherwise
	bool open();

	bool isOpen() const noexcept { return is_open; }

	/// Closes session
	void close();
};
