#pragma once

#include "sim.h"

class Sim::Session {
public:
	enum State { OK, FAIL, CLOSED };
	std::string user_id, data, username;
	unsigned user_type; // 0 - admin, 1 - teacher, 2 - normal

private:
	static const int SESSION_MAX_LIFETIME = 7 * 24 * 60 * 60; // 7 days (in sec)

	Session(const Session&);
	Session& operator=(const Session&);

	Sim& sim_;
	State state_;
	std::string id_;

	explicit Session(Sim& sim) : user_id(), data(), username(), user_type(),
		sim_(sim), state_(CLOSED), id_() {}

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
