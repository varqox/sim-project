#include "main.hpp"

namespace coloring
{
	special_aho _aho;

	void init()
	{
		string types[]={"uint_least16_t", "uint_least32_t", "uint_least64_t", "uint_fast16_t", "uint_fast64_t", "uint_least8_t", "int_least16_t", "int_least32_t", "int_least64_t", "uint_fast32_t", "uint_fast8_t", "int_fast16_t", "int_least8_t", "int_fast32_t", "int_fast64_t", "int_fast8_t", "_Char16_t", "_Char32_t", "uintptr_t", "uintmax_t", "wint_t", "wctrans_t", "unsigned", "uint16_t", "uint32_t", "uint64_t", "intmax_t", "wctype_t", "intptr_t", "wchar_t", "uint8_t", "int16_t", "int32_t", "int64_t", "wchar_t", "double", "signed", "size_t", "time_t", "int8_t", "short", "float", "void", "char", "bool", "long", "int"},
			keywords[]={"enum", "auto", "class", "union", "struct", "namespace", "or", "if", "do", "for", "asm", "and", "try", "not", "xor", "new", "goto", "else", "this", "case", "or_eq", "final", "using", "while", "bitor", "compl", "const", "catch", "break", "sizeof", "static", "switch", "return", "and_eq", "bitand", "not_eq", "xor_eq", "typeid", "throw", "extern", "export", "friend", "import", "public", "inline", "delete", "typedef", "alignof", "mutable", "decltype", "continue", "explicit", "default", "nullptr", "private", "virtual", "volatile", "protected", "constexpr", "operator", "override", "template", "register", "typename", "static_cast", "const_cast", "align_union", "dynamic_cast", "reinterpret_cast", "static_assert"},
			operators[]={".", ",", "*", "?", "[", "]", "(", ")", ":", "!", "|", "^", "+", "-", "*", "{", "}", "%", ";", "<", ">", "=", "/", "&", "~"};
		unsigned types_size=sizeof(types)/sizeof(string), keywords_size=sizeof(keywords)/sizeof(string), operators_size=sizeof(operators)/sizeof(string);
		vector<pair<string, const_string> > new_patterns;
		for(unsigned i=0; i<types_size; ++i)
			new_patterns.push_back(make_pair(types[i], span::type));
		for(unsigned i=0; i<keywords_size; ++i)
			new_patterns.push_back(make_pair(keywords[i], span::keyword));
		for(unsigned i=0; i<operators_size; ++i)
			new_patterns.push_back(make_pair(operators[i], span::operators));
		new_patterns.push_back(make_pair("true", span::true_false));
		new_patterns.push_back(make_pair("false", span::true_false));
		_aho.set_patterns(new_patterns);
	}

	string synax_highlight(const string& code)
	{
		string out;
		_aho.find(code);
		for(int c_len=code.size(), i=0; i<c_len; ++i)
		{
			if(_aho[i]!=-1 && (_aho.pattern(_aho[i]).second==span::operators || i==0 || (!is_true_name[static_cast<unsigned char>(code[i-1])] && !is_true_name[static_cast<unsigned char>(code[i+_aho.pattern(_aho[i]).first.size()])])))
			{
				out+=_aho.pattern(_aho[i]).second;
				out+=safe_string(_aho.pattern(_aho[i]).first);
				i+=_aho.pattern(_aho[i]).first.size()-1;
				out+=span::end;
			}
			else out+=safe_character(code[i]);
		}
		return out;
	}

	void color_code(const string& code, ostream& output)
	{
		string to_syn_high;
		for(int code_len=code.size(), i=0; i<code_len; ++i)
		{
			if(code[i]=='#') // preprocessor
			{
				output << synax_highlight(to_syn_high);
				to_syn_high="";
				output << span::preprocessor << code[i];
				while(++i<code_len && code[i]!='\n')
				{
					if(i+1<code_len && 0==memcmp(code.c_str()+i, "\\\n", 2))
					{
						output << "\\\n";
						++i;
					}
					else
						output << safe_character(code[i]);
				}
				--i;
				output << span::end;
			}
			else if(i+1<code_len && code[i]=='/' && code[i+1]=='/') // oneline comment
			{
				output << synax_highlight(to_syn_high);
				to_syn_high="";
				output << span::comment << code[i];
				while(++i<code_len && code[i]!='\n')
					output << safe_character(code[i]);
				--i;
				output << span::end;
			}
			else if(i+1<code_len && code[i]=='/' && code[i+1]=='*') // multiline comment
			{
				output << synax_highlight(to_syn_high);
				to_syn_high="";
				output << span::comment << "/*";
				i+=2;
				if(i<code_len) output << safe_character(code[i]);
				while(++i<code_len && 0!=memcmp(code.c_str()+i-1, "*/", 2))
					output << safe_character(code[i]);
				output << '/' << span::end;
			}
			else if(code[i]=='"' || code[i]=='\'') // strings and chars
			{
				output << synax_highlight(to_syn_high);
				to_syn_high="";
				unsigned char str_or_char=code[i];
				output << (str_or_char=='\'' ? span::character : span::string) << code[i];
				while(++i<code_len && code[i]!=str_or_char)
				{
					if(code[i]=='\\')
					{
						output << span::special_character << '\\';
						if(++i<code_len)
							output << safe_character(code[i]);
						output << span::end;
					}
					else
						output << safe_character(code[i]);
				}
				output << str_or_char << span::end;
			}
			else if(code[i]>='0' && code[i]<='9' && (i==0 || (!is_name[static_cast<unsigned char>(code[i-1])] && (code[i-1]<'0' || code[i-1]>'9')))) // numbers
				{
					{
						output << synax_highlight(to_syn_high);
						to_syn_high="";
						output << span::number << code[i];
						bool point=false;
						while(++i<code_len && ((code[i]>='0' && code[i]<='9') || code[i]=='.'))
						{
							if(code[i]=='.')
							{
								if(point) break;
								point=true;
							}
							output << code[i];
						}
						if(i<code_len && code[i]=='L')
						{
							output << 'L';
							if(++i<code_len && code[i]=='L')
							{
								output << 'L';
								++i;
							}
						}
						--i;
						output << span::end;
					}
				}
			else to_syn_high+=code[i];
		}
		output << synax_highlight(to_syn_high);
	}
}