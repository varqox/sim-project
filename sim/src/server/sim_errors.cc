#include "sim.h"
#include "sim_template.h"

void SIM::error403() {
	resp_.status_code = "403 Forbidden";
	Template templ(resp_, "403 Forbidden");
	templ << "<center><h1 style=\"font-size:25px;font-weight:normal;\">403 &mdash; Sorry, but you're not allowed to see anything here.</h1></center>";
}

void SIM::error404() {
	resp_.status_code = "404 Not Found";
	Template templ(resp_, "404 Not Found");
	templ << "<center><h1 style=\"font-size:25px;font-weight:normal;\">404 &mdash; Page not found</h1></center>";
}
