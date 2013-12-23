#include "main.hpp"

int aho::class_trie::add_word(const string& word, int id)
{
	int ver=0; // actual node (vertex)
	for(int s=word.size(), i=0; i<s; ++i)
	{
		if(this->graph[ver].E[word[i]]!=0) ver=this->graph[ver].E[word[i]]; // actual view node = next node
		else
		{
			ver=this->graph[ver].E[word[i]]=this->graph.size(); // add id of new node
			this->graph.push_back(node(word[i])); // add new node
		}
	}
	if(!this->graph[ver].is_pattern)
	{
		this->graph[ver].is_pattern=true;
		this->graph[ver].pattern_id=id;
	}
return this->graph[ver].pattern_id;
}

void aho::class_trie::add_fails() // and the longest shorter patterns, based on BFS algorithm
{
	queue<int> V;
	// add root childrens
	for(int i=0; i<256; ++i)
	{
		if(this->graph.front().E[i]!=0) // if children exists
		{
			this->graph[this->graph.front().E[i]].fail=this->graph[this->graph.front().E[i]].long_sh_pat=0;
			V.push(this->graph.front().E[i]);
		}
	}
	while(!V.empty())
	{
		int actual=V.front(); // id of actual view node
		for(int i=0; i<256; ++i) // i is character of view node
		{
			if(this->graph[actual].E[i]!=0) // if children exists
			{
				actual=this->graph[actual].fail; // we have view node parent's fial edge
				while(actual>0 && this->graph[actual].E[i]==0) // while we don't have node with children of actual character (i)
					actual=this->graph[actual].fail;
				actual=this->graph[this->graph[V.front()].E[i]].fail=this->graph[actual].E[i]; // the longest sufix, if 0 then longest sufix = root
				// add the longest shorter pattern
				if(this->graph[actual].is_pattern) // if the fail node is pattern then is long_sh_pat
					this->graph[this->graph[V.front()].E[i]].long_sh_pat=actual;
				else // long_sh_pat is the fail node's long_sh_pat
					this->graph[this->graph[V.front()].E[i]].long_sh_pat=this->graph[actual].long_sh_pat;
				actual=V.front();
				V.push(this->graph[actual].E[i]); // add this children to queue
			}
		}
		V.pop(); // remove visited node
	}
}

void aho::find(const vector<string>& patterns, const string& text)
{
	vector<class_trie::node>().swap(this->trie.graph); // clear trie::graph
	vector<vector<unsigned>* >().swap(this->fin); // clear fin
	this->fin.resize(patterns.size()); // set number of patterns
	// trie::init(); 
	class_trie().swap(this->trie); // initialize trie
	unsigned tmp;
	for(int i=patterns.size()-1; i>=0; --i) // add patterns to trie
	{
		tmp=this->trie.add_word(patterns[i], i);
		if(tmp==i) this->fin[i]=new vector<unsigned>;
		else this->fin[i]=this->fin[tmp];
	}
	this->trie.add_fails(); // add fails edges
	int act=0, pat; // actual node - root
	for(int s=text.size(), i=0; i<s; ++i)
	{
		while(act>0 && this->trie.graph[act].E[text[i]]==0)
			act=this->trie.graph[act].fail; // while we can't add text[i] to path, go to fail node
		if(this->trie.graph[act].E[text[i]]!=0) // if we can add text[i] to path
			act=this->trie.graph[act].E[text[i]];
		if(this->trie.graph[act].is_pattern) // if actual node is pattern, then add it to fin
			this->fin[this->trie.graph[act].pattern_id]->push_back(i);
		pat=this->trie.graph[act].long_sh_pat; // go to the pattern node
		while(pat>0) // finding patterns
		{
			this->fin[this->trie.graph[pat].pattern_id]->push_back(i); // add pat node to fin
			pat=this->trie.graph[pat].long_sh_pat; // go to the next pattern
		}
	}
}

int special_aho::class_trie::add_word(const string& word, int id)
{
	int ver=0; // actual node (vertex)
	for(int s=word.size(), i=0; i<s; ++i)
	{
		if(this->graph[ver].E[word[i]]!=0) ver=this->graph[ver].E[word[i]]; // actual view node = next node
		else
		{
			ver=this->graph[ver].E[word[i]]=this->graph.size(); // add id of new node
			this->graph.push_back(node(word[i])); // add new node
		}
	}
	if(!this->graph[ver].is_pattern)
	{
		this->graph[ver].is_pattern=true;
		this->graph[ver].pattern_id=id;
	}
return this->graph[ver].pattern_id;
}

void special_aho::class_trie::add_fails() // and the longest shorter patterns, based on BFS algorithm
{
	queue<int> V;
	// add root childrens
	for(int i=0; i<256; ++i)
	{
		if(this->graph.front().E[i]!=0) // if children exists
		{
			this->graph[this->graph.front().E[i]].fail=this->graph[this->graph.front().E[i]].long_sh_pat=0;
			V.push(this->graph.front().E[i]);
		}
	}
	while(!V.empty())
	{
		int actual=V.front(); // id of actual view node
		for(int i=0; i<256; ++i) // i is character of view node
		{
			if(this->graph[actual].E[i]!=0) // if children exists
			{
				actual=this->graph[actual].fail; // we have view node parent's fial edge
				while(actual>0 && this->graph[actual].E[i]==0) // while we don't have node with children of actual character (i)
					actual=this->graph[actual].fail;
				actual=this->graph[this->graph[V.front()].E[i]].fail=this->graph[actual].E[i]; // the longest sufix, if 0 then longest sufix = root
				// add the longest shorter pattern
				if(this->graph[actual].is_pattern) // if the fail node is pattern then is long_sh_pat
					this->graph[this->graph[V.front()].E[i]].long_sh_pat=actual; // long_sh_pat is the fail node's long_sh_pat
				else 
					this->graph[this->graph[V.front()].E[i]].long_sh_pat=this->graph[actual].long_sh_pat;
				actual=V.front();
				V.push(this->graph[actual].E[i]); // add this children to queue
			}
		}
		V.pop(); // remove visited node
	}
}

void special_aho::set_patterns(const vector<pair<string, const_string> >& new_patterns)
{
	this->patterns=new_patterns;
	vector<class_trie::node>().swap(this->trie.graph); // clear trie.graph
	class_trie().swap(this->trie); // initialize trie
	for(int i=patterns.size()-1; i>=0; --i) // add patterns to trie
		this->trie.add_word(patterns[i].first, i);
	this->trie.add_fails(); // add fails edges
}

void special_aho::find(const string& text)
{
	// vector<int>().swap(this->fin); // clear fin
	this->fin.resize(text.size()); // set number of finds
	int act=0, pat; // actual node - root
	for(int s=text.size(), i=0; i<s; ++i)
	{
		while(act>0 && this->trie.graph[act].E[text[i]]==0)
			act=this->trie.graph[act].fail; // while we can't add text[i] to path, go to fail node
		if(this->trie.graph[act].E[text[i]]!=0) // if we can add text[i] to path
			act=this->trie.graph[act].E[text[i]];
		this->fin[i]=-1;
		if(this->trie.graph[act].is_pattern) // if actual node is pattern, then add it to fin
			this->fin[i-this->patterns[this->trie.graph[act].pattern_id].first.size()+1]=this->trie.graph[act].pattern_id;
		else if(0<(pat=this->trie.graph[act].long_sh_pat)) // go to the pattern node and check if not root
			this->fin[i-this->patterns[this->trie.graph[pat].pattern_id].first.size()+1]=this->trie.graph[pat].pattern_id;
		/*while(pat>0) // finding patterns
		{
			this->fin[this->trie.graph[pat].pattern_id]->push_back(i); // add pat node to fin
			pat=this->trie.graph[pat].long_sh_pat; // go to the next pattern
		}*/
	}
}