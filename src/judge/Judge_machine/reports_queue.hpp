#include <string>

#ifdef DEBUG
#include <ostream>
#endif

#pragma once

namespace reports_queue
{
	enum report_status{OK, ERROR, C_ERROR, WAITING};

	inline std::string to_str(report_status st)
	{
		switch(st)
		{
			case OK: return "ok";
			case ERROR: return "error";
			case C_ERROR: return "c_error";
			case WAITING: return "waiting";
		}
		return "";
	}

	class report
	{
	private:
		std::string _id, _task_id;

	public:
		report(const std::string& nid, const std::string& ntid): _id(nid), _task_id(ntid)
		{}

		const std::string& id() const
		{return _id;}

		const std::string& task_id() const
		{return _task_id;}

		void set(report_status st, int points) const;
	};

	bool empty();
	void pop();
	report extract();
	const report& front();
}

#ifdef DEBUG
std::ostream& operator<<(std::ostream& os, const reports_queue::report& r);
#endif