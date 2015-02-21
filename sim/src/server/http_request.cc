#include <unistd.h>
#include "http_request.h"

using std::map;
using std::string;

namespace server {
HttpRequest::Form::~Form() {
	for (map<string, string>::iterator i = files.begin(); i != files.end(); ++i)
		unlink(i->second.c_str());
}
} // namespace server
