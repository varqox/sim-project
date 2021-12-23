#include "src/web_server/old/sim.hh"
#include "simlib/mysql/mysql.hh"
#include "simlib/path.hh"
#include "simlib/random.hh"
#include "simlib/time.hh"
#include "src/web_server/http/request.hh"
#include "src/web_server/http/response.hh"

#include <memory>
#include <sys/stat.h>

using sim::users::User;
using std::string;

namespace web_server::old {

Sim::Sim() { web_worker = std::make_unique<web_worker::WebWorker>(mysql); }

http::Response Sim::handle(http::Request req) {
    request = std::move(req);
    resp = http::Response(http::Response::TEXT);

    stdlog(request.target);

    // TODO: this is pretty bad-looking
    auto hard_error500 = [&] {
        resp.status_code = "500 Internal Server Error";
        resp.headers["Content-Type"] = "text/html; charset=utf-8";
        resp.content = "<!DOCTYPE html>"
                       "<html lang=\"en\">"
                       "<head><title>500 Internal Server Error</title></head>"
                       "<body>"
                       "<center>"
                       "<h1>500 Internal Server Error</h1>"
                       "<p>Try to reload the page in a few seconds.</p>"
                       "<button onclick=\"history.go(0)\">Reload</button>"
                       "</center>"
                       "</body>"
                       "</html>";
    };

    try {
        STACK_UNWINDING_MARK;
        // Try to handle the request using the new request handling
        auto res = web_worker->handle(std::move(request));
        if (auto* response = std::get_if<http::Response>(&res)) {
            return *response;
        }
        request = std::move(std::get<http::Request>(res));

        try {
            STACK_UNWINDING_MARK;

            url_args = RequestUriParser{request.target};
            StringView next_arg = url_args.extract_next_arg();

            // Reset state
            page_template_began = false;
            notifications.clear();
            session = std::nullopt;
            form_validation_error = false;

            // Check CSRF token
            if (request.method == http::Request::POST) {
                // If no session is open, load value from cookie to pass
                // verification
                if (session_open() and
                        request.form_fields.get("csrf_token").value_or("") !=
                                session->csrf_token)
                {
                    error403();
                    goto cleanup;
                }
            }

            if (next_arg == "kit") {
                // Subsystems that do not need the session to be opened
                static_file();

            } else {
                // Other subsystems need the session to be opened in order to
                // work properly
                session_open();

                if (next_arg == "c") {
                    contests_handle();

                } else if (next_arg == "s") {
                    submissions_handle();

                } else if (next_arg == "u") {
                    users_handle();

                } else if (next_arg == "") {
                    main_page();

                } else if (next_arg == "api") {
                    api_handle();

                } else if (next_arg == "p") {
                    problems_handle();

                } else if (next_arg == "contest_file") {
                    contest_file_handle();

                } else if (next_arg == "jobs") {
                    jobs_handle();

                } else if (next_arg == "file") {
                    file_handle();

                } else if (next_arg == "logs") {
                    view_logs();

                } else {
                    error404();
                }
            }

        cleanup:
            page_template_end();

            // Make sure that the session is closed
            session_close();

#ifdef DEBUG
            if (notifications.size != 0) {
                THROW("There are notifications left: ", notifications);
            }
#endif

        } catch (const std::exception& e) {
            ERRLOG_CATCH(e);
            error500();
            session_close(); // Prevent session from being left open

        } catch (...) {
            ERRLOG_CATCH();
            error500();
            session_close(); // Prevent session from being left open
        }

    } catch (const std::exception& e) {
        ERRLOG_CATCH(e);
        // We cannot use error500() because it will probably throw
        hard_error500();
        session = std::nullopt; // Prevent session from being left open

    } catch (...) {
        ERRLOG_CATCH();
        // We cannot use error500() because it will probably throw
        hard_error500();
        session = std::nullopt; // Prevent session from being left open
    }

    return std::move(resp);
}

void Sim::main_page() {
    STACK_UNWINDING_MARK;

    page_template("Main page");
    append("main_page();");
}

void Sim::static_file() {
    STACK_UNWINDING_MARK;

    string file_path = concat_tostr("static",
            path_absolute(intentional_unsafe_string_view(
                    decode_uri(substring(request.target, 1, request.target.find('?'))))));
    // Extract path (ignore query)
    D(stdlog(file_path);)

    // Get file stat
    struct stat attr {};
    if (stat(file_path.c_str(), &attr) != -1) {
        // Extract time of last modification
        resp.headers["last-modified"] = date("%a, %d %b %Y %H:%M:%S GMT", attr.st_mtime);
        resp.set_cache(true, 100 * 24 * 60 * 60, false); // 100 days

        // If "If-Modified-Since" header is set and its value is not lower than
        // attr.st_mtime
        struct tm client_mtime {};
        auto if_modified_since = request.headers.get("if-modified-since");
        if (if_modified_since and
                strptime(if_modified_since->data(), "%a, %d %b %Y %H:%M:%S GMT",
                        &client_mtime) != nullptr and
                timegm(&client_mtime) >= attr.st_mtime)
        {
            resp.status_code = "304 Not Modified";
            return;
        }
    }

    resp.content_type = http::Response::FILE;
    resp.content = std::move(file_path);
}

void Sim::view_logs() {
    STACK_UNWINDING_MARK;

    // TODO: convert it to some kind of permissions
    if (!session.has_value()) {
        return error403();
    }

    switch (session->user_type) {
    case User::Type::ADMIN: break;
    case User::Type::TEACHER:
    case User::Type::NORMAL: return error403();
    }

    page_template("Logs");

    append("tab_logs_view($('body'));");
}

} // namespace web_server::old
