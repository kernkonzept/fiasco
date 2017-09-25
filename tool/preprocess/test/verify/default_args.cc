// AUTOMATICALLY GENERATED -- DO NOT EDIT!         -*- c++ -*-

#include "default_args.h"
#include "default_args_i.h"

#line 26 "default_args.cpp"

extern "C"
void
disasm_bytes(char *buffer, unsigned len, unsigned va, unsigned task,
             int show_symbols, int show_intel_syntax,
             int (*peek_task)(unsigned addr, unsigned task),
             const char* (*get_symbol)(unsigned addr, unsigned task)) WEAK;
