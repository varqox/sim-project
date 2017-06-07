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
	void redirect(std::string location) {
		STACK_UNWINDING_MARK;

		resp.status_code = "302 Moved Temporarily";
		resp.headers["Location"] = std::move(location);
	}

	/// Sets resp.status_code to @p status_code and resp.content to
	///  @p response_body
	void set_response(StringView status_code, StringView response_body = {}) {
		STACK_UNWINDING_MARK;

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


	/* ================================ API ================================ */

	void api_error400(StringView response_body = {}) {
		set_response("400 Bad Request", response_body);
	}

	void api_error403(StringView response_body = {}) {
		set_response("403 Forbidden", response_body);
	}

	void api_error404(StringView response_body = {}) {
		set_response("404 Not Found", response_body);
	}

	// api.cc
	void api_handle();

	void api_logs();

	// jobs_api.cc
	void api_jobs();

	void api_job();

	void api_job_restart(JobType job_type, StringView job_info);

	void api_job_cancel();

	void api_job_download_report();

	void api_job_download_uploaded_package();

	// users_api.cc
	void api_users();

	void api_user();

	void api_user_add();

	void api_user_edit();

	void api_user_delete();

	void api_user_change_password();

	// submissions_api.cc
	void api_submissions();

	void api_submission();

	void api_submission_rejudge();

	void api_submission_change_type();

	void api_submission_delete();

	void api_submission_source();

	void api_submission_download();

	/* ============================== Session ============================== */

	bool session_is_open = false;
	UserType session_user_type = UserType::NORMAL;
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
	 * @param temporary_session Whether create temporary or persistent session
	 *
	 * @errors Throws an exception in any case of error
	 */
	void session_create_and_open(StringView user_id, bool temporary_session);

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
		resp.content.append(std::forward<Args>(args)...);
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
		STACK_UNWINDING_MARK;

		auto const& form = request.form_data.other;
		auto it = form.find(name);
		if (it == form.end()) {
			form_validation_error = true;
			addNotification("error", "Invalid ", htmlEscape(name_to_print));
			return false;

		} else if (it->second.size() > max_size) {
			form_validation_error = true;
			addNotification("error", htmlEscape(name_to_print),
				" cannot be longer than ", max_size, " bytes");
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
		STACK_UNWINDING_MARK;

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
		STACK_UNWINDING_MARK;

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
				" cannot be longer than ", max_size, " bytes");
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
		STACK_UNWINDING_MARK;

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
		STACK_UNWINDING_MARK;

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
		NONE = 0,
		VIEW = 1,
		EDIT = 2,
		CHANGE_PASS = 4,
		ADMIN_CHANGE_PASS = 8,
		DELETE = 16,

		VIEW_ALL = 32,
		MAKE_ADMIN = 64,
		MAKE_TEACHER = 128,
		MAKE_NORMAL = 256,
		ADD_USER = 512
	};

	friend DECLARE_ENUM_UNARY_OPERATOR(UserPermissions, ~)
	friend DECLARE_ENUM_OPERATOR(UserPermissions, |)
	friend DECLARE_ENUM_OPERATOR(UserPermissions, &)

	UserType users_user_type = UserType::NORMAL;
	UserPermissions users_perms = UserPermissions::NONE;
	InplaceBuff<30> users_user_id;
	InplaceBuff<USERNAME_MAX_LEN> users_username;
	InplaceBuff<USER_FIRST_NAME_MAX_LEN> users_first_name;
	InplaceBuff<USER_LAST_NAME_MAX_LEN> users_last_name;
	InplaceBuff<USER_EMAIL_MAX_LEN> users_email;

	/**
	 * @brief Returns a set of operation the viewer is allowed to do over the
	 *   user
	 *
	 * @param uid uid of (viewed) user
	 * @param utype type of (viewed) user
	 *
	 * @return ORed permissions flags
	 */
	Sim::UserPermissions users_get_permissions(StringView uid, UserType utype);

	Sim::UserPermissions users_get_permissions() {
		// Return overall permissions
		return users_get_permissions("", UserType::NORMAL);
	}

	void users_page_template(StringView title, StringView styles = {},
		StringView scripts = {});

	void users_print_user() {
		STACK_UNWINDING_MARK;

		append("<h4><a href=\"/u/", users_user_id, "\">",
				htmlEscape(users_username), "</a> (",
				htmlEscape(users_first_name), ' ', htmlEscape(users_last_name),
				")</h4>"
			"<div class=\"right-flow\" style=\"width:87%\">"
				"<a class=\"btn-small\" href=\"/u/", users_user_id, "\">"
					"View profile</a>");

		if (uint(users_perms & UserPermissions::EDIT))
			append("<a class=\"btn-small blue\" href=\"/u/", users_user_id,
				"/edit\">Edit profile</a>");

		if (uint(users_perms & UserPermissions::CHANGE_PASS))
			append("<a class=\"btn-small orange\" href=\"/u/", users_user_id,
				"/change-password\">Change password</a>");

		append("</div>");
	}

	/* Pages */

	void login();

	void logout();

	void sign_up();

	// @brief Main User handler
	void users_handle();

	void users_add();

	void users_user();

	/* ============================= Job queue ============================= */

	enum class JobPermissions : uint {
		NONE = 0,
		VIEW = 1, // allowed to view the job
		DOWNLOAD_REPORT = 2,
		DOWNLOAD_UPLOADED_PACKAGE = 4,
		CANCEL = 8,
		RESTART = 16,
		VIEW_ALL = 32 // allowed to view all the jobs
	};

	friend DECLARE_ENUM_UNARY_OPERATOR(JobPermissions, ~)
	friend DECLARE_ENUM_OPERATOR(JobPermissions, |)
	friend DECLARE_ENUM_OPERATOR(JobPermissions, &)

	InplaceBuff<30> jobs_job_id;
	JobPermissions jobs_perms = JobPermissions::NONE;

	// Session must be open to access the jobs
	JobPermissions jobs_get_permissions(StringView creator_id, JobType job_type,
		JobStatus job_status);

	JobPermissions jobs_get_permissions() {
		// Return overall permissions
		return jobs_get_permissions("", JobType::VOID, JobStatus::DONE);
	}

	void jobs_page_template(StringView title, StringView styles = {},
		StringView scripts = {})
	{
		STACK_UNWINDING_MARK;
		page_template(title, concat("body{padding-left:32px}", styles), scripts);
	}

	/// Main Jobs handler
	void jobs_handle();

	/* ============================== Problems ============================== */

	enum class ProblemPermissions : uint {
		NONE = 0,
		VIEW = 1,
		VIEW_ALL = 2,
		VIEW_SOLUTIONS = 4,
		DOWNLOAD = 8,
		SEE_SIMFILE = 16,
		SEE_OWNER = 32,
		ADMIN = 64,
		ADD = 128
	};

	friend DECLARE_ENUM_UNARY_OPERATOR(ProblemPermissions, ~)
	friend DECLARE_ENUM_OPERATOR(ProblemPermissions, |)
	friend DECLARE_ENUM_OPERATOR(ProblemPermissions, &)

	ProblemPermissions problems_get_permissions(StringView owner_id,
		ProblemType ptype);

	ProblemPermissions problems_get_permissions() {
		// Return overall permissions
		return problems_get_permissions("", ProblemType::VOID);
	}

	ProblemPermissions problems_perms = ProblemPermissions::NONE;
	UserType problems_owner_utype = UserType::NORMAL;
	std::string problems_owner_username;
	std::string problems_problem_id;
	std::string problems_problem_name;
	std::string problems_problem_label;
	std::string problems_problem_owner;
	std::string problems_problem_added;
	std::string problems_problem_simfile;
	ProblemType problems_problem_type = ProblemType::VOID;

	void problems_page_template(StringView title, StringView styles = {},
		StringView scripts = {});

	/// Main Problemset handler
	void problems_handle();

	/// WARNING: this function assumes that the user is allowed to view the
	/// statement
	void problems_view_statement(StringView problem_id);

	void problems_add_problem();

	void problems_view_problem();

	void problems_edit_problem();

	void problems_download_problem();

	void problems_reupload_problem();

	void problems_rejudge_problem_submissions();

	void problems_delete_problem();

	void problems_problem_solutions();

	void problems_delete_submission(StringView submission_id,
		StringView submission_owner);

	void problems_change_submission_type_to(StringView submission_id,
		StringView submission_owner, SubmissionType stype);

	void problems_problem_submissions();

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

	/* ============================= Submission ============================= */

	enum class SubmissionPermissions : uint {
		NONE = 0,
		VIEW = 1,
		VIEW_SOURCE = 2,
		VIEW_FULL_REPORT = 4,
		CHANGE_TYPE = 8,
		REJUDGE = 16,
		DELETE = 32
	};

	friend DECLARE_ENUM_UNARY_OPERATOR(SubmissionPermissions, ~)
	friend DECLARE_ENUM_OPERATOR(SubmissionPermissions, |)
	friend DECLARE_ENUM_OPERATOR(SubmissionPermissions, &)

	SubmissionPermissions submission_get_permissions(
		StringView submission_owner, StringView contest_owner,
		ContestUserMode cu_mode)
	{
		using PERM = SubmissionPermissions;
		using CUM = ContestUserMode;

		D(throw_assert(session_is_open);) // Session must be open to gain access

		if (session_user_type == UserType::ADMIN or
			session_user_id == contest_owner or cu_mode == CUM::MODERATOR)
		{
			return PERM::VIEW | PERM::VIEW_SOURCE | PERM::VIEW_FULL_REPORT |
				PERM::CHANGE_TYPE | PERM::REJUDGE | PERM::DELETE;
		}

		if (session_user_id == submission_owner or cu_mode == CUM::CONTESTANT)
			return PERM::VIEW | PERM::VIEW_SOURCE;

		return PERM::NONE;
	}

	StringView submission_id;
	SubmissionPermissions submission_perms = SubmissionPermissions::NONE;

	// Pages
	void submission_handle();

	/* =============================== Other =============================== */

	// Pages
	void main_page();

	void static_file();

	void view_logs();

public:
	Sim() : sqlite(SQLITE_DB_FILE, SQLITE_OPEN_READONLY | SQLITE_OPEN_NOMUTEX),
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
