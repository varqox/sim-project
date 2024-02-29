#pragma once

#include <simlib/argv_parser.hh>

namespace commands {

void checker(ArgvParser args);

void clean();

void devclean();

void doc(ArgvParser args);

void docwatch(ArgvParser args);

void genin();

void genout();

void gen();

void regenin();

void regen();

void reseed();

// Displays help
void help(const char* program_name);

// Displays version
void version() noexcept;

void init(ArgvParser args);

void interactive(ArgvParser args);

void label(ArgvParser args);

void main_sol(ArgvParser args);

void mem(ArgvParser args);

void name(ArgvParser args);

void prog(ArgvParser args);

void save(ArgvParser args);

void statement(ArgvParser args);

void template_command(ArgvParser args);

void test(ArgvParser args);

void unset(ArgvParser args);

void zip();

void devzip();

} // namespace commands
