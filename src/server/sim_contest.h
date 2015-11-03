#pragma once

#include "sim.h"

class Sim::Contest {
private:
	Contest(const Contest&);
	Contest& operator=(const Contest&);

	struct Round {
		std::string id, parent, problem_id, access, name, owner;
		std::string begins, ends, full_results;
		bool visible, show_ranking;
	};

	struct Subround {
		std::string id, name;
	};

	struct Problem {
		std::string id, parent, name;
	};

	enum RoundType {
		CONTEST = 0,
		ROUND = 1,
		PROBLEM = 2
	};

	class RoundPath {
	private:
		RoundPath(const RoundPath&);
		RoundPath& operator=(const RoundPath&);

	public:
		bool admin_access;
		RoundType type;
		std::string round_id;
		UniquePtr<Round> contest, round, problem;

		RoundPath(const std::string& rid) : admin_access(false), type(CONTEST),
				round_id(rid), contest(nullptr), round(nullptr),
				problem(nullptr) {}

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

	Sim& sim_;
	UniquePtr<RoundPath> r_path_;
	size_t arg_beg;

	explicit Contest(Sim& sim) : sim_(sim), arg_beg(0) {}

	~Contest() {}

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

	// Contest utilities (sim_contest_utility.cc)
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
