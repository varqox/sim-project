#pragma once

#include <simlib/parsers.h>
#include <simlib/sim/simfile.h>

// simfile_commands.cc
void replace_var_in_simfile(const sim::Simfile& sf, FilePath simfile_path,
	StringView simfile_contents, StringView var_name, StringView replacement,
	bool escape_replacement = true);

void replace_var_in_simfile(const sim::Simfile& sf, FilePath simfile_path,
	StringView simfile_contents, StringView var_name,
	const std::vector<std::string>& replacement);

namespace command {

// simfile_commands.cc
int checker(ArgvParser args);

int init(ArgvParser args);

int label(ArgvParser args);

int main_sol(ArgvParser args);

int mem(ArgvParser args);

int name(ArgvParser args);

int statement(ArgvParser args);

// commands.cc
int doc(ArgvParser args);

int genout(ArgvParser args);

int gentests(ArgvParser args);

int package(ArgvParser args);

int prepare(ArgvParser args);

int prog(ArgvParser args);

int test(ArgvParser args);

int clean(ArgvParser args);

int zip(ArgvParser args);

} // namespace command
