#pragma once

#include "internal_files.hh"
#include "users.hh"

#include <sim/problems/old_problem.hh>
#include <simlib/concat.hh>
#include <simlib/libzip.hh>
#include <simlib/sim/problem_package.hh>
#include <simlib/sim/simfile.hh>

namespace sim_merger {

static std::pair<std::vector<std::string>, std::string> package_fingerprint(FilePath zip_file_path
) {
    std::vector<std::string> entries_list;
    ZipFile zip(zip_file_path, ZIP_RDONLY);
    entries_list.reserve(zip.entries_no());
    for (int i = 0; i < zip.entries_no(); ++i) {
        entries_list.emplace_back(zip.get_name(i));
    }
    sort(entries_list.begin(), entries_list.end());
    // Read statement file contents
    auto main_dir = sim::zip_package_main_dir(zip);
    auto simfile_contents = zip.extract_to_str(zip.get_index(concat(main_dir, "Simfile")));
    sim::Simfile sf(simfile_contents);
    sf.load_statement();
    auto statement_contents =
        zip.extract_to_str(zip.get_index(concat(main_dir, WONT_THROW(sf.statement.value()))));

    return {entries_list, statement_contents};
}

class ProblemsMerger : public Merger<sim::problems::OldProblem> {
    const InternalFilesMerger& internal_files_;
    const UsersMerger& users_;
    bool reset_new_problems_time_limits_;

    std::map<decltype(sim::problems::OldProblem::id), decltype(sim::problems::OldProblem::id)>
        to_merge_; // (new problem id, new target problem id)

    void load(RecordSet& record_set) override {
        STACK_UNWINDING_MARK;
        sim::problems::OldProblem prob;
        old_mysql::Optional<decltype(prob.owner_id)::value_type> m_owner_id;
        auto old_mysql = old_mysql::ConnectionView{*mysql};
        auto stmt = old_mysql.prepare(
            "SELECT id, file_id, type, name, label, "
            "simfile, owner_id, created_at, updated_at FROM ",
            record_set.sql_table_name
        );
        stmt.bind_and_execute();
        stmt.res_bind_all(
            prob.id,
            prob.file_id,
            prob.type,
            prob.name,
            prob.label,
            prob.simfile,
            m_owner_id,
            prob.created_at,
            prob.updated_at
        );
        while (stmt.next()) {
            prob.file_id = internal_files_.new_id(prob.file_id, record_set.kind);
            prob.owner_id = m_owner_id.to_opt();
            if (prob.owner_id.has_value()) {
                prob.owner_id = users_.new_id(prob.owner_id.value(), record_set.kind);
            }

            record_set.add_record(
                prob, str_to_time_point(from_unsafe{prob.created_at.to_string()})
            );
        }
    }

    void merge() override {
        STACK_UNWINDING_MARK;
        Merger::merge([&](const sim::problems::OldProblem& /*unused*/) { return nullptr; });

        std::map<size_t, size_t> dsu; // (problem idx, target problem idx)
        // Returns problem index to which problem of index @p idx will be merged
        auto find = [&](size_t idx) {
            auto impl = [&dsu](auto& self, size_t x) -> size_t {
                auto it = dsu.find(x);
                throw_assert(it != dsu.end());
                auto& dsu_idx = it->second;
                return (x == dsu_idx ? x : dsu_idx = self(self, dsu_idx));
            };

            return impl(impl, idx);
        };

        auto merge_into = [&](size_t src_idx, size_t target_idx) {
            dsu[find(src_idx)] = find(target_idx);
        };

        std::map<
            std::invoke_result_t<decltype(package_fingerprint), FilePath>,
            decltype(sim::problems::OldProblem::id)>
            pkg_fingerprint_to_problem_idx;
        for (size_t idx = 0; idx < new_table_.size(); ++idx) {
            dsu.emplace(idx, idx);
            const auto& p = new_table_[idx].data;
            auto p_fingerprint = package_fingerprint(internal_files_.path_to_file(p.file_id));

            auto it = pkg_fingerprint_to_problem_idx.find(p_fingerprint);
            if (it == pkg_fingerprint_to_problem_idx.end()) {
                pkg_fingerprint_to_problem_idx.emplace(p_fingerprint, idx);
                continue; // No duplicate was found
            }

            size_t other_idx = find(it->second);
            // Problem with lower new id will be the target problem
            merge_into(idx, other_idx);
        }

        for (auto [src_idx, dest_idx] : dsu) {
            dest_idx = find(src_idx);
            if (src_idx == dest_idx) {
                continue; // No merging
            }

            const auto& src = new_table_[src_idx];
            const auto& dest = new_table_[dest_idx];
            to_merge_.emplace(src.data.id, dest.data.id);
            stdlog(
                "\nWill merge: ",
                src.data.name,
                ' ',
                id_info(src),
                "\n      into: ",
                dest.data.name,
                ' ',
                id_info(dest)
            );
        }
    }

public:
    void save_merged() override {
        STACK_UNWINDING_MARK;
        auto transaction = mysql->start_repeatable_read_transaction();
        auto old_mysql = old_mysql::ConnectionView{*mysql};
        old_mysql.update("TRUNCATE ", sql_table_name());
        auto stmt = old_mysql.prepare(
            "INSERT INTO ",
            sql_table_name(),
            "(id, file_id, type, name, label, simfile,"
            " owner_id, created_at, updated_at) "
            "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?)"
        );

        ProgressBar progress_bar("Problems saved:", new_table_.size(), 128);
        for (const NewRecord& new_record : new_table_) {
            Defer progressor = [&] { progress_bar.iter(); };
            const auto& x = new_record.data;
            stmt.bind_and_execute(
                x.id,
                x.file_id,
                x.type,
                x.name,
                x.label,
                x.simfile,
                x.owner_id,
                x.created_at,
                x.updated_at
            );
        }

        old_mysql.update("ALTER TABLE ", sql_table_name(), " AUTO_INCREMENT=", last_new_id_ + 1);
        transaction.commit();
    }

    ProblemsMerger(
        const PrimaryKeysFromMainAndOtherJobs& ids_from_both_jobs,
        const InternalFilesMerger& internal_files,
        const UsersMerger& users,
        bool reset_new_problems_time_limits
    )
    : Merger("problems", ids_from_both_jobs.main.problems, ids_from_both_jobs.other.problems)
    , internal_files_(internal_files)
    , users_(users)
    , reset_new_problems_time_limits_(reset_new_problems_time_limits) {
        STACK_UNWINDING_MARK;
        initialize();
    }

    void run_after_saving_hooks() override {
        STACK_UNWINDING_MARK;
        auto transaction = mysql->start_repeatable_read_transaction();
        // Add jobs to merge the problems
        for (auto [src_id, dest_id] : to_merge_) {
            throw_assert(src_id != dest_id);
            auto old_mysql = old_mysql::ConnectionView{*mysql};
            old_mysql
                .prepare("INSERT jobs (creator, status, priority, type, created_at,"
                         " aux_id, aux_id_2) VALUES(NULL, ?, ?, ?, ?, ?, ?)")
                .bind_and_execute(
                    EnumVal(sim::jobs::OldJob::Status::PENDING),
                    default_priority(sim::jobs::OldJob::Type::MERGE_PROBLEMS),
                    EnumVal(sim::jobs::OldJob::Type::MERGE_PROBLEMS),
                    utc_mysql_datetime(),
                    src_id,
                    dest_id
                );
        }

        if (reset_new_problems_time_limits_) {
            for (auto& new_record : new_table_) {
                const sim::problems::OldProblem& p = new_record.data;
                if (not new_record.other_ids.empty() and to_merge_.count(p.id) == 0) {
                    schedule_reseting_problem_time_limits(p.id);
                }
            }
        }

        transaction.commit();
    }

private:
    static void schedule_reseting_problem_time_limits(decltype(sim::problems::OldProblem::id
    ) problem_new_id) {
        auto old_mysql = old_mysql::ConnectionView{*mysql};
        old_mysql
            .prepare("INSERT jobs (creator, status, priority, type, created_at, aux_id) "
                     "VALUES(NULL, ?, ?, ?, ?, ?)")
            .bind_and_execute(
                EnumVal(sim::jobs::OldJob::Status::PENDING),
                default_priority(
                    sim::jobs::OldJob::Type::RESET_PROBLEM_TIME_LIMITS_USING_MODEL_SOLUTION
                ),
                EnumVal(sim::jobs::OldJob::Type::RESET_PROBLEM_TIME_LIMITS_USING_MODEL_SOLUTION),
                utc_mysql_datetime(),
                problem_new_id
            );
    }
};

} // namespace sim_merger
