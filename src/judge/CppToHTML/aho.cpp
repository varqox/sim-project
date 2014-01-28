#include "main.hpp"

int aho::class_trie::add_word(const string& word, int id)
{
	int ver=0; // actual node (vertex)
	for(int s=word.size(), i=0; i<s; ++i)
	{
		if(graph[ver].E[static_cast<unsigned char>(word[i])]!=0) ver=graph[ver].E[static_cast<unsigned char>(word[i])]; // actual view node = next node
		else
		{
			ver=graph[ver].E[static_cast<unsigned char>(word[i])]=graph.size(); // add id of new node
			graph.push_back(node(word[i])); // add new node
		}
	}
	if(!graph[ver].is_pattern)
	{
		graph[ver].is_pattern=true;
		graph[ver].pattern_id=id;
	}
return graph[ver].pattern_id;
}

void aho::class_trie::add_fails() // and the longest shorter patterns, based on BFS algorithm
{
	queue<int> V;
	// add root childrens
	for(int i=0; i<256; ++i)
	{
		if(graph.front().E[i]!=0) // if children exists
		{
			graph[graph.front().E[i]].fail=graph[graph.front().E[i]].long_sh_pat=0;
			V.push(graph.front().E[i]);
		}
	}
	while(!V.empty())
	{
		int actual=V.front(); // id of actual view node
		for(int i=0; i<256; ++i) // i is character of view node
		{
			if(graph[actual].E[i]!=0) // if children exists
			{
				actual=graph[actual].fail; // we have view node parent's fial edge
				while(actual>0 && graph[actual].E[i]==0) // while we don't have node with children of actual character (i)
					actual=graph[actual].fail;
				actual=graph[graph[V.front()].E[i]].fail=graph[actual].E[i]; // the longest sufix, if 0 then longest sufix = root
				// add the longest shorter pattern
				if(graph[actual].is_pattern) // if the fail node is pattern then is long_sh_pat
					graph[graph[V.front()].E[i]].long_sh_pat=actual;
				else // long_sh_pat is the fail node's long_sh_pat
					graph[graph[V.front()].E[i]].long_sh_pat=graph[actual].long_sh_pat;
				actual=V.front();
				V.push(graph[actual].E[i]); // add this children to queue
			}
		}
		V.pop(); // remove visited node
	}
}

void aho::find(const vector<string>& patterns, const string& text)
{
	vector<class_trie::node>().swap(trie.graph); // clear trie::graph
	vector<vector<unsigned>* >().swap(fin); // clear fin
	fin.resize(patterns.size()); // set number of patterns
	// trie::init(); 
	class_trie().swap(trie); // initialize trie
	unsigned tmp;
	for(int i=patterns.size()-1; i>=0; --i) // add patterns to trie
	{
		tmp=trie.add_word(patterns[i], i);
		if(tmp==static_cast<unsigned>(i)) fin[i]=new vector<unsigned>;
		else fin[i]=fin[tmp];
	}
	trie.add_fails(); // add fails edges
	int act=0, pat; // actual node - root
	for(int s=text.size(), i=0; i<s; ++i)
	{
		while(act>0 && trie.graph[act].E[static_cast<unsigned char>(text[i])]==0)
			act=trie.graph[act].fail; // while we can't add text[i] to path, go to fail node
		if(trie.graph[act].E[static_cast<unsigned char>(text[i])]!=0) // if we can add text[i] to path
			act=trie.graph[act].E[static_cast<unsigned char>(text[i])];
		if(trie.graph[act].is_pattern) // if actual node is pattern, then add it to fin
			fin[trie.graph[act].pattern_id]->push_back(i);
		pat=trie.graph[act].long_sh_pat; // go to the pattern node
		while(pat>0) // finding patterns
		{
			fin[trie.graph[pat].pattern_id]->push_back(i); // add pat node to fin
			pat=trie.graph[pat].long_sh_pat; // go to the next pattern
		}
	}
}

int special_aho::class_trie::add_word(const string& word, int id)
{
	int ver=0; // actual node (vertex)
	for(int s=word.size(), i=0; i<s; ++i)
	{
		if(graph[ver].E[static_cast<unsigned char>(word[i])]!=0) ver=graph[ver].E[static_cast<unsigned char>(word[i])]; // actual view node = next node
		else
		{
			ver=graph[ver].E[static_cast<unsigned char>(word[i])]=graph.size(); // add id of new node
			graph.push_back(node(word[i])); // add new node
		}
	}
	if(!graph[ver].is_pattern)
	{
		graph[ver].is_pattern=true;
		graph[ver].pattern_id=id;
	}
return graph[ver].pattern_id;
}

void special_aho::class_trie::add_fails() // and the longest shorter patterns, based on BFS algorithm
{
	queue<int> V;
	// add root childrens
	for(int i=0; i<256; ++i)
	{
		if(graph.front().E[i]!=0) // if children exists
		{
			graph[graph.front().E[i]].fail=graph[graph.front().E[i]].long_sh_pat=0;
			V.push(graph.front().E[i]);
		}
	}
	while(!V.empty())
	{
		int actual=V.front(); // id of actual view node
		for(int i=0; i<256; ++i) // i is character of view node
		{
			if(graph[actual].E[i]!=0) // if children exists
			{
				actual=graph[actual].fail; // we have view node parent's fial edge
				while(actual>0 && graph[actual].E[i]==0) // while we don't have node with children of actual character (i)
					actual=graph[actual].fail;
				actual=graph[graph[V.front()].E[i]].fail=graph[actual].E[i]; // the longest sufix, if 0 then longest sufix = root
				// add the longest shorter pattern
				if(graph[actual].is_pattern) // if the fail node is pattern then is long_sh_pat
					graph[graph[V.front()].E[i]].long_sh_pat=actual; // long_sh_pat is the fail node's long_sh_pat
				else 
					graph[graph[V.front()].E[i]].long_sh_pat=graph[actual].long_sh_pat;
				actual=V.front();
				V.push(graph[actual].E[i]); // add this children to queue
			}
		}
		V.pop(); // remove visited node
	}
}

void special_aho::set_patterns(const vector<pair<string, const_string> >& new_patterns)
{
	patterns=new_patterns;
	vector<class_trie::node>().swap(trie.graph); // clear trie.graph
	class_trie().swap(trie); // initialize trie
	for(int i=patterns.size()-1; i>=0; --i) // add patterns to trie
		trie.add_word(patterns[i].first, i);
	trie.add_fails(); // add fails edges
}

void special_aho::find(const string& text)
{
	// vector<int>().swap(fin); // clear fin
	fin.resize(text.size()); // set number of finds
	int act=0, pat; // actual node - root
	for(int s=text.size(), i=0; i<s; ++i)
	{
		while(act>0 && trie.graph[act].E[static_cast<unsigned char>(text[i])]==0)
			act=trie.graph[act].fail; // while we can't add text[i] to path, go to fail node
		if(trie.graph[act].E[static_cast<unsigned char>(text[i])]!=0) // if we can add text[i] to path
			act=trie.graph[act].E[static_cast<unsigned char>(text[i])];
		fin[i]=-1;
		if(trie.graph[act].is_pattern) // if actual node is pattern, then add it to fin
			fin[i-patterns[trie.graph[act].pattern_id].first.size()+1]=trie.graph[act].pattern_id;
		else if(0<(pat=trie.graph[act].long_sh_pat)) // go to the pattern node and check if not root
			fin[i-patterns[trie.graph[pat].pattern_id].first.size()+1]=trie.graph[pat].pattern_id;
		/*while(pat>0) // finding patterns
		{
			fin[trie.graph[pat].pattern_id]->push_back(i); // add pat node to fin
			pat=trie.graph[pat].long_sh_pat; // go to the next pattern
		}*/
	}
}