#pragma once

#include "errors.h"
#include "template.h"

class Problemset : virtual protected Template, virtual protected Errors {
protected:
	Problemset() = default;

	Problemset(const Problemset&) = delete;
	Problemset(Problemset&&) = delete;
	Problemset& operator=(const Problemset&) = delete;
	Problemset& operator=(Problemset&&) = delete;

	virtual ~Problemset() = default;

	enum Permissions : uint {
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

	Permissions getPermissions(const std::string& owner_id, ProblemType ptype);

private:
	std::string problem_id_;
	std::string problem_name, problem_label, problem_owner, problem_added;
	std::string problem_simfile;
	ProblemType problem_type = ProblemType::VOID;
	std::string owner_username;
	uint owner_utype = UTYPE_NORMAL;
	Permissions perms = PERM_NONE;

	void problemsetTemplate(const StringView& title,
		const StringView& styles = {}, const StringView& scripts = {});

protected:
	/// Main Problemset handler
	void handle();

	/// Warning: this function assumes that the user is allowed to view the
	/// statement
	void problemStatement(StringView problem_id);

private:
	void addProblem();

	void problem();

	void editProblem();

	void downloadProblem();

	void reuploadProblem();

	void rejudgeProblemSubmissions();

	void deleteProblem();

	void problemSolutions();

	void problemSubmissions();
};
