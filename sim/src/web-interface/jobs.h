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
		PERM_VIEW = 1, // allowed to view the job
		PERM_VIEW_ALL = 2, // allowed to view all the jobs
		PERM_CANCEL = 4,
		PERM_RESTART = 8
	};

	std::string job_id;
	Permissions perms = PERM_NONE;

	Permissions getPermissions(const std::string& owner_id,
		JobQueueStatus job_status);

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

	void downloadReport(std::string data_preview);

	void downloadUploadedPackage();

	void cancelJob();

	void restartJob(JobQueueType job_type, StringView job_info);
};
