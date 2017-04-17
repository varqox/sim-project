#pragma once


#include "http_request.h"
#include "http_response.h"

#include <sim/constants.h>
#include <sim/cpp_syntax_highlighter.h>
#include <sim/mysql.h>
#include <sim/sqlite.h>
#include <simlib/http/response.h>
#include <simlib/parsers.h>
#include <utime.h>

// Every object is independent, objects can be used in multi-thread program
// as long as one is not used by two threads simultaneously
class Sim final {
private:
	/* ============================== General ============================== */

	SQLite::Connection sqlite;
	MySQL::Connection mysql;
	CStringView client_ip; // TODO: put in request?
	server::HttpRequest request;
	server::HttpResponse resp;
	RequestURIParser url_args {""};
	CppSyntaxHighlighter cpp_syntax_highlighter;

	/**
	 * @brief Sets headers to make a redirection
	 * @details Does not clear response headers and contents
	 *
	 * @param location URL address where to redirect
	 */
	virtual void redirect(std::string location) {
		resp.status_code = "302 Moved Temporarily";
		resp.headers["Location"] = std::move(location);
	}

	/// Sets resp.status_code to @p status_code and resp.content to
	///  @p response_body
	void set_response(std::string status_code, std::string response_body = {}) {
		resp.status_code = std::move(status_code);
		resp.content = response_body;
	}

	// Notifies the Job server that there are jobs to do
	static void notify_job_server() noexcept {
		utime(JOB_SERVER_NOTIFYING_FILE, nullptr);
	}

	static std::string submission_status_as_td(SubmissionStatus status,
		bool show_final);

	static std::string submission_status_css_class(SubmissionStatus status,
		bool show_final);

	/* ============================== Session ============================== */

	bool session_is_open = false;
	uint8_t session_user_type = UTYPE_NORMAL;
	InplaceBuff<SESSION_ID_LEN + 1> session_id;
	InplaceBuff<SESSION_CSRF_TOKEN_LEN + 1> session_csrf_token;
	InplaceBuff<30> session_user_id;
	InplaceBuff<USERNAME_MAX_LEN + 1> session_username;
	InplaceBuff<4096> session_data;

	/**
	 * @brief Generates id of length @p length which consist of [a-zA-Z0-9]
	 *
	 * @param length the length of generated id
	 *
	 * @return generated id
	 */
	static std::string session_generate_id(uint length);

	/**
	 * @brief Creates session and opens it
	 * @details First closes old session and then creates and opens new one
	 *
	 * @param user_id Id of user to which session will belong
	 *
	 * @errors Throws an exception in any case of error
	 */
	void session_create_and_open(StringView user_id);

	/// Destroys session (removes from database, etc.)
	void session_destroy();

	/// Opens session, returns true if opened successfully, false otherwise
	bool session_open();

	/// Closes session (even if it throws, the session is closed)
	void session_close();

	/* =========================== Page template =========================== */

	bool page_template_began = false;
	InplaceBuff<1024> notifications;

	// Appends
	void page_template(StringView title, StringView styles = {},
		StringView scripts = {});

	void page_template_end();

	template<class... Args>
	void addNotification(StringView css_classes, Args&&... message) {
		notifications.append("<pre class=\"", css_classes, "\">",
			std::forward<Args>(message)..., "</pre>");
	}

	template<class... Args>
	Sim& append(Args&&... args) {
		back_insert(resp.content, std::forward<Args>(args)...);
		return *this;
	}

	void error_page_template(StringView status, StringView code,
		StringView message);

	// 403
	void error403() {
		error_page_template("403 Forbidden", "403",
			"Sorry, but you're not allowed to see anything here.");
	}

	// 404
	void error404() {
		error_page_template("404 Not Found", "404", "Page not found");
	}

	// 500
	void error500() {
		error_page_template("500 Internal Server Error", "500",
			"Internal Server Error");
	}

	// 501
	void error501() {
		error_page_template("501 Not Implemented", "501",
			"This feature has not been implemented yet.");
	}

	/* ============================== Forms ============================== */

	bool form_validation_error = false;

	/// Returns true if and only if the field @p name exists and its value is no
	/// longer than max_size
	template<class T>
	bool form_validate(T& var, const std::string& name,
		StringView name_to_print, size_t max_size = -1)
	{
		auto const& form = request.form_data.other;
		auto it = form.find(name);
		if (it == form.end()) {
			form_validation_error = true;
			addNotification("error", "Invalid ", htmlEscape(name_to_print));
			return false;

		} else if (it->second.size() > max_size) {
			form_validation_error = true;
			addNotification("error", htmlEscape(name_to_print),
				" cannot be longer than ", toStr(max_size), " bytes");
			return false;
		}

		var = it->second;
		return true;
	}

	/// Validates field and (if not blank) checks it by comp
	template<class T, class Checker>
	bool form_validate(T& var, const std::string& name,
		StringView name_to_print, Checker&& check, StringView error_msg,
		size_t max_size = -1)
	{
		if (form_validate(var, name, name_to_print, max_size)) {
			if (string_length(var) == 0 || check(var))
				return true;

			form_validation_error = true;
			if (error_msg.empty())
				addNotification("error",
					htmlEscape(concat(name_to_print, " validation error")));
			else
				addNotification("error", htmlEscape(error_msg));
		}

		return false;
	}

	/// Like validate() but also validate not blank
	template<class T>
	bool form_validate_not_blank(T& var, const std::string& name,
		StringView name_to_print, size_t max_size = -1)
	{
		auto const& form = request.form_data.other;
		auto it = form.find(name);
		if (it == form.end()) {
			form_validation_error = true;
			addNotification("error", "Invalid ", htmlEscape(name_to_print));
			return false;

		} else if (it->second.empty()) {
			form_validation_error = true;
			addNotification("error", htmlEscape(name_to_print),
				" cannot be blank");
			return false;

		} else if (it->second.size() > max_size) {
			form_validation_error = true;
			addNotification("error", htmlEscape(name_to_print),
				" cannot be longer than ", toStr(max_size), " bytes");
			return false;
		}

		var = it->second;
		return true;
	}

	/// Validates field and checks it by comp
	template<class T, class Checker>
	bool form_validate_not_blank(T& var, const std::string& name,
		StringView name_to_print, Checker&& check, StringView error_msg,
		size_t max_size = -1)
	{
		if (form_validate_not_blank(var, name, name_to_print, max_size)) {
			if (check(var))
				return true;

			form_validation_error = true;
			if (error_msg.empty())
				addNotification("error",
					htmlEscape(concat(name_to_print, " validation error")));
			else
				addNotification("error", htmlEscape(error_msg));
		}

		return false;
	}

	/// @brief Sets path of file of form name @p name to @p var or sets an error
	/// if such does not exist
	template<class T>
	bool form_validate_file_path_not_blank(T& var, const std::string& name,
		StringView name_to_print)
	{
		auto const& form = request.form_data.files;
		auto it = form.find(name);
		if (it == form.end()) {
			form_validation_error = true;
			addNotification("error", htmlEscape(name_to_print),
				" has to be submitted as a file</pre>");
			return false;
		}

		var = it->second;
		return true;
	}

	/* ============================== Users ============================== */

	enum class UserPermissions : uint {
		PERM_NONE = 0,
		PERM_VIEW = 1,
		PERM_EDIT = 2,
		PERM_CHANGE_PASS = 4,
		PERM_ADMIN_CHANGE_PASS = 8,
		PERM_DELETE = 16,
		PERM_ADMIN = 31,

		PERM_MAKE_ADMIN = 32,
		PERM_MAKE_TEACHER = 64,
		PERM_DEMOTE = 128
	};

	friend DECLARE_ENUM_UNARY_OPERATOR(UserPermissions, ~)
	friend DECLARE_ENUM_OPERATOR(UserPermissions, |)
	friend DECLARE_ENUM_OPERATOR(UserPermissions, &)

	uint8_t users_user_type = 0;
	UserPermissions users_permissions = UserPermissions::PERM_NONE;
	std::string users_user_id;
	std::string users_username;
	std::string users_first_name;
	std::string users_last_name;
	std::string users_email;

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
	static uint users_get_permissions(StringView viewer_id, uint viewer_type,
		StringView uid, uint utype);

	uint users_get_viewer_permissions(StringView uid, uint utype) {
		return users_get_permissions(session_user_id, session_user_type, uid,
			utype);
	}

	void users_page_template(StringView title, StringView styles = {},
		StringView scripts = {});

	void users_print_user() {
		append("<h4><a href=\"/u/", users_user_id, "\">",
				htmlEscape(users_username), "</a> (",
				htmlEscape(users_first_name), ' ', htmlEscape(users_last_name),
				")</h4>"
			"<div class=\"right-flow\" style=\"width:87%\">"
				"<a class=\"btn-small\" href=\"/u/", users_user_id, "\">"
					"View profile</a>");

		if (uint(users_permissions & UserPermissions::PERM_EDIT))
			append("<a class=\"btn-small blue\" href=\"/u/", users_user_id,
				"/edit\">Edit profile</a>");

		if (uint(users_permissions & UserPermissions::PERM_CHANGE_PASS))
			append("<a class=\"btn-small orange\" href=\"/u/", users_user_id,
				"/change-password\">Change password</a>");

		append("</div>");
	}

	/* Pages */

	// @brief Main User handler
	void users_handle();

	void login();

	void logout();

	void sign_up();

	void users_list_users();

	void users_change_password();

	void users_user_profile();

	void users_edit_profile();

	void users_delete_account();

	/**
	 * @brief Prints submissions table
	 *
	 * @param limit the maximum number of submissions in the table, 0 means
	 *   infinity (no limit)
	 */
	void users_print_user_submissions(uint limit = 0);

	void users_user_submissions();

	/* ============================== Contest ============================== */

	struct Round {
		std::string id, parent, problem_id, name, owner;
		std::string begins, ends, full_results;
		bool is_public, visible, show_ranking;
	};

	struct Subround {
		std::string id, name;
	};

	struct Problem {
		std::string id, parent, name;
	};

	enum RoundType : uint8_t {
		CONTEST = 0,
		ROUND = 1,
		PROBLEM = 2
	};

	class RoundPath {
	public:
		bool admin_access = false;
		RoundType type = CONTEST;
		std::string round_id;
		std::unique_ptr<Round> contest, round, problem;

		explicit RoundPath(std::string rid) : round_id(std::move(rid)) {}

		RoundPath(const RoundPath&) = delete;

		RoundPath(RoundPath&&) = default;

		RoundPath& operator=(const RoundPath&) = delete;

		RoundPath& operator=(RoundPath&&) = default;

		// void swap(RoundPath& rp) {
			// std::swap(admin_access, rp.admin_access);
			// std::swap(type, rp.type);
			// round_id.swap(rp.round_id);
			// contest.swap(rp.contest);
			// round.swap(rp.round);
			// problem.swap(rp.problem);
		// }

		~RoundPath() {}
	};

	std::unique_ptr<RoundPath> constest_rpath;

	// Utilities
	// contest_utilities.cc
	RoundPath* get_round_path(StringView round_id);

	void contest_page_template(StringView title, StringView styles = {},
		StringView scripts = {});

	void print_round_path(StringView page = "", bool force_normal = false);

	void print_round_view(bool link_to_problem_statement, bool admin_view = false);

	// Pages
	// contest.cc
	/// @brief Main contest handler
	void contest_handle();

	void contest_add_contest();

	void contest_add_round();

	void contest_add_problem();

	void contest_edit_contest();

	void contest_edit_round();

	void contest_edit_problem();

	void contest_delete_contest();

	void contest_delete_round();

	void contest_delete_problem();

	void contest_users();

	void contest_list_roblems(bool admin_view);

	void contest_ranking(bool admin_view);

	// submissions.cc
	void constest_submit(bool admin_view);

	void contest_delete_submission(StringView submission_id,
		StringView submission_owner);

	void contest_change_submission_type_to(StringView submission_id,
		StringView submission_owner, SubmissionType stype);

	void contest_submissions(bool admin_view);

	// files.cc
	void contest_add_file();

	void contest_edit_file(StringView id, std::string name);

	void contest_delete_file(StringView id, StringView name);

	void contest_file();

	void contest_files(bool admin_view);

	/* ============================= Job queue ============================= */

	enum class JobPermissions : uint {
		PERM_NONE = 0,
		PERM_VIEW = 1, // allowed to view the job
		PERM_VIEW_ALL = 2, // allowed to view all the jobs
		PERM_CANCEL = 4,
		PERM_RESTART = 8
	};

	friend DECLARE_ENUM_UNARY_OPERATOR(JobPermissions, ~)
	friend DECLARE_ENUM_OPERATOR(JobPermissions, |)
	friend DECLARE_ENUM_OPERATOR(JobPermissions, &)

	std::string jobs_job_id;
	JobPermissions jobs_perms = JobPermissions::PERM_NONE;

	JobPermissions jobs_get_permissions(StringView owner_id,
		JobQueueStatus job_status);

	void jobs_page_template(StringView title, StringView styles = {},
		StringView scripts = {})
	{
		page_template(title, concat("body{margin-left:32px}", styles), scripts);
	}

	/// Main Jobs handler
	void jobs_handle();

	void jobs_job();

	void jobs_download_report(std::string data_preview);

	void jobs_download_uploaded_package();

	void jobs_cancel_job();

	void jobs_restart_job(JobQueueType job_type, StringView job_info);

	/* ============================= Problemset ============================= */

	enum class ProblemsetPermissions : uint {
		PERM_NONE = 0,
		PERM_VIEW = 1,
		PERM_VIEW_ALL = 2,
		PERM_VIEW_SOLUTIONS = 4,
		PERM_DOWNLOAD = 8,
		PERM_SEE_SIMFILE = 16,
		PERM_SEE_OWNER = 32,
		PERM_ADMIN = 64,
		PERM_ADD = 128
	};

	friend DECLARE_ENUM_UNARY_OPERATOR(ProblemsetPermissions, ~)
	friend DECLARE_ENUM_OPERATOR(ProblemsetPermissions, |)
	friend DECLARE_ENUM_OPERATOR(ProblemsetPermissions, &)

	ProblemsetPermissions problemset_get_permissions(StringView owner_id,
		ProblemType ptype);

	ProblemsetPermissions problemset_perms = ProblemsetPermissions::PERM_NONE;
	uint problemset_owner_utype = UTYPE_NORMAL;
	std::string problemset_owner_username;
	std::string problemset_problem_id;
	std::string problemset_problem_name;
	std::string problemset_problem_label;
	std::string problemset_problem_owner;
	std::string problemset_problem_added;
	std::string problemset_problem_simfile;
	ProblemType problemset_problem_type = ProblemType::VOID;

	void problemset_page_template(StringView title, StringView styles = {},
		StringView scripts = {});

	/// Main Problemset handler
	void problemset_handle();

	/// Warning: this function assumes that the user is allowed to view the
	/// statement
	void problemset_view_statement(StringView problem_id);

	void problemset_add_problem();

	void problemset_view_problem();

	void problemset_edit_problem();

	void problemset_download_problem();

	void problemset_reupload_problem();

	void problemset_rejudge_problem_submissions();

	void problemset_delete_problem();

	void problemset_problem_solutions();

	void problemset_delete_submission(StringView submission_id,
		StringView submission_owner);

	void problemset_change_submission_type_to(StringView submission_id,
		StringView submission_owner, SubmissionType stype);

	void problemset_problem_submissions();

	/* ================================ API ================================ */

	void api_handle();

	void api_logs();

	void api_list_problems();

	void api_change_submission_type_to();

	void api_list_submissions();

	void api_list_jobs();

	/* =============================== Other =============================== */

	// Pages
	void main_page();

	void static_file();

	void view_logs();

	void view_submission();

public:
	Sim() : sqlite(SQLITE_DB_FILE, SQLITE_OPEN_READONLY),
		mysql(MySQL::makeConnWithCredFile(".db.config")) {}

	~Sim() {}

	Sim(const Sim&) = delete;
	Sim(Sim&&) = delete;
	Sim& operator=(const Sim&) = delete;
	Sim& operator=(Sim&&) = delete;

	/**
	 * @brief Handles request
	 * @details Takes requests, handle it and returns response.
	 *   This function is not thread-safe
	 *
	 * @param client_ip IP address of the client
	 * @param req request
	 *
	 * @return response
	 */
	server::HttpResponse handle(CStringView _client_ip,
		server::HttpRequest req); // TODO: close session
};
