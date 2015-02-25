#pragma once

#include "http_response.h"
#include "sim_template.h"

namespace sim {

inline server::HttpResponse error403() {
	server::HttpResponse resp(server::HttpResponse::TEXT, "403 Forbidden");
	Template templ(resp, "403 Forbidden");
	templ << "<center><h1 style=\"font-size:25px;font-weight:normal;\">403 &mdash; Sorry, but you're not allowed to see anything here.</h1></center>";
	templ.close();
	return resp;
}

inline server::HttpResponse error404() {
	server::HttpResponse resp(server::HttpResponse::TEXT, "404 Not Found");
	Template templ(resp, "404 Not Found");
	templ << "<center><h1 style=\"font-size:25px;font-weight:normal;\">404 &mdash; Page not found</h1></center>";
	templ.close();
	return resp;
}

} // namespace sim
