/*
 * vi:ft=c
 */

#pragma once

#define JDB_TABLE(name) \
  . = ALIGN(32);                               \
  PROVIDE(_jdb_ ## name ## _table = .);        \
  KEEP(*(SORT(.debug.jdb.name ## _table)));    \
  PROVIDE(_jdb_ ## name ## _table_end = .);
