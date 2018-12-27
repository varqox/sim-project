#pragma once

#include <simlib/parsers.h>

namespace commands {

// commands.cc
void checker(ArgvParser args);

void clean(ArgvParser args);

void doc(ArgvParser args);

void genin(ArgvParser args);

void genout(ArgvParser args);

void gen(ArgvParser args);

// Displays help
void help(const char* program_name);

void init(ArgvParser args);

void label(ArgvParser args);

void main_sol(ArgvParser args);

void mem(ArgvParser args);

void name(ArgvParser args);

void prog(ArgvParser args);

void statement(ArgvParser args);

void test(ArgvParser args);

void zip(ArgvParser args);

} // namespace commands
