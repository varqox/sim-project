#include "sim_main.h"
#include "sim_template.h"
#include "sim_errors.h"
#include "../include/string.h"
#include "../include/debug.h"

#include <unistd.h>

using std::string;

namespace sim {

server::HttpResponse main(const server::HttpRequest& req) {
	D(eprintf("%s\n", req.target.c_str()));
	if (isPrefix(req.target, "/kit"))
		return getStaticFile(req);
	else if (req.target.size() == 1 || req.target[1] == '?')
		return mainPage(req);
	else
		return error404();
}

server::HttpResponse mainPage(const server::HttpRequest& req) {
	server::HttpResponse resp;
	sim::Template templ(resp, "Main page");
	templ << "<div style=\"text-align: center\">\n"
				"<img src=\"/kit/img/SIM-logo.png\" width=\"260\" height=\"336\" alt=\"\"/>\n"
				"<p style=\"font-size: 30px\">Welcome to SIM</p>\n"
				"<hr/>\n"
				"<p>SIM is open source platform for carrying out algorithmic contests</p>\n"
				"</div>\n";
	templ.close();
	return resp;
}

server::HttpResponse getStaticFile(const server::HttpRequest& req) {
	string file = "html";
	file += abspath(decodeURI(req.target, 4, req.target.find('?'))); // Extract path ignoring query
	D(eprintf("%s\n", file.c_str()));

	// If file doesn't exist
	if (access(file.c_str(), F_OK) == -1)
		return error404();
	server::HttpResponse resp(server::HttpResponse::FILE);
	resp.content = file;
	return resp;
}

} // namespace sim
