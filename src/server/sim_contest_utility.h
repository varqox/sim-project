#pragma once

#include "sim_contest.h"
#include "sim_template.h"

struct Sim::Contest::Round {
	std::string id, parent, problem_id, access, name, owner;
	std::string begins, ends, full_results;
	bool visible, show_ranking;
};

struct Sim::Contest::Subround {
	std::string id, name;
};

struct Sim::Contest::Problem {
	std::string id, parent, name;
};

class Sim::Contest::RoundPath {
private:
	RoundPath(const RoundPath&);
	RoundPath& operator=(const RoundPath&);

public:
	bool admin_access;
	RoundType type;
	std::string round_id;
	UniquePtr<Round> contest, round, problem;

	RoundPath(const std::string& rid) : admin_access(false), type(CONTEST),
			round_id(rid), contest(NULL), round(NULL), problem(NULL) {}

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

class Sim::Contest::TemplateWithMenu : public Template {
public:
	TemplateWithMenu(Contest& contest, const std::string& title,
		const std::string& styles = "", const std::string& scripts = "");

	void printRoundPath(const RoundPath& r_path, const std::string& page);

	void printRoundView(const RoundPath& r_path, bool link_to_problem_statement,
		bool admin_view = false);
};
