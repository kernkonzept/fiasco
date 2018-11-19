/* SPDX-License-Identifier: GPL-2.0-only OR License-Ref-kk-custom */
/*
 * Copyright (C) 2018-2021, 2023 Kernkonzept GmbH.
 * Author(s): Steffen Liebergeld <steffen.liebergeld@kernkonzept.com>
 *            Marcus Hähnel <marcus.haehnel@kernkonzept.com>
 */

IMPLEMENTATION:

#include "jdb_module.h"
#include "static_init.h"
#include "types.h"

extern "C" void gcov_print();

class Jdb_gcov_module : public Jdb_module
{
public:
  Jdb_gcov_module() FIASCO_INIT;
};

static Jdb_gcov_module jdb_gcov_module INIT_PRIORITY(JDB_MODULE_INIT_PRIO);

PUBLIC
Jdb_module::Action_code
Jdb_gcov_module::action(int cmd, void *&, char const *&, int &)
{
  if (cmd == 0)
    gcov_print();

  return NOTHING;
}

PUBLIC
int
Jdb_gcov_module::num_cmds() const
{
  return 1;
}

PUBLIC
Jdb_module::Cmd const *
Jdb_gcov_module::cmds() const
{
  static Cmd cs[] = { { 0, "&", "gcov", "", "&\tprint gcov info", 0 } };

  return cs;
}

IMPLEMENT
Jdb_gcov_module::Jdb_gcov_module()
: Jdb_module("GENERAL")
{}
