#include "sim.h"

#include <sim/random.hh>

void Sim::api_contest_entry_token() {
	STACK_UNWINDING_MARK;

	if (not session_is_open)
		return api_error403();

	StringView next_arg = url_args.extractNextArg();
	if (next_arg.empty()) {
		return api_error404();
	} else if (next_arg[0] == '=') {
		StringView token = next_arg.substr(1);
		static_assert(CONTEST_ENTRY_TOKEN_LEN != CONTEST_ENTRY_SHORT_TOKEN_LEN,
		              "These cannot be equal because this would cause conflict "
		              "in selecting the token in the below query");
		auto stmt = mysql.prepare("SELECT c.id, c.name "
		                          "FROM contest_entry_tokens t "
		                          "JOIN contests c ON c.id=t.contest_id "
		                          "WHERE t.token=?"
		                          " OR (t.short_token=?"
		                          " AND ?<=t.short_token_expiration)");
		stmt.bindAndExecute(token, token, mysql_date());

		decltype(contests_cid) contest_id;
		InplaceBuff<CONTEST_NAME_MAX_LEN> contest_name;
		stmt.res_bind_all(contest_id, contest_name);

		if (not stmt.next())
			return api_error404();

		next_arg = url_args.extractNextArg();
		if (next_arg == "use") {
			return api_contest_entry_token_use_to_enter_contest(contest_id);
		} else if (not next_arg.empty())
			return api_error400();

		append("[{\"fields\":["
		       "\"contest_id\","
		       "\"contest_name\""
		       "]},\n",
		       contest_id, ',', jsonStringify(contest_name), ']');

	} else if (next_arg[0] == 'c') {
		StringView contest_id = next_arg.substr(1);
		ContestPermissions cperms = contests_get_permissions(contest_id)
		                               .value_or(ContestPermissions::NONE);
		if (uint(~cperms & ContestPermissions::MANAGE_CONTEST_ENTRY_TOKEN))
			return api_error404(); // To not reveal that the contest exists

		next_arg = url_args.extractNextArg();
		if (next_arg == "add")
			return api_contest_entry_token_add(contest_id);
		else if (next_arg == "add_short")
			return api_contest_entry_token_short_add(contest_id);
		else if (next_arg == "regen")
			return api_contest_entry_token_regen(contest_id);
		else if (next_arg == "regen_short")
			return api_contest_entry_token_short_regen(contest_id);
		else if (next_arg == "delete")
			return api_contest_entry_token_delete(contest_id);
		else if (next_arg == "delete_short")
			return api_contest_entry_token_short_delete(contest_id);
		else if (not next_arg.empty())
			return api_error400();

		auto stmt =
		   mysql.prepare("SELECT token, short_token, short_token_expiration "
		                 "FROM contest_entry_tokens WHERE contest_id=?");
		stmt.bindAndExecute(contest_id);

		InplaceBuff<CONTEST_ENTRY_TOKEN_LEN> token;
		MySQL::Optional<InplaceBuff<CONTEST_ENTRY_SHORT_TOKEN_LEN>> short_token;
		MySQL::Optional<InplaceBuff<20>> short_token_expiration;
		stmt.res_bind_all(token, short_token, short_token_expiration);

		append("[{\"fields\":["
		       "\"token\","
		       "\"short_token\","
		       "\"short_token_expiration_date\""
		       "]},\n");

		if (stmt.next()) {
			append(jsonStringify(token), ',');

			if (not short_token.has_value() or
			    not short_token_expiration.has_value() or
			    short_token_expiration.value() <
			       intentionalUnsafeStringView(mysql_date())) {
				append("null,null]");
			} else {
				append(jsonStringify(short_token.value()), ",\"",
				       short_token_expiration.value(), "\"]");
			}

		} else {
			append("null,null,null]");
		}

	} else {
		api_error400();
	}
}

void Sim::api_contest_entry_token_add(StringView contest_id) {
	STACK_UNWINDING_MARK;

	auto transaction = mysql.start_transaction();

	auto stmt =
	   mysql.prepare("SELECT 1 FROM contest_entry_tokens WHERE contest_id=?");
	stmt.bindAndExecute(contest_id);
	if (stmt.next())
		return api_error400("Contest already has an entry token");

	stmt = mysql.prepare(
	   "INSERT IGNORE contest_entry_tokens(token, contest_id, short_token, "
	   "short_token_expiration) VALUES(?, ?, NULL, NULL)");
	std::string token;
	do {
		token = generate_random_token(CONTEST_ENTRY_TOKEN_LEN);
		stmt.bindAndExecute(token, contest_id);
	} while (stmt.affected_rows() == 0);

	transaction.commit();
}

void Sim::api_contest_entry_token_regen(StringView contest_id) {
	STACK_UNWINDING_MARK;

	auto transaction = mysql.start_transaction();

	auto stmt =
	   mysql.prepare("SELECT 1 FROM contest_entry_tokens WHERE contest_id=?");
	stmt.bindAndExecute(contest_id);
	if (not stmt.next())
		return api_error400("Contest does not have an entry token");

	stmt = mysql.prepare(
	   "UPDATE IGNORE contest_entry_tokens SET token=? WHERE contest_id=?");
	std::string new_token;
	do {
		new_token = generate_random_token(CONTEST_ENTRY_TOKEN_LEN);
		stmt.bindAndExecute(new_token, contest_id);
	} while (stmt.affected_rows() == 0);

	transaction.commit();
}

void Sim::api_contest_entry_token_delete(StringView contest_id) {
	STACK_UNWINDING_MARK;

	mysql.prepare("DELETE FROM contest_entry_tokens WHERE contest_id=?")
	   .bindAndExecute(contest_id);
}

void Sim::api_contest_entry_token_short_add(StringView contest_id) {
	STACK_UNWINDING_MARK;

	auto transaction = mysql.start_transaction();

	auto stmt = mysql.prepare("SELECT short_token_expiration FROM "
	                          "contest_entry_tokens WHERE contest_id=?");
	stmt.bindAndExecute(contest_id);

	MySQL::Optional<InplaceBuff<20>> short_token_expiration;
	stmt.res_bind_all(short_token_expiration);
	if (not stmt.next())
		return api_error400("Contest does not have an entry token");
	if (short_token_expiration.has_value() and
	    short_token_expiration.value() >
	       intentionalUnsafeStringView(mysql_date()))
		return api_error400("Contest already has a short entry token");

	stmt =
	   mysql.prepare("UPDATE IGNORE contest_entry_tokens SET short_token=?, "
	                 "short_token_expiration=? WHERE contest_id=?");
	std::string new_token;
	auto exp_date =
	   mysql_date(time(nullptr) + CONTEST_ENTRY_SHORT_TOKEN_MAX_LIFETIME);
	do {
		new_token = generate_random_token(CONTEST_ENTRY_SHORT_TOKEN_LEN);
		stmt.bindAndExecute(new_token, exp_date, contest_id);
	} while (stmt.affected_rows() == 0);

	transaction.commit();
}

void Sim::api_contest_entry_token_short_regen(StringView contest_id) {
	STACK_UNWINDING_MARK;

	auto transaction = mysql.start_transaction();

	auto stmt = mysql.prepare("SELECT short_token_expiration FROM "
	                          "contest_entry_tokens WHERE contest_id=?");
	stmt.bindAndExecute(contest_id);

	MySQL::Optional<InplaceBuff<20>> short_token_expiration;
	stmt.res_bind_all(short_token_expiration);
	if (not stmt.next())
		return api_error400("Contest does not have an entry token");
	if (not short_token_expiration.has_value() or
	    short_token_expiration.value() <=
	       intentionalUnsafeStringView(mysql_date()))
		return api_error400("Contest does not have a short entry token");

	stmt =
	   mysql.prepare("UPDATE IGNORE contest_entry_tokens SET short_token=?, "
	                 "short_token_expiration=? WHERE contest_id=?");
	std::string new_token;
	auto exp_date =
	   mysql_date(time(nullptr) + CONTEST_ENTRY_SHORT_TOKEN_MAX_LIFETIME);
	do {
		new_token = generate_random_token(CONTEST_ENTRY_SHORT_TOKEN_LEN);
		stmt.bindAndExecute(new_token, exp_date, contest_id);
	} while (stmt.affected_rows() == 0);

	transaction.commit();
}

void Sim::api_contest_entry_token_short_delete(StringView contest_id) {
	STACK_UNWINDING_MARK;

	auto stmt =
	   mysql.prepare("UPDATE contest_entry_tokens SET short_token=NULL, "
	                 "short_token_expiration=NULL WHERE contest_id=?");
	stmt.bindAndExecute(contest_id);
}

void Sim::api_contest_entry_token_use_to_enter_contest(StringView contest_id) {
	STACK_UNWINDING_MARK;

	auto stmt =
	   mysql.prepare("INSERT IGNORE contest_users(user_id, contest_id, mode) "
	                 "VALUES(?, ?, " CU_MODE_CONTESTANT_STR ")");
	stmt.bindAndExecute(session_user_id, contest_id);
	if (stmt.affected_rows() == 0)
		return api_error400("You already participate in the contest");
}
