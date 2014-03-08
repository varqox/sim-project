#pragma once

#include <map>
#include <string>
#include <vector>

class aho_base
{
protected:
	// node of Trie tree
	struct node
	{
		typedef node* _ptr;

		bool is_pattern;
		unsigned char key;
		unsigned pattern_id; // pattern id
		_ptr fail, long_sh_pat; // fail pointer, the longest shorter pattern
		_ptr son[256];

		node(unsigned char _k = 0, bool _ip = false, _ptr _f = NULL, _ptr _l = NULL): is_pattern(_ip), key(_k), pattern_id(), fail(_f), long_sh_pat(_l)
		{
			for(int i = 0; i < 256; ++i)
				son[i] = NULL;
		}

		node(const node& nd): is_pattern(nd.is_pattern), key(nd.key), pattern_id(nd.pattern_id), fail(nd.fail), long_sh_pat(nd.long_sh_pat)
		{
			for(int i = 0; i < 256; ++i)
				son[i] = nd.son[i];
		}

		node& operator=(const node& nd)
		{
			is_pattern = nd.is_pattern;
			key = nd.key;
			pattern_id = nd.pattern_id;
			fail = nd.fail;
			long_sh_pat = nd.long_sh_pat;
			for(int i = 0; i < 256; ++i)
				son[i] = nd.son[i];
			return *this;
		}

		~node()
		{
			for(int i = 0; i != 256; ++i)
				if(son[i] != NULL)
					delete son[i];
		}

		void copy(_ptr nd)
		{
			is_pattern = nd->is_pattern;
			key = nd->key;
			pattern_id = nd->pattern_id;
			for(int i = 0; i != 256; ++i)
				if(nd->son[i])
					son[i] = new node(*nd->son[i]), son[i]->copy(nd->son[i]);
				else
					son[i] = NULL;
		}
	};

	node::_ptr root;

	void add_fails();

	// returns real id of inserted word
	unsigned insert(const std::string& word, unsigned id);

	aho_base(): root(new node)
	{
		root->fail = root->long_sh_pat = root;
	}

	aho_base(const aho_base& ab): root(new node)
	{
		root->copy(ab.root);
		root->fail = root->long_sh_pat = root;
		add_fails();
	}

	aho_base& operator=(const aho_base& ab)
	{
		root->copy(ab.root);
		root->fail = root->long_sh_pat = root;
		add_fails();
		return *this;
	}

	virtual ~aho_base()
	{
		delete root;
	}
};

class aho : public aho_base
{
protected:
	std::vector<std::vector<unsigned>*> fin; // finded patterns
	std::vector<unsigned> fin_id; // tells real (in Trie) id of pattern

public:
	aho(): aho_base(), fin(), fin_id()
	{}

	aho(const aho& _a): aho_base(_a), fin(_a.fin), fin_id(_a.fin_id)
	{
		for(unsigned i = 0, s = fin.size(); i < s; ++i)
		{
			if(fin_id[i] == i)
				fin[i] = new std::vector<unsigned>(*fin[i]);
			else
				fin[i] = fin[fin_id[i]];
		}
	}

	aho& operator=(aho& _a)
	{
		aho_base::operator=(_a);
		fin = _a.fin;
		fin_id = _a.fin_id;
		for(unsigned i = 0, s = fin.size(); i < s; ++i)
		{
			if(fin_id[i] == i)
				fin[i] = new std::vector<unsigned>(*fin[i]);
			else
				fin[i] = fin[fin_id[i]];
		}
		return *this;
	}

	virtual ~aho()
	{
		for(unsigned i = 0, s = fin.size(); i < s; ++i)
			if(fin_id[i] == i)
				delete fin[i];
	}

	void swap(aho& _a)
	{
		std::swap(root, _a.root);
		std::swap(fin, _a.fin);
		std::swap(fin_id, _a.fin_id);
	}

	std::vector<std::vector<unsigned>*>::size_type size() const
	{return fin.size();}

	std::vector<unsigned>& operator[](std::vector<std::vector<unsigned>*>::size_type n)
	{return *fin[n];}

	const std::vector<unsigned>& operator[](std::vector<std::vector<unsigned>*>::size_type n) const
	{return *fin[n];}

	void find(const std::vector<std::string>& patterns, const std::string& text);
};

template<class T>
class special_aho : public aho_base
{
protected:
	std::vector<int> fin; // finded patterns occurrences
	std::vector<std::pair<std::string, T> > patterns;

public:
	special_aho(): aho_base(), fin(), patterns()
	{}

	special_aho(const special_aho& _a): aho_base(_a), fin(_a.fin), patterns(_a.patterns)
	{}

	special_aho& operator=(special_aho& _a)
	{
		aho_base::operator=(_a);
		fin = _a.fin;
		patterns = _a.patterns;
		return *this;
	}

	virtual ~special_aho()
	{}

	void swap(special_aho& _a)
	{
		std::swap(root, _a.root);
		std::swap(fin, _a.fin);
		std::swap(patterns, _a.patterns);
	}

	std::vector<int>::size_type size() const
	{return fin.size();}

	int& operator[](std::vector<int>::size_type n)
	{return fin[n];}

	const int& operator[](std::vector<int>::size_type n) const
	{return fin[n];}

	const std::pair<std::string, T>& pattern(typename std::vector<std::pair<std::string, T> >::size_type n) const
	{return patterns[n];}

	void set_patterns(const std::vector<std::pair<std::string, T> >& new_patterns);
	void find(const std::string& text);
};

template<class T>
void special_aho<T>::set_patterns(const std::vector<std::pair<std::string, T> >& new_patterns)
{
	patterns = new_patterns;
	// clear Trie
	delete root;
	root = new node;
	root->fail = root->long_sh_pat = root;
	// Build Trie
	for(unsigned i = 0, s = patterns.size(); i < s; ++i)
		insert(patterns[i].first, i);
	add_fails(); // add fail and long_sh_pat edges
}

template<class T>
void special_aho<T>::find(const std::string& text)
{
	// clear fin
	fin.resize(text.size());
	node::_ptr actual = root, found; // actual node and auxiliary pointer (to finding patterns)
	for(unsigned i = 0, s = text.size(); i < s; ++i)
	{
		// we search for node wich has soon with key == *i
		while(actual != root && actual->son[static_cast<unsigned char>(text[i])] == NULL)
			actual = actual->fail;
		// if we find this son (else actual == root)
		if(actual->son[static_cast<unsigned char>(text[i])] != NULL)
			actual = actual->son[static_cast<unsigned char>(text[i])];
		// default value (if none pattern will be found)
		fin[i] = -1;
		// if actual node is pattern then we'll add it to fin
		if(actual->is_pattern)
			fin[i - patterns[actual->pattern_id].first.size() + 1] = actual->pattern_id;
		// find orher patterns
		else if((found = actual->long_sh_pat) != root)
			fin[i - patterns[found->pattern_id].first.size() + 1] = found->pattern_id;
	}
}