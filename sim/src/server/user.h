#pragma once

#include "errors.h"
#include "template.h"

class User : virtual protected Template, virtual protected Errors {
protected:
	User() = default;

	User(const User&) = delete;
	User(User&&) = delete;
	User& operator=(const User&) = delete;
	User& operator=(User&&) = delete;

	virtual ~User() = default;

private:
	uint8_t user_type = 0, permissions = 0;
	std::string user_id, username, first_name, last_name, email;

	static constexpr uint PERM_VIEW = 1;
	static constexpr uint PERM_EDIT = 2;
	static constexpr uint PERM_CHANGE_PASS = 4;
	static constexpr uint PERM_ADMIN_CHANGE_PASS = 8;
	static constexpr uint PERM_DELETE = 16;
	static constexpr uint PERM_ADMIN = 31;

	static constexpr uint PERM_MAKE_ADMIN = 32;
	static constexpr uint PERM_MAKE_TEACHER = 64;
	static constexpr uint PERM_DEMOTE = 128;

	/**
	 * @brief Returns a set of operation the viewer is allowed to do over the
	 *   user
	 *
	 * @param viewer_id uid of viewer
	 * @param viewer_type user_type of viewer
	 * @param uid uid of (viewed) user
	 * @param utype type of (viewed) user
	 * @return ORed permissions flags
	 */
	static uint getPermissions(const std::string& viewer_id, uint viewer_type,
		const std::string& uid, uint utype);

	uint getPermissions(const std::string& uid, uint utype) {
		return getPermissions(Session::user_id, Session::user_type, uid, utype);
	}

	TemplateEnder userTemplate(const StringView& title,
		const StringView& styles = {}, const StringView& scripts = {});

	void printUser() {
		append("<h4><a href=\"/u/", user_id, "\">", username, "</a> (",
			first_name, ' ', last_name, ")</h4>\n");
	}

	// Pages
protected:
	// @brief Main User handler
	void handle();

	void login();

	void logout();

	void signUp();

private:
	void listUsers();

	void changePassword();

	void editProfile();

	void deleteAccount();
};
