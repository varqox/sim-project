#pragma once

#include <cppconn/prepared_statement.h>
#include <mysql_connection.h>
#include <simlib/memory.h>

namespace DB {

class Result {
private:
	std::unique_ptr<sql::ResultSet> res_;

	explicit Result(sql::ResultSet* res) : res_(res) {}

public:
	Result() = default;

	Result(const Result&) = delete;

	Result(Result&& ps)
		: res_(std::move(ps.res_)) {}

	Result& operator=(const Result&) = delete;

	Result& operator=(Result&& ps) {
		res_ = std::move(ps.res_);
		return *this;
	}

	~Result() = default;

	sql::ResultSet* impl() { return res_.get(); }

	bool next() { return res_->next(); }

	bool previous() { return res_->previous(); }

	size_t rowCount() const { return res_->rowsCount(); }

	bool isNull(uint index) const { return res_->isNull(index); }

	bool getBool(uint index) const { return res_->getBoolean(index); }

	int32_t getInt(uint index) const { return res_->getInt(index); }

	uint32_t getUInt(uint index) const { return res_->getUInt(index); }

	int64_t getInt64(uint index) const { return res_->getInt64(index); }

	uint64_t getUInt64(uint index) const { return res_->getUInt64(index); }

	long double getDouble(uint index) const { return res_->getDouble(index); }

	std::string getString(uint index) const { return res_->getString(index); }

	std::string operator[](uint index) const { return getString(index); }

	friend class Statement;
	friend class Connection;
};

class Statement {
private:
	std::unique_ptr<sql::PreparedStatement> pstmt_;

	explicit Statement(sql::PreparedStatement* pstmt) : pstmt_(pstmt) {}

public:
	Statement() = default;

	Statement(const Statement&) = delete;

	Statement(Statement&& ps)
		: pstmt_(std::move(ps.pstmt_)) {}

	Statement& operator=(const Statement&) = delete;

	Statement& operator=(Statement&& ps) {
		pstmt_ = std::move(ps.pstmt_);
		return *this;
	}

	~Statement() = default;

	sql::Statement* impl() { return pstmt_.get(); }

	void clearParams() { pstmt_->clearParameters(); }

	void setBool(uint index, bool val) { pstmt_->setBoolean(index, val); }

	void setInt(uint index, int32_t val) { pstmt_->setInt(index, val); }

	void setUInt(uint index, uint32_t val) { pstmt_->setUInt(index, val); }

	void setInt64(uint index, int64_t val) { pstmt_->setInt64(index, val); }

	void setUInt64(uint index, uint64_t val) { pstmt_->setUInt64(index, val); }

	void setString(uint index, const std::string& val) {
		pstmt_->setString(index, val);
	}

	void setNull(uint index) { pstmt_->setNull(index, 0); }

	int executeUpdate() { return pstmt_->executeUpdate(); }

	Result executeQuery() { return Result(pstmt_->executeQuery()); }

	friend class Connection;
};

class Connection {
private:
	std::unique_ptr<sql::Connection> conn_;
	std::string host_, user_, password_, database_;

	void connect();

public:
	Connection() = default;

	Connection(const std::string& host, const std::string& user,
			const std::string& password, const std::string& database);

	Connection(const Connection&) = delete;

	Connection(Connection&& conn) : conn_(std::move(conn.conn_)),
		host_(std::move(conn.host_)), user_(std::move(conn.user_)),
		password_(std::move(conn.password_)),
		database_(std::move(conn.database_)) {}

	Connection& operator=(const Connection&) = delete;

	Connection& operator=(Connection&& conn) {
		conn_ = std::move(conn.conn_);
		host_ = std::move(conn.host_);
		user_ = std::move(conn.user_);
		password_ = std::move(conn.password_);
		database_ = std::move(conn.database_);

		return *this;
	}

	~Connection() {}

	sql::Connection* impl() {
		if (conn_->isClosed())
			connect();
		return conn_.get();
	}

	Statement prepare(const std::string& query) noexcept(false) {
		return Statement(impl()->prepareStatement(query));
	}

	int executeUpdate(const std::string& update_query) noexcept(false) {
		std::unique_ptr<sql::Statement> stmt(impl()->createStatement());
		return stmt->executeUpdate(update_query);
	}

	Result executeQuery(const std::string& query) noexcept(false) {
		std::unique_ptr<sql::Statement> stmt(impl()->createStatement());
		return Result(stmt->executeQuery(query));
	}
};

/**
 * @brief Creates Connection using file @p filename
 * @details File format: "USER\nPASSWORD\nDATABASE\nHOST"
 *
 * @param filename file to load credentials from
 *
 * @return Connection object
 *
 * @errors On error throws std::runtime_error
 */
Connection createConnectionUsingPassFile(const std::string& filename)
	noexcept(false);

} // namespace DB
