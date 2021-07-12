#include "src/web_server/old/sim.hh"
#include "src/web_server/ui_template.hh"

namespace web_server::old {

void Sim::page_template(StringView title) {
    STACK_UNWINDING_MARK;

    begin_ui_template(
        resp,
        {
            .title = title,
            .session = session,
            .notifications = notifications,
        });

#ifdef DEBUG
    notifications.clear();
#endif

    page_template_began = true;
}

void Sim::page_template_end() {
    STACK_UNWINDING_MARK;

    if (page_template_began) {
        page_template_began = false;
        end_ui_template(resp);
    }
}

void Sim::error_page_template(StringView status, StringView code, StringView message) {
    STACK_UNWINDING_MARK;

    resp.status_code = status.to_string();
    resp.headers.clear();

    auto prev = request.headers.get("Referer");
    if (prev.empty()) {
        prev = "/";
    }

    page_template(status);
    // clang-format off
    append("document.body.insertAdjacentHTML('beforeend', `<center>"
           "<h1 style=\"font-size:25px;font-weight:normal;\">",
              code, " &mdash; ", message, "</h1>"
           "<a class=\"btn\" href=\"", prev, "\">Go back</a>"
           "</center>`);");
    // clang-format on
}

} // namespace web_server::old
