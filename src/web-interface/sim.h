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

	#ifdef DEBUG
		if (response_body.find(notifications) == StringView::npos)
			THROW("Omitted notifications in the error message: ", response_body);
	#endif

		resp.status_code = std::move(status_code);
		resp.content = response_body;
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

	void api_job_download_log();

	void api_job_download_uploaded_package();

	// jobs_api.cc
	void api_problems();

	void api_problem();

	void api_problem_add_or_reupload_impl(bool reuploading);

	void api_problem_add();

 	void api_statement_impl(StringView problem_id, StringView problem_label,
 		StringView simfile);

	void api_problem_statement(StringView problem_label, StringView simfile);

	void api_problem_download(StringView problem_label);

	void api_problem_rejudge_all_submissions();

	void api_problem_reupload();

	// void api_problem_edit();

	// void api_problem_delete();

	// users_api.cc
	void api_users();

	void api_user();

	void api_user_add();

	void api_user_edit();

	void api_user_delete();

	void api_user_change_password();

	// submissions_api.cc
	void append_submission_status(SubmissionStatus status,
		bool show_full_results);

	void api_submissions();

	void api_submission();

	void api_submission_add();

	void api_submission_rejudge();

	void api_submission_change_type();

	void api_submission_delete();

	void api_submission_source();

	void api_submission_download();

	// contests_api.cc
	void append_contest_actions_str();

	void api_contests();

	void api_contest();

	void api_contest_round();

	void api_contest_problem();

	void api_contest_add();

	void api_contest_edit(bool is_public);

	void api_contest_delete();

	void api_contest_round_add();

	void api_contest_round_edit();

	void api_contest_round_delete();

	void api_contest_problem_statement(StringView problem_id);

	void api_contest_problem_add();

	void api_contest_problem_edit();

	void api_contest_problem_delete();

	void api_contest_ranking(StringView submissions_id_name,
		StringView query_id);

	void api_contest_users();

	void api_contest_user_add();

	void api_contest_user_edit();

	void api_contest_user_expel();

	void api_contest_files();

	void api_contest_file_add();

	void api_contest_file_edit();

	void api_contest_file_delete();

	/* ============================== Session ============================== */

	bool session_is_open = false;
	UserType session_user_type = UserType::NORMAL;
	InplaceBuff<SESSION_ID_LEN + 1> session_id;
	InplaceBuff<SESSION_CSRF_TOKEN_LEN + 1> session_csrf_token;
	InplaceBuff<32> session_user_id;
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
	void add_notification(StringView css_classes, Args&&... message) {
		notifications.append("<pre class=\"", css_classes, "\">",
			std::forward<Args>(message)..., "</pre>\n");
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
		if (not it) {
			form_validation_error = true;
			add_notification("error", "Invalid ", htmlEscape(name_to_print));
			return false;

		} else if (it->second.size() > max_size) {
			form_validation_error = true;
			add_notification("error", htmlEscape(name_to_print),
				" cannot be longer than ", max_size, " bytes");
			return false;
		}

		var = it->second;
		return true;
	}

	/// Validates field and (if not blank) checks it by comp
	template<class T, class Checker,
		typename = std::enable_if_t<!std::is_convertible<Checker, size_t>::value>>
	bool form_validate(T& var, const std::string& name,
		StringView name_to_print, Checker&& check, StringView error_msg = {},
		size_t max_size = -1)
	{
		STACK_UNWINDING_MARK;

		if (form_validate(var, name, name_to_print, max_size)) {
			if (string_length(var) == 0 || check(var))
				return true;

			form_validation_error = true;
			if (error_msg.empty())
				add_notification("error",
					htmlEscape(concat(name_to_print, " validation error")));
			else
				add_notification("error", htmlEscape(error_msg));
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
		if (not it) {
			form_validation_error = true;
			add_notification("error", "Invalid ", htmlEscape(name_to_print));
			return false;

		} else if (it->second.empty()) {
			form_validation_error = true;
			add_notification("error", htmlEscape(name_to_print),
				" cannot be blank");
			return false;

		} else if (it->second.size() > max_size) {
			form_validation_error = true;
			add_notification("error", htmlEscape(name_to_print),
				" cannot be longer than ", max_size, " bytes");
			return false;
		}

		var = it->second;
		return true;
	}

	/// Validates field and checks it by comp
	template<class T, class Checker,
		typename = std::enable_if_t<!std::is_convertible<Checker, size_t>::value>>
	bool form_validate_not_blank(T& var, const std::string& name,
		StringView name_to_print, Checker&& check, StringView error_msg = {},
		size_t max_size = -1)
	{
		STACK_UNWINDING_MARK;

		if (form_validate_not_blank(var, name, name_to_print, max_size)) {
			if (check(var))
				return true;

			form_validation_error = true;
			if (error_msg.empty())
				add_notification("error",
					htmlEscape(concat(name_to_print, ": invalid value")));
			else
				add_notification("error", htmlEscape(error_msg));
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
		if (not it) {
			form_validation_error = true;
			add_notification("error", htmlEscape(name_to_print),
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

	UserPermissions users_perms = UserPermissions::NONE;
	InplaceBuff<32> users_uid;

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

	/* Pages */

	void login();

	void logout();

	void sign_up();

	// @brief Main User handler
	void users_handle();

	void users_user();

	/* ============================= Job queue ============================= */

	enum class JobPermissions : uint {
		NONE = 0,
		VIEW = 1, // allowed to view the job
		DOWNLOAD_LOG = 2,
		DOWNLOAD_UPLOADED_PACKAGE = 4,
		CANCEL = 8,
		RESTART = 16,
		VIEW_ALL = 32 // allowed to view all the jobs
	};

	friend DECLARE_ENUM_UNARY_OPERATOR(JobPermissions, ~)
	friend DECLARE_ENUM_OPERATOR(JobPermissions, |)
	friend DECLARE_ENUM_OPERATOR(JobPermissions, &)
	friend DECLARE_ENUM_ASSIGN_OPERATOR(JobPermissions, |=)

	InplaceBuff<32> jobs_jid;
	JobPermissions jobs_perms = JobPermissions::NONE;

	// Session must be open to access the jobs
	JobPermissions jobs_get_permissions(StringView creator_id, JobType job_type,
		JobStatus job_status);

	// Used to get granted permissions to the problem jobs
	JobPermissions jobs_granted_permissions_problem(StringView problem_id);

	// Used to get granted permissions to the submission jobs
	JobPermissions jobs_granted_permissions_submission(
		StringView submission_id);

	JobPermissions jobs_get_permissions() {
		// Return overall permissions
		return jobs_get_permissions("", JobType::VOID, JobStatus::DONE);
	}

	/// Main Jobs handler
	void jobs_handle();

	/* ============================== Problems ============================== */

	enum class ProblemPermissions : uint {
		NONE = 0,
		// Both
		ADMIN = 1,
		// Overall
		ADD = 2,
		VIEW_TPUBLIC = 4,
		VIEW_TCONTEST_ONLY = 8,
		SELECT_BY_OWNER = 1 << 4,
		// Problem specific
		VIEW = 1 << 5,
		VIEW_STATEMENT = VIEW,
		VIEW_TAGS = VIEW,
		VIEW_HIDDEN_TAGS = 1 << 6,
		VIEW_SOLUTIONS_AND_SUBMISSIONS = 1 << 7,
		VIEW_SIMFILE = 1 << 8,
		VIEW_OWNER = 1 << 9,
		VIEW_ADD_TIME = VIEW_OWNER,
		VIEW_RELATED_JOBS = 1 << 10,
		DOWNLOAD = 1 << 11,
		SUBMIT = 1 << 12,
		EDIT = 1 << 13,
		SUBMIT_IGNORED = EDIT,
		REUPLOAD = EDIT,
		REJUDGE_ALL = EDIT,
		EDIT_TAGS = 1 << 14,
		EDIT_HIDDEN_TAGS = 1 << 15,
		DELETE = EDIT,
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
	InplaceBuff<32> problems_pid;

	/// Main Problems handler
	void problems_handle();

	void problems_problem();

	/* ============================== Contest ============================== */

	enum class ContestPermissions : uint {
		NONE = 0,
		// Overall
		VIEW_PUBLIC = 2,
		VIEW_ALL = 4,
		ADD_PRIVATE = 8,
		ADD_PUBLIC = 1 << 4,
		SELECT_BY_USER = 1 << 5,
		// Semi
		MAKE_PUBLIC = ADD_PUBLIC,
		// Contest specific
		VIEW = 1 << 6,
		PARTICIPATE = 1 << 7,
		ADMIN = 1 << 8,
		EDIT_OWNERS = 1 << 9,
		DELETE = 1 << 10,
	};

	friend DECLARE_ENUM_UNARY_OPERATOR(ContestPermissions, ~)
	friend DECLARE_ENUM_OPERATOR(ContestPermissions, |)
	friend DECLARE_ENUM_OPERATOR(ContestPermissions, &)

	ContestPermissions contests_get_permissions(bool is_public,
		ContestUserMode cu_mode);

	ContestPermissions contests_get_permissions() {
		return contests_get_permissions(false, ContestUserMode::IS_NULL);
	}

	ContestPermissions contests_perms = ContestPermissions::NONE;
	InplaceBuff<32> contests_cid;
	InplaceBuff<32> contests_crid;
	InplaceBuff<32> contests_cpid;

	/// Main Contests handler
	void contests_handle();

	void contests_contest();

	void contests_contest_round();

	void contests_contest_problem();

	/* ============================= Submissions ============================= */

	enum class SubmissionPermissions : uint {
		NONE = 0,
		// Overall
		VIEW_ALL = 1,
		// Submission specific
		VIEW = 2,
		VIEW_SOURCE = 4,
		VIEW_FINAL_REPORT = 8,
		VIEW_RELATED_JOBS = 1 << 4,
		CHANGE_TYPE = 1 << 5,
		REJUDGE = 1 << 6,
		DELETE = 1 << 7
	};

	friend DECLARE_ENUM_UNARY_OPERATOR(SubmissionPermissions, ~)
	friend DECLARE_ENUM_OPERATOR(SubmissionPermissions, |)
	friend DECLARE_ENUM_OPERATOR(SubmissionPermissions, &)

	SubmissionPermissions submissions_get_permissions(
		StringView submission_owner, SubmissionType stype,
		ContestUserMode cu_mode, StringView problem_owner);

	SubmissionPermissions submissions_get_permissions() {
		return submissions_get_permissions("", SubmissionType::VOID,
			ContestUserMode::IS_NULL, "");
	}

	StringView submissions_sid;
	SubmissionPermissions submissions_perms = SubmissionPermissions::NONE;

	// Pages
	void submissions_handle();

	/* =============================== Other =============================== */

	// Pages
	void main_page();

	void static_file();

	void view_logs();

public:
	Sim() : sqlite(SQLITE_DB_FILE, SQLITE_OPEN_READONLY | SQLITE_OPEN_NOMUTEX),
		mysql(MySQL::make_conn_with_credential_file(".db.config")) {}

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
