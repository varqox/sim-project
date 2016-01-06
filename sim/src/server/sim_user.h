#pragma once

class Sim::User {
private:
	Sim& sim_;

	explicit User(Sim& sim) : sim_(sim) {}

	User(const User&) = delete;

	User(User&& u) : sim_(u.sim_) {}

	User& operator=(const User&) = delete;

	User& operator=(User&&) = delete;

	~User() {}

	struct Data;

	class TemplateWithMenu;

	// Pages
	void login();

	void logout();

	void signUp();

	void changePassword(Data& data);

	void editProfile(Data& data);

	void deleteAccount(Data& data);

public:
	/**
 	 * @brief Main User handler
	 */
	void handle();

	friend Sim::Sim();
	friend Sim::~Sim();
	friend server::HttpResponse Sim::handle(std::string client_ip,
			const server::HttpRequest& req);
};
