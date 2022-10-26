#pragma once

#include "sim/contest_entry_tokens/contest_entry_token.hh"
#include "sim/random.hh"
#include "src/sim_merger/contests.hh"

#include <set>

namespace sim_merger {

struct ContestEntryTokenIdGetter {
    template <class T>
    auto& operator()(T&& x) const noexcept {
        return x.token;
    }
};

class ContestEntryTokensMerger : public Merger<sim::contest_entry_tokens::ContestEntryToken> {
    const ContestsMerger& contests_;

    std::set<decltype(sim::contest_entry_tokens::ContestEntryToken::token)> taken_tokens_;
    std::set<decltype(sim::contest_entry_tokens::ContestEntryToken::short_token)::value_type>
            taken_short_tokens_;

    void load(RecordSet& record_set) override {
        STACK_UNWINDING_MARK;

        sim::contest_entry_tokens::ContestEntryToken cet;
        mysql::Optional<
                decltype(sim::contest_entry_tokens::ContestEntryToken::short_token)::value_type>
                m_short_token;
        mysql::Optional<decltype(sim::contest_entry_tokens::ContestEntryToken::
                        short_token_expiration)::value_type>
                m_short_token_expiration;
        auto stmt = conn.prepare("SELECT token, contest_id, short_token,"
                                 " short_token_expiration "
                                 "FROM ",
                record_set.sql_table_name);
        stmt.bind_and_execute();
        stmt.res_bind_all(cet.token, cet.contest_id, m_short_token, m_short_token_expiration);
        while (stmt.next()) {
            cet.short_token = m_short_token.to_opt();
            cet.short_token_expiration = m_short_token_expiration.to_opt();

            cet.contest_id = contests_.new_id(cet.contest_id, record_set.kind);

            // Update short token
            if (cet.short_token) {
                std::string new_short_token = cet.short_token->to_string();
                while (not taken_short_tokens_.emplace(new_short_token).second) {
                    new_short_token = sim::generate_random_token(
                            decltype(sim::contest_entry_tokens::ContestEntryToken::short_token)::
                                    value_type::max_len);
                }
                cet.short_token = new_short_token;
            }

            // Time does not matter
            record_set.add_record(cet, std::chrono::system_clock::time_point::min());
        }
    }

    void merge() override {
        STACK_UNWINDING_MARK;
        Merger::merge([&](const sim::contest_entry_tokens::ContestEntryToken& /*unused*/) {
            return nullptr;
        });
    }

    PrimaryKeyType pre_merge_record_id_to_post_merge_record_id(
            const PrimaryKeyType& record_id) override {
        STACK_UNWINDING_MARK;
        std::string new_id = record_id.to_string();
        while (not taken_tokens_.emplace(new_id).second) {
            new_id = sim::generate_random_token(
                    decltype(sim::contest_entry_tokens::ContestEntryToken::token)::max_len);
        }
        return PrimaryKeyType{new_id};
    }

public:
    void save_merged() override {
        STACK_UNWINDING_MARK;
        auto transaction = conn.start_transaction();
        conn.update("TRUNCATE ", sql_table_name());
        auto stmt = conn.prepare("INSERT INTO ", sql_table_name(),
                "(token, contest_id, short_token,"
                " short_token_expiration) "
                "VALUES(?, ?, ?, ?)");

        ProgressBar progress_bar("Contest entry tokens saved:", new_table_.size(), 128);
        for (const NewRecord& new_record : new_table_) {
            Defer progressor = [&] { progress_bar.iter(); };
            const auto& x = new_record.data;
            stmt.bind_and_execute(x.token, x.contest_id, x.short_token, x.short_token_expiration);
        }

        transaction.commit();
    }

    ContestEntryTokensMerger(const PrimaryKeysFromMainAndOtherJobs& ids_from_both_jobs,
            const ContestsMerger& contests)
    : Merger("contest_entry_tokens", ids_from_both_jobs.main.contest_entry_tokens,
              ids_from_both_jobs.other.contest_entry_tokens)
    , contests_(contests) {
        STACK_UNWINDING_MARK;
        initialize();
    }
};

} // namespace sim_merger
