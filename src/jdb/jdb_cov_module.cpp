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

extern "C" void cov_print(void) __attribute__ ((weak));

class Jdb_cov_module : public Jdb_module
{
public:
  Jdb_cov_module() FIASCO_INIT;
};

static Jdb_cov_module jdb_cov_module INIT_PRIORITY(JDB_MODULE_INIT_PRIO);

PUBLIC
Jdb_module::Action_code
Jdb_cov_module::action(int cmd, void *&, char const *&, int &) override
{
  if (cmd == 0)
    cov_print();

  return NOTHING;
}

PUBLIC
int
Jdb_cov_module::num_cmds() const override
{
  return 1;
}

PUBLIC
Jdb_module::Cmd const *
Jdb_cov_module::cmds() const override
{
  static Cmd cs[] = { { 0, "&", "cov", "", "&\tprint cov info", 0 } };

  return cs;
}

IMPLEMENT
Jdb_cov_module::Jdb_cov_module()
: Jdb_module("GENERAL")
{}
