#pragma once

#include "session.h"

#include <simlib/utilities.h>

class Template : virtual protected Session {
private:
	bool template_began = false;

protected:
	Template() = default;

	Template(const Template&) = delete;
	Template(Template&&) = delete;
	Template& operator=(const Template&) = delete;
	Template& operator=(Template&&) = delete;

	virtual ~Template() = default;

	void baseTemplate(const StringView& title, const StringView& styles = {},
		const StringView& scripts = {});

	void endTemplate();

	void reset() noexcept { template_began = false; }

	template<class... Args>
	Template& append(Args&&... args) {
		back_insert(resp.content, std::forward<Args>(args)...);
		return *this;
	}
};
