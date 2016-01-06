#pragma once

#include "sim.h"

class Sim::Session {
public:
	enum State : uint8_t { OK, FAIL, CLOSED };
	std::string user_id, data, username;
	unsigned user_type = 0; // 0 - admin, 1 - teacher, 2 - normal

private:
	static constexpr int SESSION_ID_LENGTH = 30;
	static constexpr int SESSION_MAX_LIFETIME = 7 * 24 * 60 * 60; // 7 days (in
	                                                              // seconds)
	Sim& sim_;
	State state_;
	std::string id_;

	explicit Session(Sim& sim) : sim_(sim), state_(CLOSED), id_() {}

	Session(const Session&) = delete;

	Session(Session&& s) : user_id(std::move(s.user_id)),
			data(std::move(s.data)), username(std::move(s.username)),
			user_type(s.user_type), sim_(s.sim_), state_(s.state_),
			id_(std::move(s.id_)) {
		s.state_ = CLOSED;
	}

	Session& operator=(const Session&) = delete;

	Session& operator=(Session&& s) = delete;

	static std::string generateId();

	~Session() { close(); }

public:
	State open();

	State create(const std::string& _user_id);

	State state() const { return state_; }

	void destroy();

	void close();

	friend Sim::Sim();
	friend Sim::~Sim();
};
