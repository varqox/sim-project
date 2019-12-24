#pragma once

#include "internal_files.hh"
#include "users.hh"

#include <sim/problem.hh>
#include <simlib/libzip.h>
#include <simlib/sim/problem_package.h>
#include <simlib/sim/simfile.h>

static std::pair<std::vector<std::string>, std::string>
package_fingerprint(FilePath zip_file_path) {
	std::vector<std::string> entries_list;
	ZipFile zip(zip_file_path, ZIP_RDONLY);
	for (int i = 0; i < zip.entries_no(); ++i)
		entries_list.emplace_back(zip.get_name(i));
	sort(entries_list.begin(), entries_list.end());
	// Read statement file contents
	auto master_dir = sim::zip_package_master_dir(zip);
	auto simfile_contents =
	   zip.extract_to_str(zip.get_index(concat(master_dir, "Simfile")));
	sim::Simfile sf(simfile_contents);
	sf.load_statement();
	auto statement_contents =
	   zip.extract_to_str(zip.get_index(concat(master_dir, sf.statement)));

	return {entries_list, statement_contents};
}

class ProblemsMerger : public Merger<sim::Problem> {
	const InternalFilesMerger& internal_files_;
	const UsersMerger& users_;
	bool reset_new_problems_time_limits_;

	std::map<uintmax_t, uintmax_t>
	   to_merge_; // (new problem id, new target problem id)

	void load(RecordSet& record_set) override {
		STACK_UNWINDING_MARK;
		sim::Problem prob;
		MySQL::Optional<decltype(prob.owner)::value_type> m_owner;
		auto stmt = conn.prepare("SELECT id, file_id, type, name, label, "
		                         "simfile, owner, added, last_edit FROM ",
		                         record_set.sql_table_name);
		stmt.bindAndExecute();
		stmt.res_bind_all(prob.id, prob.file_id, prob.type, prob.name,
		                  prob.label, prob.simfile, m_owner, prob.added,
		                  prob.last_edit);
		while (stmt.next()) {
			prob.file_id =
			   internal_files_.new_id(prob.file_id, record_set.kind);
			prob.owner = m_owner.opt();
			if (prob.owner.has_value())
				prob.owner = users_.new_id(prob.owner.value(), record_set.kind);

			record_set.add_record(prob, strToTimePoint(prob.added.to_string()));
		}
	}

	void merge() override {
		STACK_UNWINDING_MARK;
		Merger::merge([&](const sim::Problem&) { return nullptr; });

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

		std::map<std::invoke_result_t<decltype(package_fingerprint), FilePath>,
		         uintmax_t>
		   pkg_fingerprint_to_problem_idx;
		for (size_t idx = 0; idx < new_table_.size(); ++idx) {
			dsu.emplace(idx, idx);
			auto const& p = new_table_[idx].data;
			auto p_fingerprint =
			   package_fingerprint(internal_files_.path_to_file(p.file_id));

			auto it = pkg_fingerprint_to_problem_idx.find(p_fingerprint);
			if (it == pkg_fingerprint_to_problem_idx.end()) {
				pkg_fingerprint_to_problem_idx.emplace(p_fingerprint, idx);
				continue; // No duplicate was found
			}

			size_t other_idx = find(it->second);
			auto const& other = new_table_[other_idx].data;
			// Newer problem will be the target problem
			if (other.last_edit <= p.last_edit)
				merge_into(other_idx, idx);
			else
				merge_into(idx, other_idx);
		}

		for (auto [src_idx, dest_idx] : dsu) {
			dest_idx = find(src_idx);
			if (src_idx == dest_idx)
				continue; // No merging

			auto const& src = new_table_[src_idx];
			auto const& dest = new_table_[dest_idx];
			to_merge_.emplace(src.data.id, dest.data.id);
			stdlog("\nWill merge: ", src.data.name, ' ', id_info(src),
			       "\n      into: ", dest.data.name, ' ', id_info(dest));
		}
	}

public:
	void save_merged() override {
		STACK_UNWINDING_MARK;
		auto transaction = conn.start_transaction();
		conn.update("TRUNCATE ", sql_table_name());
		auto stmt = conn.prepare("INSERT INTO ", sql_table_name(),
		                         "(id, file_id, type, name, label, simfile,"
		                         " owner, added, last_edit) "
		                         "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?)");
		for (const NewRecord& new_record : new_table_) {
			const sim::Problem& x = new_record.data;
			stmt.bindAndExecute(x.id, x.file_id, x.type, x.name, x.label,
			                    x.simfile, x.owner, x.added, x.last_edit);
		}

		conn.update("ALTER TABLE ", sql_table_name(),
		            " AUTO_INCREMENT=", last_new_id_ + 1);
		transaction.commit();
	}

	ProblemsMerger(const IdsFromMainAndOtherJobs& ids_from_both_jobs,
	               const InternalFilesMerger& internal_files,
	               const UsersMerger& users,
	               bool reset_new_problems_time_limits)
	   : Merger("problems", ids_from_both_jobs.main.problems,
	            ids_from_both_jobs.other.problems),
	     internal_files_(internal_files), users_(users),
	     reset_new_problems_time_limits_(reset_new_problems_time_limits) {
		STACK_UNWINDING_MARK;
		initialize();
	}

	void run_after_saving_hooks() override {
		STACK_UNWINDING_MARK;
		auto transaction = conn.start_transaction();
		// Add jobs to merge the problems
		for (auto [src_id, dest_id] : to_merge_) {
			throw_assert(src_id != dest_id);
			conn
			   .prepare("INSERT jobs (creator, status, priority, type, added,"
			            " aux_id, info, data) "
			            "VALUES(NULL, " JSTATUS_PENDING_STR ", ?,"
			            " " JTYPE_MERGE_PROBLEMS_STR,
			            ", ?, ?, ?, '')")
			   .bindAndExecute(priority(JobType::MERGE_PROBLEMS), mysql_date(),
			                   src_id,
			                   jobs::MergeProblemsInfo(dest_id, false).dump());
		}

		if (reset_new_problems_time_limits_) {
			for (auto& new_record : new_table_) {
				const sim::Problem& p = new_record.data;
				if (not new_record.other_ids.empty() and
				    to_merge_.count(p.id) == 0) {
					schedule_reseting_problem_time_limits(p.id);
				}
			}
		}

		transaction.commit();
	}

private:
	void schedule_reseting_problem_time_limits(uintmax_t problem_new_id) {
		conn
		   .prepare(
		      "INSERT jobs (creator, status, priority, type, added, aux_id,"
		      " info, data) "
		      "VALUES(NULL, " JSTATUS_PENDING_STR
		      ", ?, " JTYPE_RESET_PROBLEM_TIME_LIMITS_USING_MODEL_SOLUTION_STR,
		      ", ?, ?, '', '')")
		   .bindAndExecute(
		      priority(JobType::RESET_PROBLEM_TIME_LIMITS_USING_MODEL_SOLUTION),
		      mysql_date(), problem_new_id);
	}
};
