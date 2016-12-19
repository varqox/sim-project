#pragma once

#include "errors.h"
#include "problemset.h"
#include "template.h"

class Contest : virtual protected Problemset {
protected:
	Contest() = default;

	Contest(const Contest&) = delete;
	Contest(Contest&&) = delete;
	Contest& operator=(const Contest&) = delete;
	Contest& operator=(Contest&&) = delete;

	virtual ~Contest() = default;

private:
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

		explicit RoundPath(const std::string& rid) : round_id(rid) {}

		RoundPath(const RoundPath&) = delete;

		RoundPath(RoundPath&&) = default;

		RoundPath& operator=(const RoundPath&) = delete;

		RoundPath& operator=(RoundPath&&) = default;

		void swap(RoundPath& rp) {
			std::swap(admin_access, rp.admin_access);
			std::swap(type, rp.type);
			round_id.swap(rp.round_id);
			contest.swap(rp.contest);
			round.swap(rp.round);
			problem.swap(rp.problem);
		}

		~RoundPath() {}
	};

	std::unique_ptr<RoundPath> rpath;

	// Utilities
	// contest_utilities.cc
	RoundPath* getRoundPath(const std::string& round_id);

	void contestTemplate(const StringView& title, const StringView& styles = {},
		const StringView& scripts = {});

	void printRoundPath(const StringView& page = "", bool force_normal = false);

	void printRoundView(bool link_to_problem_statement,
		bool admin_view = false);

	// Pages
	// contest.cc
protected:
	/// @brief Main contest handler
	void handle();

private:
	void addContest();

	void addRound();

	void addProblem();

	void reuploadProblem();

	void editContest();

	void editRound();

	void editProblem();

	void deleteContest();

	void deleteRound();

	void deleteProblem();

	void users();

	void listProblems(bool admin_view);

	void ranking(bool admin_view);

	// submissions.cc
	void submit(bool admin_view);

protected:
	void deleteSubmission(const std::string& submission_id,
		const std::string& submission_user_id);

	void submission();

private:
	void submissions(bool admin_view);

	// files.cc
	void addFile();

	void editFile(const StringView& id, std::string name);

	void deleteFile(const StringView& id, const StringView& name);

protected:
	void file();

private:
	void files(bool admin_view);
};
