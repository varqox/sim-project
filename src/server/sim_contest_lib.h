#pragma once

#include "sim.h"
#include "sim_template.h"

struct Round {
	std::string id, parent, problem_id, access, name, owner, visible;
	std::string begins, ends, full_results;
};

struct Problem {
	std::string id, parent, name;
};

struct StringOrNull {
	bool is_null;
	std::string str;

	StringOrNull(bool nulled = true) : is_null(nulled), str() {}

	void setNull() { is_null = true; }

	void setString(std::string s) {
		is_null = false;
		s.swap(str);
	}
};

enum RoundType { CONTEST, ROUND, PROBLEM };

class RoundPath {
private:
	RoundPath(const RoundPath&);
	RoundPath& operator=(const RoundPath&);

public:
	bool admin_access;
	RoundType type;
	std::string round_id;
	Round *contest, *round, *problem;

	RoundPath(const std::string& rid): admin_access(false), type(CONTEST),
			round_id(rid), contest(NULL), round(NULL), problem(NULL) {}

	~RoundPath() {
		if (contest)
			delete contest;
		if (round)
			delete round;
		if (problem)
			delete problem;
	}
};

class Contest {
private:
	class TemplateWithMenu : public SIM::Template {
	public:
		TemplateWithMenu(SIM& sim, const std::string& title,
			const RoundPath& rp, const std::string& styles = "",
			const std::string& scripts = "");
	};

	// Pages
	static void addContest(SIM& sim);

	static void addRound(SIM& sim, const RoundPath& rp);

	static void addProblem(SIM& sim, const RoundPath& rp);

	static void editContest(SIM& sim, const RoundPath& rp);

	static void editRound(SIM& sim, const RoundPath& rp);

	static void editProblem(SIM& sim, const RoundPath& rp);

	static void problems(SIM& sim, const RoundPath& rp, bool admin_view = true);

	static void submit(SIM& sim, const RoundPath& rp, bool admin_view = true);

	static void submissions(SIM& sim, const RoundPath& rp,
		bool admin_view = true);

	// Functions
	static RoundPath* getRoundPath(SIM& sim, const std::string& round_id);

	/**
	 * @brief Converts @p type to int
	 *
	 * @param type "admin" or "teacher" or "normal"
	 * @return 0 if type is "admin", 1 if type is "teacher", 2 in other case
	 */
	static int getUserRank(const std::string& type);

	static int getUserRank(SIM& sim, const std::string& id);

	// Returns whether user has admin access
	static bool isAdmin(SIM& sim, const RoundPath& rp);

	static void printRoundPath(SIM::Template& templ, const RoundPath& rp,
		const std::string& page);

	static void printRoundView(SIM& sim, SIM::Template& templ,
			const RoundPath& rp, bool link_to_problem_statement,
			bool admin_view = false);

	friend class SIM;
};
