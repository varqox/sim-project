#pragma once

#include "string.h"

#include <algorithm>
#include <vector>

// TODO: Try to make constexpr constructor (that sorts in constexpr)
class SyscallNameSet {
	typedef std::pair<int, CStringView> SyscallInfo;
	std::vector<SyscallInfo> t;

public:
	SyscallNameSet(std::vector<SyscallInfo> vec) : t{std::move(vec)} {
		std::sort(t.begin(), t.end());
	}

	SyscallNameSet(std::initializer_list<SyscallInfo> initl)
		: t{std::move(initl)}
	{
		std::sort(t.begin(), t.end());
	}

	SyscallNameSet(const SyscallNameSet&) = default;
	SyscallNameSet(SyscallNameSet&&) = default;
	SyscallNameSet& operator=(const SyscallNameSet&) = default;
	SyscallNameSet& operator=(SyscallNameSet&&) = default;

	/**
	 * @brief Returns syscall name
	 *
	 * @param syscall_no syscall number
	 *
	 * @return If there is no syscall in t returns empty string, otherwise
	 *   corresponding syscall name is returned
	 */
	CStringView operator[](int syscall_no) const noexcept {
		if (t.empty())
			return {};

		unsigned beg = 0, end = t.size() - 1;
		while (beg < end) {
			int mid = (beg + end) >> 1;
			if (t[mid].first < syscall_no)
				beg = mid + 1;
			else
				end = mid;
		}

		return (t[beg].first == syscall_no ? t[beg].second : CStringView{});
	};
};

extern SyscallNameSet x86_syscall_name; // 32 bit syscalls
extern SyscallNameSet x86_64_syscall_name; // 64 bit syscalls
