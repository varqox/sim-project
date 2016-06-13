#pragma once

#include "string.h"

#include <deque>

class AhoCorasick {
public:
	typedef unsigned uint;

	struct Node {
		uint patt_id = 0; // pattern id which ends in this node or zero if such
		             // does not exist
		uint fail = 0; // fail edge
		uint next_pattern = 0; // id of the longest pattern which is prefix of
		                       // one ending in this node or zero if such
		                       // does not exist
		std::vector<std::pair<char, uint>> sons; // sons (sorted array - for
		                                         // small alphabets it's the
		                                         // most efficient option)

		// Returns id of son @p c or 0 if such does not exist
		uint operator[](char c) const noexcept;
	};

private:
	std::vector<Node> nodes = {Node{}}; // root

	// Returns id of son @p c (creates one if such does not exist)
	uint son(uint id, char c);

public:
	AhoCorasick() = default;

	AhoCorasick(const AhoCorasick&) = default;

	AhoCorasick(AhoCorasick&&) = default;

	AhoCorasick& operator=(const AhoCorasick&) = default;

	AhoCorasick& operator=(AhoCorasick&&) = default;

	const Node& node(int i) const { return nodes[i]; }

	// Adds pattern s to structure and sets (override if such pattern has
	// already existed) its id to @p id, @p id equal to 0 disables the pattern,
	// pattern cannot be empty
	void addPattern(const StringView& patt, uint id);

	// Returns id of node in which pattern @p str ends or 0 if such does not
	// exist
	uint findNode(const StringView& str) const;

	// Returns id of the pattern which ends in node @p node_id
	uint pattId(uint node_id) const { return nodes[node_id].patt_id; }

	// Returns id of next pattern node for pattern which ends in node @p node_id
	uint nextPatt(uint node_id) const { return nodes[node_id].next_pattern; }

	// Builds fail edges (have to be invoked before calls to searchIn())
	void buildFails();

	// Returns set of ids of nodes in which longest matching patterns end
	std::vector<uint> searchIn(const StringView& text) const;
};
