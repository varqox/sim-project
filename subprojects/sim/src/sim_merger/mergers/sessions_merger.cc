#include "../progress_printer.hh"
#include "../sim_instance.hh"
#include "sessions_merger.hh"
#include "users_merger.hh"

#include <sim/sessions/session.hh>
#include <sim/sql/sql.hh>
#include <simlib/macros/stack_unwinding.hh>

using sim::sessions::Session;
using sim::sql::InsertIgnoreInto;
using sim::sql::Select;

namespace mergers {

SessionsMerger::SessionsMerger(
    const SimInstance& main_sim, const SimInstance& other_sim, const UsersMerger& users_merger
)
: main_sim{main_sim}
, other_sim{other_sim}
, users_merger{users_merger} {}

void SessionsMerger::merge_and_save() {
    STACK_UNWINDING_MARK;

    // Save other sessions, but skip ones with id that exists in the main sim
    auto other_sessions_copying_progress_printer =
        ProgressPrinter<decltype(Session::id)>{"progress: copying session with id:"};
    static_assert(Session::COLUMNS_NUM == 6, "Update the statements below");
    auto stmt = other_sim.mysql.execute(Select("id, csrf_token, user_id, data, user_agent, expires")
                                            .from("sessions")
                                            .order_by("id DESC"));
    stmt.do_not_store_result(); // minimize memory usage
    Session session;
    stmt.res_bind(
        session.id,
        session.csrf_token,
        session.user_id,
        session.data,
        session.user_agent,
        session.expires
    );
    while (stmt.next()) {
        other_sessions_copying_progress_printer.note_id(session.id);
        main_sim.mysql.execute(
            InsertIgnoreInto("sessions (id, csrf_token, user_id, data, user_agent, expires)")
                .values(
                    "?, ?, ?, ?, ?, ?",
                    session.id,
                    session.csrf_token,
                    users_merger.other_id_to_new_id(session.user_id),
                    session.data,
                    session.user_agent,
                    session.expires
                )
        );
    }
}

} // namespace mergers
