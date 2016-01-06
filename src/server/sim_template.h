#pragma once

#include "sim.h"

class Sim::Template {
protected:
	Sim& sim_;

public:
	Template(Sim& sim, const std::string& title,
			const std::string& styles = "", const std::string& scripts = "");

	Template(const Template&) = delete;

	Template(Template&&) = delete;

	Template& operator=(const Template&) = delete;

	Template& operator=(Template&&) = delete;

	virtual ~Template();

	Template& operator<<(char c) { sim_.resp_.content += c; return *this; }

	Template& operator<<(const char* str) {
		sim_.resp_.content += str;
		return *this;
	}

	Template& operator<<(const std::string& str) {
		sim_.resp_.content += str;
		return *this;
	}
};
