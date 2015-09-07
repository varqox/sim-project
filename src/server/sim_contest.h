#pragma once

#include "sim.h"

class Sim::Contest {
private:
	Contest(const Contest&);
	Contest& operator=(const Contest&);

	class RoundPath;
	enum RoundType { CONTEST, ROUND, PROBLEM };

	Sim& sim_;
	RoundPath *r_path_;
	size_t arg_beg;

	explicit Contest(Sim& sim) : sim_(sim), r_path_(NULL), arg_beg(0) {}

	~Contest();

	// Pages (sim_contest.cc)
	void addContest();

	void addRound();

	void addProblem();

	void editContest();

	void editRound();

	void editProblem();

	void deleteContest();

	void deleteRound();

	void deleteProblem();

	void problems(bool admin_view);

	void submit(bool admin_view);

	void submission();

	void submissions(bool admin_view);

	void ranking(bool admin_view);

	// Utility (sim_contest_utility.cc)
	struct Round;
	struct Subround;
	struct Problem;

	class TemplateWithMenu;

	static std::string submissionStatus(const std::string& status);

	static std::string convertStringBack(const std::string& str);

	RoundPath* getRoundPath(const std::string& round_id);

	// Returns whether user has admin access
	static bool isAdmin(Sim& sim, const RoundPath& r_path);

	// Returns whether user has admin access
	bool isAdmin() { return isAdmin(sim_, *r_path_); }

public:
	/**
 	 * @brief Main Contest handler
	 */
	void handle();

	friend Sim::Sim();
	friend Sim::~Sim();
	friend server::HttpResponse Sim::handle(std::string client_ip,
			const server::HttpRequest& req);
};
