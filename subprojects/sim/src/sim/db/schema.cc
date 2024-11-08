#include <algorithm>
#include <map>
#include <regex>
#include <sim/add_problem_jobs/add_problem_job.hh>
#include <sim/change_problem_statement_jobs/change_problem_statement_job.hh>
#include <sim/contest_entry_tokens/contest_entry_token.hh>
#include <sim/contest_files/contest_file.hh>
#include <sim/contest_problems/contest_problem.hh>
#include <sim/contest_rounds/contest_round.hh>
#include <sim/contest_users/contest_user.hh>
#include <sim/contests/contest.hh>
#include <sim/db/schema.hh>
#include <sim/db/tables.hh>
#include <sim/merge_problems_jobs/merge_problems_job.hh>
#include <sim/mysql/mysql.hh>
#include <sim/problem_tags/problem_tag.hh>
#include <sim/problems/problem.hh>
#include <sim/reupload_problem_jobs/reupload_problem_job.hh>
#include <sim/sessions/session.hh>
#include <sim/sql/sql.hh>
#include <sim/submissions/submission.hh>
#include <simlib/ranges.hh>
#include <simlib/string_traits.hh>
#include <simlib/string_view.hh>
#include <simlib/throw_assert.hh>
#include <string>
#include <string_view>
#include <vector>

using sim::add_problem_jobs::AddProblemJob;
using sim::change_problem_statement_jobs::ChangeProblemStatementJob;
using sim::contest_entry_tokens::ContestEntryToken;
using sim::contest_files::ContestFile;
using sim::contest_problems::ContestProblem;
using sim::contest_rounds::ContestRound;
using sim::contest_users::ContestUser;
using sim::contests::Contest;
using sim::internal_files::InternalFile;
using sim::jobs::Job;
using sim::merge_problems_jobs::MergeProblemsJob;
using sim::problem_tags::ProblemTag;
using sim::problems::Problem;
using sim::reupload_problem_jobs::ReuploadProblemJob;
using sim::sessions::Session;
using sim::submissions::Submission;
using sim::users::User;
using std::string;
using std::vector;

namespace sim::db {

const DbSchema& get_schema() {
    static const DbSchema schema = {
        // In topological order (every table depends only on the previous tables)
        .table_schemas =
            {
                {
                    // clang-format off
                    .create_table_sql = concat_tostr(
                        "CREATE TABLE `internal_files` ("
                        "  `id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,"
                        "  `created_at` datetime NOT NULL,"
                        "  PRIMARY KEY (`id`)"
                        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb3 COLLATE=utf8mb3_bin"
                    ),
                    // clang-format on
                },
                {
                    // clang-format off
                    .create_table_sql = concat_tostr(
                        "CREATE TABLE `users` ("
                        "  `id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,"
                        "  `created_at` datetime NOT NULL,"
                        "  `type` tinyint(3) unsigned NOT NULL,"
                        "  `username` varbinary(", decltype(User::username)::max_len, ") NOT NULL,"
                        "  `first_name` varbinary(", decltype(User::first_name)::max_len, ") NOT NULL,"
                        "  `last_name` varbinary(", decltype(User::last_name)::max_len, ") NOT NULL,"
                        "  `email` varbinary(", decltype(User::email)::max_len, ") NOT NULL,"
                        "  `password_salt` binary(", decltype(User::password_salt)::len, ") NOT NULL,"
                        "  `password_hash` binary(", decltype(User::password_hash)::len, ") NOT NULL,"
                        "  PRIMARY KEY (`id`),"
                        "  UNIQUE KEY `username` (`username`),"
                        "  KEY `type` (`type`,`id` DESC)"
                        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb3 COLLATE=utf8mb3_bin"
                    ),
                    // clang-format on
                },
                {
                    // clang-format off
                    .create_table_sql = concat_tostr(
                        "CREATE TABLE `sessions` ("
                        "  `id` binary(", decltype(Session::id)::len, ") NOT NULL,"
                        "  `csrf_token` binary(", decltype(Session::csrf_token)::len, ") NOT NULL,"
                        "  `user_id` bigint(20) unsigned NOT NULL,"
                        "  `data` blob NOT NULL,"
                        "  `user_agent` blob NOT NULL,"
                        "  `expires` datetime NOT NULL,"
                        "  PRIMARY KEY (`id`),"
                        "  KEY `expires` (`expires`),"
                        "  KEY `user_id` (`user_id`),"
                        "  CONSTRAINT `sessions_ibfk_1` FOREIGN KEY (`user_id`) REFERENCES `users` (`id`) ON DELETE CASCADE ON UPDATE CASCADE"
                        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb3 COLLATE=utf8mb3_bin"
                    ),
                    // clang-format on
                },
                {
                    // clang-format off
                    .create_table_sql = concat_tostr(
                        "CREATE TABLE `problems` ("
                        "  `id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,"
                        "  `created_at` datetime NOT NULL,"
                        "  `file_id` bigint(20) unsigned NOT NULL,"
                        "  `visibility` tinyint(3) unsigned NOT NULL,"
                        "  `name` varbinary(", decltype(Problem::name)::max_len, ") NOT NULL,"
                        "  `label` varbinary(", decltype(Problem::label)::max_len, ") NOT NULL,"
                        "  `simfile` mediumblob NOT NULL,"
                        "  `owner_id` bigint(20) unsigned DEFAULT NULL," // TODO: remove and add problem_users
                        "  `updated_at` datetime NOT NULL,"
                        "  PRIMARY KEY (`id`),"
                        "  KEY `file_id` (`file_id`),"
                        "  KEY `owner_id_2` (`owner_id`,`visibility`,`id`),"
                        "  KEY `owner_id` (`owner_id`,`id`),"
                        "  KEY `visibility` (`visibility`,`id`),"
                        "  CONSTRAINT `problems_ibfk_1` FOREIGN KEY (`file_id`) REFERENCES `internal_files` (`id`) ON UPDATE CASCADE,"
                        "  CONSTRAINT `problems_ibfk_2` FOREIGN KEY (`owner_id`) REFERENCES `users` (`id`) ON DELETE SET NULL ON UPDATE CASCADE"
                        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb3 COLLATE=utf8mb3_bin"
                    ),
                    // clang-format on
                },
                {
                    // clang-format off
                    .create_table_sql = concat_tostr(
                        "CREATE TABLE `problem_tags` ("
                        "  `problem_id` bigint(20) unsigned NOT NULL,"
                        "  `is_hidden` tinyint(1) NOT NULL,"
                        "  `name` varbinary(", decltype(ProblemTag::name)::max_len, ") NOT NULL,"
                        "  PRIMARY KEY (`problem_id`,`is_hidden`,`name`),"
                        "  KEY `name` (`name`,`problem_id`),"
                        "  CONSTRAINT `problem_tags_ibfk_1` FOREIGN KEY (`problem_id`) REFERENCES `problems` (`id`) ON DELETE CASCADE ON UPDATE CASCADE"
                        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb3 COLLATE=utf8mb3_bin"
                    ),
                    // clang-format on
                },
                {
                    // clang-format off
                    .create_table_sql = concat_tostr(
                        "CREATE TABLE `contests` ("
                        "  `id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,"
                        "  `created_at` datetime NOT NULL,"
                        "  `name` varbinary(", decltype(Contest::name)::max_len, ") NOT NULL,"
                        "  `is_public` tinyint(1) NOT NULL DEFAULT 0,"
                        "  PRIMARY KEY (`id`),"
                        "  KEY `is_public` (`is_public`,`id`)"
                        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb3 COLLATE=utf8mb3_bin"
                    ),
                    // clang-format on
                },
                {
                    // clang-format off
                    .create_table_sql = concat_tostr(
                        "CREATE TABLE `contest_rounds` ("
                        "  `id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,"
                        "  `created_at` datetime NOT NULL,"
                        "  `contest_id` bigint(20) unsigned NOT NULL,"
                        "  `name` varbinary(", decltype(ContestRound::name)::max_len, ") NOT NULL,"
                        "  `item` bigint(20) unsigned NOT NULL,"
                        "  `begins` binary(", decltype(ContestRound::begins)::len, ") NOT NULL,"
                        "  `ends` binary(", decltype(ContestRound::ends)::len, ") NOT NULL,"
                        "  `full_results` binary(", decltype(ContestRound::full_results)::len, ") NOT NULL,"
                        "  `ranking_exposure` binary(", decltype(ContestRound::ranking_exposure)::len, ") NOT NULL,"
                        "  PRIMARY KEY (`id`),"
                        "  UNIQUE KEY `contest_id_3` (`contest_id`,`item`),"
                        "  KEY `contest_id_2` (`contest_id`,`begins`),"
                        "  KEY `contest_id` (`contest_id`,`ranking_exposure`),"
                        "  CONSTRAINT `contest_rounds_ibfk_1` FOREIGN KEY (`contest_id`) REFERENCES `contests` (`id`) ON DELETE CASCADE ON UPDATE CASCADE"
                        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb3 COLLATE=utf8mb3_bin"
                    ),
                    // clang-format on
                },
                {
                    // clang-format off
                    .create_table_sql = concat_tostr(
                        "CREATE TABLE `contest_problems` ("
                        "  `id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,"
                        "  `created_at` datetime NOT NULL,"
                        "  `contest_round_id` bigint(20) unsigned NOT NULL,"
                        "  `contest_id` bigint(20) unsigned NOT NULL,"
                        "  `problem_id` bigint(20) unsigned NOT NULL,"
                        "  `name` varbinary(", decltype(ContestProblem::name)::max_len, ") NOT NULL,"
                        "  `item` bigint(20) unsigned NOT NULL,"
                        "  `method_of_choosing_final_submission` tinyint(3) unsigned NOT NULL,"
                        "  `score_revealing` tinyint(3) unsigned NOT NULL,"
                        "  PRIMARY KEY (`id`),"
                        "  UNIQUE KEY `contest_round_id` (`contest_round_id`,`item`),"
                        "  KEY `contest_id` (`contest_id`),"
                        "  KEY `problem_id` (`problem_id`,`id`),"
                        "  CONSTRAINT `contest_problems_ibfk_1` FOREIGN KEY (`contest_id`) REFERENCES `contests` (`id`) ON DELETE CASCADE ON UPDATE CASCADE,"
                        "  CONSTRAINT `contest_problems_ibfk_2` FOREIGN KEY (`contest_round_id`) REFERENCES `contest_rounds` (`id`) ON DELETE CASCADE ON UPDATE CASCADE,"
                        "  CONSTRAINT `contest_problems_ibfk_3` FOREIGN KEY (`problem_id`) REFERENCES `problems` (`id`) ON DELETE CASCADE ON UPDATE CASCADE"
                        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb3 COLLATE=utf8mb3_bin"
                    ),
                    // clang-format on
                },
                {
                    // clang-format off
                    .create_table_sql = concat_tostr(
                        "CREATE TABLE `contest_users` ("
                        "  `user_id` bigint(20) unsigned NOT NULL,"
                        "  `contest_id` bigint(20) unsigned NOT NULL,"
                        "  `mode` tinyint(3) unsigned NOT NULL,"
                        "  PRIMARY KEY (`user_id`,`contest_id`),"
                        "  KEY `contest_id` (`contest_id`,`user_id`),"
                        "  KEY `contest_id_2` (`contest_id`,`mode`,`user_id`),"
                        "  CONSTRAINT `contest_users_ibfk_1` FOREIGN KEY (`user_id`) REFERENCES `users` (`id`) ON DELETE CASCADE ON UPDATE CASCADE,"
                        "  CONSTRAINT `contest_users_ibfk_2` FOREIGN KEY (`contest_id`) REFERENCES `contests` (`id`) ON DELETE CASCADE ON UPDATE CASCADE"
                        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb3 COLLATE=utf8mb3_bin"
                    ),
                    // clang-format on
                },
                {
                    // clang-format off
                    .create_table_sql = concat_tostr(
                        "CREATE TABLE `contest_files` ("
                        "  `id` binary(", decltype(ContestFile::id)::len, ") NOT NULL,"
                        "  `file_id` bigint(20) unsigned NOT NULL,"
                        "  `contest_id` bigint(20) unsigned NOT NULL,"
                        "  `name` varbinary(", decltype(ContestFile::name)::max_len, ") NOT NULL,"
                        "  `description` varbinary(", decltype(ContestFile::description)::max_len, ") NOT NULL,"
                        "  `file_size` bigint(20) unsigned NOT NULL,"
                        "  `modified` datetime NOT NULL,"
                        "  `creator` bigint(20) unsigned DEFAULT NULL,"
                        "  PRIMARY KEY (`id`),"
                        "  KEY `contest_id` (`contest_id`,`modified`),"
                        "  KEY `file_id` (`file_id`),"
                        "  KEY `creator` (`creator`),"
                        "  CONSTRAINT `contest_files_ibfk_1` FOREIGN KEY (`file_id`) REFERENCES `internal_files` (`id`) ON UPDATE CASCADE,"
                        "  CONSTRAINT `contest_files_ibfk_2` FOREIGN KEY (`contest_id`) REFERENCES `contests` (`id`) ON DELETE CASCADE ON UPDATE CASCADE,"
                        "  CONSTRAINT `contest_files_ibfk_3` FOREIGN KEY (`creator`) REFERENCES `users` (`id`) ON DELETE SET NULL ON UPDATE CASCADE"
                        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb3 COLLATE=utf8mb3_bin"
                    ),
                    // clang-format on
                },
                {
                    // clang-format off
                    .create_table_sql = concat_tostr(
                        "CREATE TABLE `contest_entry_tokens` ("
                        "  `token` binary(", decltype(ContestEntryToken::token)::len, ") NOT NULL,"
                        "  `contest_id` bigint(20) unsigned NOT NULL,"
                        "  `short_token` binary(", decltype(ContestEntryToken::short_token)::value_type::len, ") DEFAULT NULL,"
                        "  `short_token_expiration` datetime DEFAULT NULL,"
                        "  PRIMARY KEY (`token`),"
                        "  UNIQUE KEY `contest_id` (`contest_id`),"
                        "  UNIQUE KEY `short_token` (`short_token`),"
                        "  CONSTRAINT `contest_entry_tokens_ibfk_1` FOREIGN KEY (`contest_id`) REFERENCES `contests` (`id`) ON DELETE CASCADE ON UPDATE CASCADE"
                        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb3 COLLATE=utf8mb3_bin"
                    ),
                    // clang-format on
                },
                {
                    // clang-format off
                    .create_table_sql = concat_tostr(
                        "CREATE TABLE `submissions` ("
                        "  `id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,"
                        "  `created_at` datetime NOT NULL,"
                        "  `file_id` bigint(20) unsigned NOT NULL,"
                        "  `user_id` bigint(20) unsigned DEFAULT NULL,"
                        "  `problem_id` bigint(20) unsigned NOT NULL,"
                        "  `contest_problem_id` bigint(20) unsigned DEFAULT NULL,"
                        "  `contest_round_id` bigint(20) unsigned DEFAULT NULL,"
                        "  `contest_id` bigint(20) unsigned DEFAULT NULL,"
                        "  `type` tinyint(3) unsigned NOT NULL,"
                        "  `language` tinyint(3) unsigned NOT NULL,"
                        "  `initial_final_candidate` tinyint(1) NOT NULL DEFAULT 0,"
                        "  `final_candidate` tinyint(1) NOT NULL DEFAULT 0,"
                        "  `problem_final` tinyint(1) NOT NULL DEFAULT 0,"
                        "  `contest_problem_final` tinyint(1) NOT NULL DEFAULT 0,"
                        // Used to color problems in the contest problem view
                        "  `contest_problem_initial_final` tinyint(1) NOT NULL DEFAULT 0,"
                        "  `initial_status` tinyint(3) unsigned NOT NULL,"
                        "  `full_status` tinyint(3) unsigned NOT NULL,"
                        "  `score` bigint(20) DEFAULT NULL,"
                        "  `last_judgment_began_at` datetime DEFAULT NULL,"
                        "  `initial_report` mediumblob NOT NULL," // TODO: rename to initial_judgement_protocol
                        "  `final_report` mediumblob NOT NULL," // TODO: rename to full_judgement_protocol
                        // Submissions API: all
                        "  PRIMARY KEY (`id`),"
                        // Submissions API: with type
                        "  KEY `type` (`type`,`id`),"
                        "  KEY `problem_final` (`problem_final`,`id`),"
                        "  KEY `contest_problem_final` (`contest_problem_final`,`id`),"
                        // Submissions API: with user_id
                        "  KEY `user_id` (`user_id`,`id`),"
                        // Submissions API: with user_id and type
                        "  KEY `user_id_type` (`user_id`,`type`,`id`),"
                        "  KEY `user_id_problem_final` (`user_id`,`problem_final`,`id`),"
                        "  KEY `user_id_contest_problem_final` (`user_id`,`contest_problem_final`,`id`),"
                        // Submissions API: with problem_id
                        "  KEY `problem_id` (`problem_id`,`id`),"
                        // Submissions API: with problem_id and type
                        "  KEY `problem_id_type` (`problem_id`,`type`,`id`),"
                        "  KEY `problem_id_problem_final` (`problem_id`,`problem_final`,`id`),"
                        "  KEY `problem_id_contest_problem_final` (`problem_id`,`contest_problem_final`,`id`),"
                        // Submissions API: with problem_id and user_id
                        "  KEY `problem_id_user_id` (`problem_id`,`user_id`,`id`),"
                        // Submissions API: with problem_id and user_id and type
                        "  KEY `problem_id_user_id_type` (`problem_id`,`user_id`,`type`,`id`),"
                        "  KEY `problem_id_user_id_problem_final` (`problem_id`,`user_id`,`problem_final`,`id`),"
                        "  KEY `problem_id_user_id_contest_problem_final` (`problem_id`,`user_id`,`contest_problem_final`,`id`),"
                        // Submissions API: with contest_id
                        "  KEY `contest_id` (`contest_id`,`id`),"
                        // Submissions API: with contest_id and type
                        "  KEY `contest_id_type` (`contest_id`,`type`,`id`),"
                        // Submissions API: with contest_id and type + Contest ranking API
                        "  KEY `contest_id_contest_problem_final` (`contest_id`,`contest_problem_final`,`id`),"
                        // Contest ranking API:
                        "  KEY `contest_id_contest_problem_initial_final` (`contest_id`,`contest_problem_initial_final`,`id`),"
                        // Submissions API: with contest_id and user_id
                        "  KEY `contest_id_user_id` (`contest_id`,`user_id`,`id`),"
                        // Submissions API: with contest_id and user_id and type
                        "  KEY `contest_id_user_id_type` (`contest_id`,`user_id`,`type`,`id`),"
                        "  KEY `contest_id_user_id_contest_problem_final` (`contest_id`,`user_id`,`contest_problem_final`,`id`),"
                        "  KEY `contest_id_user_id_contest_problem_initial_final` (`contest_id`,`user_id`,`contest_problem_initial_final`,`id`),"
                        // Submissions API: with contest_round_id
                        "  KEY `contest_round_id` (`contest_round_id`,`id`),"
                        // Submissions API: with contest_round_id and type
                        "  KEY `contest_round_id_type` (`contest_round_id`,`type`,`id`),"
                        // Submissions API: with contest_round_id and type + Contest round ranking API
                        "  KEY `contest_round_id_contest_problem_final` (`contest_round_id`,`contest_problem_final`,`id`),"
                        // Contest round ranking API:
                        "  KEY `contest_round_id_contest_problem_initial_final` (`contest_round_id`,`contest_problem_initial_final`,`id`),"
                        // Submissions API: with contest_round_id and user_id
                        "  KEY `contest_round_id_user_id` (`contest_round_id`,`user_id`,`id`),"
                        // Submissions API: with contest_round_id and user_id and type
                        "  KEY `contest_round_id_user_id_type` (`contest_round_id`,`user_id`,`type`,`id`),"
                        "  KEY `contest_round_id_user_id_contest_problem_final` (`contest_round_id`,`user_id`,`contest_problem_final`,`id`),"
                        "  KEY `contest_round_id_user_id_contest_problem_initial_final` (`contest_round_id`,`user_id`,`contest_problem_initial_final`,`id`),"
                        // Submissions API: with contest_problem_id
                        "  KEY `contest_problem_id` (`contest_problem_id`,`id`),"
                        // Submissions API: with contest_problem_id and type
                        "  KEY `contest_problem_id_type` (`contest_problem_id`,`type`,`id`),"
                        // Submissions API: with contest_problem_id and type + Contest problem ranking API
                        "  KEY `contest_problem_id_contest_problem_final` (`contest_problem_id`,`contest_problem_final`,`id`),"
                        // Contest problem ranking API:
                        "  KEY `contest_problem_id_contest_problem_initial_final` (`contest_problem_id`,`contest_problem_initial_final`,`id`),"
                        // Submissions API: with contest_problem_id and user_id
                        "  KEY `contest_problem_id_user_id` (`contest_problem_id`,`user_id`,`id`),"
                        // Submissions API: with contest_problem_id and user_id and type
                        "  KEY `contest_problem_id_user_id_type` (`contest_problem_id`,`user_id`,`type`,`id`),"
                        // Submissions API: with contest_problem_id and user_id and type + contest / contest round / contest problem API: view coloring
                        "  KEY `contest_problem_id_user_id_contest_problem_final` (`contest_problem_id`,`user_id`,`contest_problem_final`),"
                        "  KEY `contest_problem_id_user_id_contest_problem_initial_final` (`contest_problem_id`,`user_id`,`contest_problem_initial_final`),"
                        // Needed to efficiently select problem final submission
                        "  KEY `for_problem_final` (`final_candidate`,`user_id`,`problem_id`,`score` DESC,`full_status`,`id` DESC),"
                        // Needed to efficiently select contest problem final submission
                        "  KEY `for_contest_problem_final_latest` (`final_candidate`,`user_id`,`contest_problem_id`,`id` DESC),"
                        "  KEY `for_contest_problem_final_by_score` (`final_candidate`,`user_id`,`contest_problem_id`,`score` DESC,`full_status`,`id` DESC),"
                        // Needed to efficiently select contest problem initial final submission
                        "  KEY `for_contest_initial_problem_final_latest` (`initial_final_candidate`,`user_id`,`contest_problem_id`,`id` DESC),"
                        "  KEY `for_contest_initial_problem_final_by_initial_status` (`initial_final_candidate`,`user_id`,`contest_problem_id`,`initial_status`,`id` DESC),"
                        "  KEY `for_contest_initial_problem_final_by_score_and_initial_status` (`initial_final_candidate`,`user_id`,`contest_problem_id`,`score` DESC,`initial_status`,`id` DESC),"
                        "  KEY `for_contest_initial_problem_final_by_score_and_full_status` (`initial_final_candidate`,`user_id`,`contest_problem_id`,`score` DESC,`full_status`,`id` DESC),"
                        // For foreign keys
                        "  KEY `file_id` (`file_id`),"
                        "  CONSTRAINT `submissions_ibfk_1` FOREIGN KEY (`file_id`) REFERENCES `internal_files` (`id`) ON UPDATE CASCADE,"
                        "  CONSTRAINT `submissions_ibfk_2` FOREIGN KEY (`user_id`) REFERENCES `users` (`id`) ON DELETE CASCADE ON UPDATE CASCADE,"
                        "  CONSTRAINT `submissions_ibfk_3` FOREIGN KEY (`problem_id`) REFERENCES `problems` (`id`) ON DELETE CASCADE ON UPDATE CASCADE,"
                        "  CONSTRAINT `submissions_ibfk_4` FOREIGN KEY (`contest_problem_id`) REFERENCES `contest_problems` (`id`) ON DELETE CASCADE ON UPDATE CASCADE,"
                        "  CONSTRAINT `submissions_ibfk_5` FOREIGN KEY (`contest_round_id`) REFERENCES `contest_rounds` (`id`) ON DELETE CASCADE ON UPDATE CASCADE,"
                        "  CONSTRAINT `submissions_ibfk_6` FOREIGN KEY (`contest_id`) REFERENCES `contests` (`id`) ON DELETE CASCADE ON UPDATE CASCADE"
                        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb3 COLLATE=utf8mb3_bin"
                    ),
                    // clang-format on
                },
                {
                    // clang-format off
                    .create_table_sql = concat_tostr(
                        "CREATE TABLE `jobs` ("
                        "  `id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,"
                        "  `created_at` datetime NOT NULL,"
                        "  `creator` bigint(20) unsigned DEFAULT NULL,"
                        "  `type` tinyint(3) unsigned NOT NULL,"
                        "  `priority` tinyint(3) unsigned NOT NULL,"
                        "  `status` tinyint(3) unsigned NOT NULL,"
                        "  `aux_id` bigint(20) unsigned DEFAULT NULL,"
                        "  `aux_id_2` bigint(20) unsigned DEFAULT NULL,"
                        "  `log` mediumblob NOT NULL DEFAULT '',"
                        "  PRIMARY KEY (`id`),"
                        "  KEY `status` (`status`,`priority` DESC,`id`),"
                        "  KEY `type` (`type`,`aux_id`,`id` DESC),"
                        "  KEY `aux_id` (`aux_id`,`id` DESC),"
                        "  KEY `creator` (`creator`,`id` DESC),"
                        "  KEY `creator_2` (`creator`,`type`,`aux_id`,`id` DESC),"
                        "  KEY `creator_3` (`creator`,`aux_id`,`id` DESC)"
                        // Foreign keys cannot be used as we want to preserve information about who
                        // created the job and what the job was doing specifically
                        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb3 COLLATE=utf8mb3_bin"
                    ),
                    // clang-format on
                },
                {
                    // clang-format off
                    .create_table_sql = concat_tostr(
                        "CREATE TABLE `add_problem_jobs` ("
                        "  `id` bigint(20) unsigned NOT NULL,"
                        "  `file_id` bigint(20) unsigned NOT NULL,"
                        "  `visibility` tinyint(1) unsigned NOT NULL,"
                        "  `force_time_limits_reset` tinyint(1) NOT NULL,"
                        "  `ignore_simfile` tinyint(1) NOT NULL,"
                        "  `name` varbinary(", decltype(AddProblemJob::name)::max_len, ") NOT NULL,"
                        "  `label` varbinary(", decltype(AddProblemJob::label)::max_len, ") NOT NULL,"
                        "  `memory_limit_in_mib` bigint(20) unsigned DEFAULT NULL,"
                        "  `fixed_time_limit_in_ns` bigint(20) unsigned DEFAULT NULL,"
                        "  `reset_scoring` tinyint(1) NOT NULL,"
                        "  `look_for_new_tests` tinyint(1) NOT NULL,"
                        "  PRIMARY KEY (`id`),"
                        "  KEY `file_id` (`file_id`),"
                        "  CONSTRAINT `add_problem_jobs_ibfk_1` FOREIGN KEY (`id`) REFERENCES `jobs` (`id`) ON DELETE CASCADE ON UPDATE CASCADE,"
                        "  CONSTRAINT `add_problem_jobs_ibfk_2` FOREIGN KEY (`file_id`) REFERENCES `internal_files` (`id`) ON UPDATE CASCADE"
                        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb3 COLLATE=utf8mb3_bin"
                    ),
                    // clang-format on
                },
                {
                    // clang-format off
                    .create_table_sql = concat_tostr(
                        "CREATE TABLE `reupload_problem_jobs` ("
                        "  `id` bigint(20) unsigned NOT NULL,"
                        "  `file_id` bigint(20) unsigned NOT NULL,"
                        "  `force_time_limits_reset` tinyint(1) NOT NULL,"
                        "  `ignore_simfile` tinyint(1) NOT NULL,"
                        "  `name` varbinary(", decltype(AddProblemJob::name)::max_len, ") NOT NULL,"
                        "  `label` varbinary(", decltype(AddProblemJob::label)::max_len, ") NOT NULL,"
                        "  `memory_limit_in_mib` bigint(20) unsigned DEFAULT NULL,"
                        "  `fixed_time_limit_in_ns` bigint(20) unsigned DEFAULT NULL,"
                        "  `reset_scoring` tinyint(1) NOT NULL,"
                        "  `look_for_new_tests` tinyint(1) NOT NULL,"
                        "  PRIMARY KEY (`id`),"
                        "  KEY `file_id` (`file_id`),"
                        "  CONSTRAINT `reupload_problem_jobs_ibfk_1` FOREIGN KEY (`id`) REFERENCES `jobs` (`id`) ON DELETE CASCADE ON UPDATE CASCADE,"
                        "  CONSTRAINT `reupload_problem_jobs_ibfk_2` FOREIGN KEY (`file_id`) REFERENCES `internal_files` (`id`) ON UPDATE CASCADE"
                        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb3 COLLATE=utf8mb3_bin"
                    ),
                    // clang-format on
                },
                {
                    // clang-format off
                    .create_table_sql = concat_tostr(
                        "CREATE TABLE `merge_problems_jobs` ("
                        "  `id` bigint(20) unsigned NOT NULL,"
                        "  `rejudge_transferred_submissions` tinyint(1) NOT NULL,"
                        "  PRIMARY KEY (`id`),"
                        "  CONSTRAINT `merge_problems_jobs_ibfk_1` FOREIGN KEY (`id`) REFERENCES `jobs` (`id`) ON DELETE CASCADE ON UPDATE CASCADE"
                        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb3 COLLATE=utf8mb3_bin"
                    ),
                    // clang-format on
                },
                {
                    // clang-format off
                    .create_table_sql = concat_tostr(
                        "CREATE TABLE `change_problem_statement_jobs` ("
                        "  `id` bigint(20) unsigned NOT NULL,"
                        "  `new_statement_file_id` bigint(20) unsigned NOT NULL,"
                        "  `path_for_new_statement` varbinary(", decltype(change_problem_statement_jobs::ChangeProblemStatementJob::path_for_new_statement)::max_len, ") NOT NULL,"
                        "  PRIMARY KEY (`id`),"
                        "  KEY `new_statement_file_id` (`new_statement_file_id`),"
                        "  CONSTRAINT `change_problem_statement_jobs_ibfk_1` FOREIGN KEY (`id`) REFERENCES `jobs` (`id`) ON DELETE CASCADE ON UPDATE CASCADE,"
                        "  CONSTRAINT `change_problem_statement_jobs_ibfk_2` FOREIGN KEY (`new_statement_file_id`) REFERENCES `internal_files` (`id`) ON UPDATE CASCADE"
                        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb3 COLLATE=utf8mb3_bin"
                    ),
                    // clang-format on
                },
                {
                    // clang-format off
                    .create_table_sql = concat_tostr(
                        "CREATE TABLE `schema_subversion_1` ("
                        "  `x` bit(1) NOT NULL"
                        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb3 COLLATE=utf8mb3_bin"
                    ),
                    // clang-format on
                },
            },
    };
    [[maybe_unused]] static int asserts = [] {
        throw_assert(TABLES_NUM == schema.table_schemas.size());
        // Assert that number of column of each table matches the declared value
        constexpr auto table_name_to_column_number = std::array{
            std::pair{std::string_view{"add_problem_jobs"}, AddProblemJob::COLUMNS_NUM},
            std::pair{
                std::string_view{"change_problem_statement_jobs"},
                ChangeProblemStatementJob::COLUMNS_NUM
            },
            std::pair{std::string_view{"contest_entry_tokens"}, ContestEntryToken::COLUMNS_NUM},
            std::pair{std::string_view{"contest_files"}, ContestFile::COLUMNS_NUM},
            std::pair{std::string_view{"contest_problems"}, ContestProblem::COLUMNS_NUM},
            std::pair{std::string_view{"contest_rounds"}, ContestRound::COLUMNS_NUM},
            std::pair{std::string_view{"contest_users"}, ContestUser::COLUMNS_NUM},
            std::pair{std::string_view{"contests"}, Contest::COLUMNS_NUM},
            std::pair{std::string_view{"internal_files"}, InternalFile::COLUMNS_NUM},
            std::pair{std::string_view{"jobs"}, Job::COLUMNS_NUM},
            std::pair{std::string_view{"merge_problems_jobs"}, MergeProblemsJob::COLUMNS_NUM},
            std::pair{std::string_view{"problem_tags"}, ProblemTag::COLUMNS_NUM},
            std::pair{std::string_view{"problems"}, Problem::COLUMNS_NUM},
            std::pair{std::string_view{"reupload_problem_jobs"}, ReuploadProblemJob::COLUMNS_NUM},
            std::pair{std::string_view{"schema_subversion_1"}, size_t{1}},
            std::pair{std::string_view{"sessions"}, Session::COLUMNS_NUM},
            std::pair{std::string_view{"submissions"}, Submission::COLUMNS_NUM},
            std::pair{std::string_view{"users"}, User::COLUMNS_NUM},
        };
        static_assert(TABLES_NUM == table_name_to_column_number.size());
        static_assert(is_sorted(table_name_to_column_number));
        for (const auto& table_schema : schema.table_schemas) {
            auto table_name = [&] {
                auto beg = table_schema.create_table_sql.find('`');
                auto end = table_schema.create_table_sql.find('`', beg + 1);
                return std::string_view{
                    table_schema.create_table_sql.data() + beg + 1, end - beg - 1
                };
            }();
            auto it = std::lower_bound(
                table_name_to_column_number.begin(),
                table_name_to_column_number.end(),
                std::pair{table_name, size_t{0}}
            );
            if (it == table_name_to_column_number.end() || it->first != table_name) {
                THROW("missing table ", table_name, " in table_name_to_column_number");
            }
            size_t rows = 0;
            for (size_t beg = 0;;) {
                auto pos = table_schema.create_table_sql.find("  `", beg);
                if (pos == std::string::npos) {
                    break;
                }
                ++rows;
                beg = pos + 1;
            }
            if (it->second != rows) {
                THROW(
                    "invalid number of rows in table ",
                    table_name,
                    ": declared ",
                    it->second,
                    ", actual ",
                    rows
                );
            }
        }
        return 0;
    }();

    return schema;
}

string normalized(const TableSchema& table_schema) {
    // First split table schema
    std::smatch parts;
    std::regex_match(
        table_schema.create_table_sql,
        parts,
        std::regex{R"((CREATE TABLE .*?\()(.*)(\) ENGINE=.*))"}
    );
    auto create_table_line = parts[1].str();
    auto inside_lines = parts[2].str();
    auto end_line = parts[3].str();
    // Split table schema to rows
    vector<string> rows;
    rows.emplace_back(create_table_line);
    if (!inside_lines.empty()) {
        std::string_view unparsed = inside_lines;
        for (;;) {
            auto pos = unparsed.find(",  ");
            if (pos == std::string_view::npos) {
                auto str = string{unparsed};
                // Add comma to the last row of table definition, because it may become an
                // earlier row during sorting
                if (!has_suffix(str, ",")) {
                    str += ",";
                }
                rows.emplace_back(str);
                break;
            }
            rows.emplace_back(unparsed.substr(0, pos + 1));
            unparsed.remove_prefix(pos + 1);
        }
    }
    rows.emplace_back(end_line);
    // Sort table schema rows
    auto row_to_comparable = [](StringView row) -> std::pair<int, StringView> {
        if (has_prefix(row, "CREATE TABLE ")) {
            return {0, row};
        }
        if (has_prefix(row, "  `")) {
            return {1, ""}; // preserve column order
        }
        if (has_prefix(row, "  PRIMARY KEY ")) {
            return {2, row};
        }
        if (has_prefix(row, "  UNIQUE KEY ")) {
            return {3, row};
        }
        if (has_prefix(row, "  KEY ")) {
            return {4, row};
        }
        if (has_prefix(row, "  CONSTRAINT ")) {
            return {5, row};
        }
        if (has_prefix(row, ")")) {
            return {6, row};
        }
        THROW("BUG: unknown prefix");
    };
    std::stable_sort(rows.begin(), rows.end(), [&](StringView a, StringView b) {
        return row_to_comparable(a) < row_to_comparable(b);
    });
    if (rows.size() > 2) {
        // Remove trailing ',' from the last row of table definition
        rows[rows.size() - 2].pop_back();
    }

    string res;
    for (const auto& row : rows) {
        res += row;
        res += '\n';
    }
    res.pop_back();
    return res;
}

string normalized(const DbSchema& db_schema) {
    std::map<StringView, size_t> our_tables_to_idx;
    for (auto [idx, table_name] : enumerate_view(get_tables())) {
        our_tables_to_idx.emplace(table_name, idx);
    }

    vector<std::pair<size_t, string>> table_schemas;
    table_schemas.reserve(db_schema.table_schemas.size());
    for (const auto& table_schema : db_schema.table_schemas) {
        auto normalized_table_schema = normalized(table_schema);
        // Extract table name
        size_t beg = normalized_table_schema.find('`');
        size_t end = normalized_table_schema.find('`', beg + 1);
        auto table_name = normalized_table_schema.substr(beg + 1, end - beg - 1);
        // Add to table_schemas
        auto it = our_tables_to_idx.find(table_name);
        auto idx = it == our_tables_to_idx.end() ? our_tables_to_idx.size() : it->second;
        table_schemas.emplace_back(idx, std::move(normalized_table_schema));
    }
    std::sort(table_schemas.begin(), table_schemas.end());

    string res;
    for (const auto& p : table_schemas) {
        res += p.second;
        res += ";\n";
    }
    if (!table_schemas.empty()) {
        res.pop_back();
    }
    return res;
}

vector<string> get_all_table_names(mysql::Connection& mysql) {
    vector<string> table_names;
    auto stmt = mysql.execute("SHOW TABLES");
    string table_name;
    stmt.res_bind(table_name);
    while (stmt.next()) {
        table_names.emplace_back(table_name);
    }
    return table_names;
}

DbSchema get_db_schema(mysql::Connection& mysql) {
    auto table_names = get_all_table_names(mysql);
    DbSchema db_schema;
    db_schema.table_schemas.reserve(table_names.size());

    for (const auto& table_name : table_names) {
        auto stmt =
            mysql.execute(sql::SqlWithParams{concat_tostr("SHOW CREATE TABLE `", table_name, '`')});
        std::string tmp;
        std::string create_table_sql;
        stmt.res_bind(tmp, create_table_sql);
        throw_assert(stmt.next());
        // Remove AUTOINCREMENT=
        create_table_sql = std::regex_replace(
            create_table_sql, std::regex{R"((\n\).*) AUTO_INCREMENT=\w+)"}, "$1"
        );
        // Remove newlines
        create_table_sql = std::regex_replace(create_table_sql, std::regex{"\n"}, "");

        db_schema.table_schemas.emplace_back(TableSchema{.create_table_sql = create_table_sql});
    }
    return db_schema;
}

} // namespace sim::db
