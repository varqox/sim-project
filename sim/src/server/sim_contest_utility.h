#pragma once

#include "sim_contest.h"
#include "sim_template.h"

class Sim::Contest::TemplateWithMenu : public Template {
public:
	TemplateWithMenu(Contest& contest, const std::string& title,
		const std::string& styles = "", const std::string& scripts = "");

	void printRoundPath(const RoundPath& r_path, const std::string& page);

	void printRoundView(const RoundPath& r_path, bool link_to_problem_statement,
		bool admin_view = false);
};
