#pragma once

#include "sim/random.hh"
#include "simlib/defer.hh"
#include "src/sim_merger/users.hh"

#include <set>

struct Session {
    InplaceBuff<SESSION_ID_LEN> id;
    InplaceBuff<SESSION_CSRF_TOKEN_LEN> csrf_token;
    uintmax_t user_id{};
    InplaceBuff<32> data;
    InplaceBuff<SESSION_IP_LEN> ip;
    InplaceBuff<128> user_agent;
    InplaceBuff<24> expires;
};

class SessionsMerger : public Merger<Session> {
    const UsersMerger& users_;

    std::set<InplaceBuff<SESSION_ID_LEN>> taken_sessions_ids_;

    void load(RecordSet& record_set) override {
        STACK_UNWINDING_MARK;
        // Prefer main users to come first
        auto time = [&] {
            switch (record_set.kind) {
            case IdKind::Main: return std::chrono::system_clock::from_time_t(0);
            case IdKind::Other: return std::chrono::system_clock::time_point::max();
            }

            THROW("BUG");
        }();

        Session ses;
        auto stmt = conn.prepare(
            "SELECT id, csrf_token, user_id, data, ip, "
            "user_agent, expires FROM ",
            record_set.sql_table_name);
        stmt.bind_and_execute();
        stmt.res_bind_all(
            ses.id, ses.csrf_token, ses.user_id, ses.data, ses.ip, ses.user_agent,
            ses.expires);

        while (stmt.next()) {
            ses.user_id = users_.new_id(ses.user_id, record_set.kind);
            record_set.add_record(ses, time);
        }
    }

    void merge() override {
        STACK_UNWINDING_MARK;
        Merger::merge([&](const Session& /*unused*/) { return nullptr; });
    }

    InplaceBuff<SESSION_ID_LEN> new_id_for_record_to_merge_into_new_records(
        const InplaceBuff<SESSION_ID_LEN>& record_id) override {
        STACK_UNWINDING_MARK;
        std::string new_id = record_id.to_string();
        while (not taken_sessions_ids_.emplace(new_id).second) {
            new_id = generate_random_token(SESSION_ID_LEN);
        }
        return InplaceBuff<SESSION_ID_LEN>(new_id);
    }

public:
    void save_merged() override {
        STACK_UNWINDING_MARK;
        auto transaction = conn.start_transaction();
        conn.update("TRUNCATE ", sql_table_name());
        auto stmt = conn.prepare(
            "INSERT INTO ", sql_table_name(),
            "(id, csrf_token, user_id, data, ip,"
            " user_agent, expires) "
            "VALUES(?, ?, ?, ?, ?, ?, ?)");

        ProgressBar progress_bar("Sessions saved:", new_table_.size(), 128);
        for (const NewRecord& new_record : new_table_) {
            Defer progressor = [&] { progress_bar.iter(); };
            const Session& x = new_record.data;
            stmt.bind_and_execute(
                x.id, x.csrf_token, x.user_id, x.data, x.ip, x.user_agent, x.expires);
        }

        transaction.commit();
    }

    SessionsMerger(const IdsFromMainAndOtherJobs& ids_from_both_jobs, const UsersMerger& users)
    : Merger("session", ids_from_both_jobs.main.sessions, ids_from_both_jobs.other.sessions)
    , users_(users) {
        STACK_UNWINDING_MARK;
        initialize();
    }
};
