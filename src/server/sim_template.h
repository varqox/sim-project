#pragma once

#include "sim.h"

class SIM::Template {
private:
	Template(const Template&);
	Template& operator=(const Template&);

	server::HttpResponse *resp_;

public:
	Template(server::HttpResponse& resp, const std::string& title, const std::string& scripts = "",
			const std::string& styles = "");

	Template& operator<<(char c) { resp_->content += c; return *this; }

	Template& operator<<(const char* str) {
		resp_->content += str;
		return *this;
	}

	Template& operator<<(const std::string& str) {
		resp_->content += str;
		return *this;
	}

	~Template() {
		*this << "</div>\n"
			"</body>\n"
			"</html>\n";
	}
};
