#include "simlib/aho_corasick.hh"

using std::vector;

uint AhoCorasick::Node::operator[](char c) const noexcept {
	uint beg = 0;
	uint end = sons.size();
	while (beg < end) {
		uint mid = (beg + end) >> 1;
		if (c < sons[mid].first) {
			end = mid;
		} else if (c == sons[mid].first) {
			return sons[mid].second;
		} else {
			beg = mid + 1;
		}
	}

	if (beg == sons.size() || sons[beg].first != c) {
		return 0;
	}

	return sons[beg].second;
}

uint AhoCorasick::son(uint id, char c) {
	uint beg = 0;
	uint end = nodes[id].sons.size();
	while (beg < end) {
		uint mid = (beg + end) >> 1;
		if (c < nodes[id].sons[mid].first) {
			end = mid;
		} else if (c == nodes[id].sons[mid].first) {
			return nodes[id].sons[mid].second;
		} else {
			beg = mid + 1;
		}
	}

	if (beg == nodes[id].sons.size() || nodes[id].sons[beg].first != c) {
		nodes.emplace_back();
		return nodes[id]
			.sons.emplace(nodes[id].sons.begin() + beg, c, nodes.size() - 1)
			->second;
	}

	return nodes[id].sons[beg].second;
}

void AhoCorasick::add_pattern(const StringView& patt, uint id) {
	uint curr = 0;
	for (char c : patt) {
		curr = son(curr, c);
	}
	nodes[curr].patt_id = id;
}

uint AhoCorasick::find_node(const StringView& str) const noexcept {
	uint x = 0;
	for (char c : str) {
		x = nodes[x][c];
		if (x == 0) {
			return 0;
		}
	}

	return x;
}

void AhoCorasick::build_fail_edges() {
	std::deque<uint> queue;
	for (auto&& p : nodes[0].sons) {
		queue.emplace_back(p.second);
	}

	while (!queue.empty()) {
		uint curr = queue.front();
		queue.pop_front();
		for (auto&& p : nodes[curr].sons) {
			uint v = nodes[curr].fail;
			uint x = 0;
			while ((x = nodes[v][p.first]) == 0 && v) {
				v = nodes[v].fail;
			}
			nodes[p.second].fail = x;
			nodes[p.second].next_pattern =
				(nodes[x].patt_id ? x : nodes[x].next_pattern);
			queue.emplace_back(p.second);
		}
	}
}

vector<uint> AhoCorasick::search_in(StringView text) const {
	vector<uint> res(text.size());
	uint curr = 0;
	uint x = 0;
	for (size_t i = 0; i < text.size(); ++i) {
		char c = text[i];
		while ((x = nodes[curr][c]) == 0 && curr) {
			curr = nodes[curr].fail;
		}
		curr = x;
		res[i] = (nodes[curr].patt_id ? curr : nodes[curr].next_pattern);
	}

	return res;
}
