#pragma once

#include "errors.h"
#include "template.h"

class Jobs : virtual protected Template, virtual protected Errors {
protected:
	Jobs() = default;

	Jobs(const Jobs&) = delete;
	Jobs(Jobs&&) = delete;
	Jobs& operator=(const Jobs&) = delete;
	Jobs& operator=(Jobs&&) = delete;

	virtual ~Jobs() = default;

private:
	enum Permissions : uint {
		PERM_NONE = 0,
		PERM_VIEW = 1,
		PERM_VIEW_ALL = 2,
		PERM_CANCEL = 4
	};

	std::string job_id;
	Permissions perms;

	Permissions getPermissions(const std::string& owner_id);

	void jobsTemplate(const StringView& title,
		const StringView& styles = {}, const StringView& scripts = {})
	{
		baseTemplate(title, concat("body{margin-left:32px}", styles), scripts);
	}

protected:
	/// Main Jobs handler
	void handle();

private:
	void job();

	void cancelJob();
};
