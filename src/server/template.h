#pragma once

#include "session.h"

#include <simlib/utilities.h>

class Template : virtual protected Session {
private:
	void endTemplate();

protected:
	Template() = default;

	Template(const Template&) = delete;
	Template(Template&&) = delete;
	Template& operator=(const Template&) = delete;
	Template& operator=(Template&&) = delete;

	virtual ~Template() = default;

	class TemplateEnder {
		Template* templ;
	public:
		TemplateEnder(const TemplateEnder&) = delete;

		TemplateEnder(TemplateEnder&& te) : templ(te.templ) {
			te.templ = nullptr;
		}

		TemplateEnder& operator=(const TemplateEnder&) = delete;

		TemplateEnder& operator=(TemplateEnder&& te) {
			templ = te.templ;
			te.templ = nullptr;
			return *this;
		}

		TemplateEnder(Template& templ1) : templ(&templ1) {}

		~TemplateEnder() {
			if (templ)
				templ->endTemplate();
		}
	};

	TemplateEnder baseTemplate(const StringView& title,
		const StringView& styles = {}, const StringView& scripts = {});

	template<class... Args>
	Template& append(Args&&... args) {
		back_insert(resp.content, std::forward<Args>(args)...);
		return *this;
	}
};
