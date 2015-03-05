#pragma once

#include "sim.h"

class SIM::Template {
private:
	Template(const Template&);
	Template& operator=(const Template&);

	SIM& sim_;

public:
	Template(SIM& sim, const std::string& title,
			const std::string& scripts = "", const std::string& styles = "");

	Template& operator<<(char c) { sim_.resp_.content += c; return *this; }

	Template& operator<<(const char* str) {
		sim_.resp_.content += str;
		return *this;
	}

	Template& operator<<(const std::string& str) {
		sim_.resp_.content += str;
		return *this;
	}

	~Template() {
		*this << "</div>\n"
			"</body>\n"
			"</html>\n";
	}
};
