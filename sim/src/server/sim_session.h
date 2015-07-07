#pragma once

#include "sim.h"

class Sim::Session {
public:
	enum State { OK, FAIL, CLOSED };
	std::string user_id, data;

private:
	static const int SESSION_MAX_LIFETIME = 24 * 60 * 60; // in sec

	Session(const Session&);
	Session& operator=(const Session&);

	Sim& sim_;
	State state_;
	std::string id_;

	explicit Session(Sim& sim) : user_id(), data(), sim_(sim), state_(CLOSED),
		id_() {}

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
