#pragma once

#include "http_response.h"
#include "../include/time.h"

namespace sim {

class Template {
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

	void close() {
		resp_->content += "</div>\n"
		"</body>\n"
		"</html>\n";
		resp_ = NULL;
	}

	~Template() {
		if (resp_)
			close();
	}
};

} // namesapce sim
