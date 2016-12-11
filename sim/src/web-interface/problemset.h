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

private:
	std::string problem_id_;
	std::string problem_name, problem_label, problem_owner, problem_added;
	std::string problem_simfile;
	bool problem_is_public;
	std::string owner_username;
	uint owner_utype;

	void problemsetTemplate(const StringView& title,
		const StringView& styles = {}, const StringView& scripts = {});

	// Pages
protected:
	// @brief Main User handler
	void handle();

	void problemStatement(StringView problem_id);

private:
	void addProblem();

	void problem();

	void editProblem();

	void deleteProblem();

	void problemSolutions();
};
