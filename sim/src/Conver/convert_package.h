#pragma once

#include "conver.h"

struct TestNameCompatator {
	/**
	 * @brief Extracts tag from @p str
	 * @details Test name: str tag = str group_id test_id = .*[0-9]+[a-z]*
	 * e.g. "test1abc" -> ("1", "abc")
	 *
	 * @param str string from which the tag will be extracted
	 * @return (group_id, test_id)
	 */
	static std::pair<std::string, std::string>
	extractTag(const std::string& str);

	bool operator()(const std::string& a, const std::string& b) const {
		std::pair<std::string, std::string> tag_a = extractTag(a),
			tag_b = extractTag(b);

		if (tag_a.second == "ocen") {
			if (tag_b.second == "ocen")
				return StrNumCompare()(tag_a.first, tag_b.first);

			return tag_b.first != "0";
		}
		if (tag_b.second == "ocen")
			return tag_a.first != "0";

		return tag_a.first == tag_b.first ? tag_a.second < tag_b.second :
			StrNumCompare()(tag_a.first, tag_b.first);
	}
};

/**
 * @brief Converts package @p tmp_package to @p sim_package
 *
 * @param tmp_package input package which will be converted (main folder)
 * @param out_package output pathname in which the converted package will be
 * placed
 * @return 0 on success, -1 on error
 */
int convertPackage(std::string tmp_package, std::string out_package);
