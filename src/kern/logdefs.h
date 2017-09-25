// ### How to create a tracebuffer entry ###
//
// If you only need a temporary debugging aid then you can use one of
// the standard kernel logging events: 
// 
//   LOG_MSG(Context *context, const char *msg)
//     - context is something like context_of(this) or current_context()
//       or 0 if there is no context available
//     - msg should be displayed in the tracebuffer view
//
//   LOG_MSG_3VAL(Context *context, const char *msg,
//                Mword val1, Mword val2, Mword val3)
//     - context and msg can be used the same way LOG_MSG does
//     - val1, val2, and val3 are values that will be displayed in
//       the tracebuffer view as hexadecimal values
//
//
#pragma once

#include "globalconfig.h"

#if !defined(CONFIG_JDB)

#define LOG_TRACE_COND(name, sc, ctx, fmt, cond, code...) do { } while (0)
#define LOG_TRACE(name, sc, ctx, fmt, code...) do { } while (0)
#define LOG_CONTEXT_SWITCH                     do { } while (0)
#define LOG_TRAP                               do { } while (0)
#define LOG_TRAP_N(n)                          do { } while (0)
#define LOG_TRAP_CN(c, n)                      do { } while (0)
#define LOG_SCHED_SAVE(n)                      do { } while (0)
#define LOG_SCHED_LOAD(n)                      do { } while (0)
#define LOG_MSG(ctx, txt)                      do { } while (0)
#define LOG_MSG_3VAL(ctx, txt, v1, v2, v3)     do { } while (0)

#else

#include "globals.h"
#include "jdb_tbuf.h"
#include "cpu_lock.h"
#include "lock_guard.h"
#include "processor.h"

#define LOG_TRACE_COND(name, sc, ctx, fmt, cond, code...)               \
  BEGIN_LOG_EVENT(name, sc, fmt)                                        \
    if (cond)                                                           \
      {                                                                 \
        fmt *l = Jdb_tbuf::new_entry<fmt>();                            \
        l->set_global(__do_log__, ctx, Proc::program_counter());        \
        {code;}                                                         \
        Jdb_tbuf::commit_entry();                                       \
      }                                                                 \
  END_LOG_EVENT

#define LOG_TRACE(name, sc, ctx, fmt, code...)                          \
  LOG_TRACE_COND(name, sc, ctx, fmt, true, code)

#define LOG_CONTEXT_SWITCH                                              \
  LOG_TRACE("Context switch", "csw", this, Tb_entry_ctx_sw,             \
    Sched_context *cs = Sched_context::rq.current().current_sched();    \
    l->from_space = space();                                            \
    l->_ip = regs()->ip_syscall_page_user();                            \
    l->dst = t;                                                         \
    l->dst_orig = t_orig;                                               \
    l->lock_cnt = t_orig->lock_cnt();                                   \
    l->from_sched = cs;                                                 \
    l->from_prio = cs ? cs->prio() : 0;                                 \
    l->kernel_ip = (Mword)__builtin_return_address(0) )

#define LOG_TRAP                                                        \
  LOG_TRACE_COND("Exceptions", "exc", current(), Tb_entry_trap,         \
                 (!ts->exclude_logging()),                              \
    l->set(ts->ip(), ts);                                               \
    Jdb_tbuf::commit_entry() )

#define LOG_TRAP_CN(curr, n)                                            \
  LOG_TRACE("Exceptions", "exc", curr, Tb_entry_trap,                   \
    Mword ip = (Mword)(__builtin_return_address(0));                    \
    l->set(ip, n))

#define LOG_TRAP_N(n)                                                   \
  LOG_TRAP_CN(current(), n)

#define LOG_SCHED_SAVE(cs)                                              \
  LOG_TRACE("Scheduling context save", "sch", current(), Tb_entry_sched,\
    l->mode = 0;                                                        \
    l->owner = cs->context();                                           \
    l->id = 0;                                                          \
    l->prio = cs->prio();                                               \
    l->left = cs->left();                                               \
    l->quantum = 0; /*cs->quantum()*/)

#define LOG_SCHED_LOAD(cs)                                              \
  LOG_TRACE("Scheduling context load", "sch", current(), Tb_entry_sched,\
    l->mode = 1;                                                        \
    l->owner = cs->context();                                           \
    l->id = 0;                                                          \
    l->prio = cs->prio();                                               \
    l->left = cs->left();                                               \
    l->quantum = 0; /*cs->quantum()*/)

/*
 * Kernel instrumentation macro used by fm3. Do not remove!
 */
#define LOG_MSG(context, text)                                          \
  do {                                                                  \
    /* The cpu_lock is needed since virq::hit() depends on it */        \
    auto guard = lock_guard(cpu_lock);                                  \
    Tb_entry_ke *tb = static_cast<Tb_entry_ke*>(Jdb_tbuf::new_entry()); \
    tb->set(context, Proc::program_counter());                          \
    tb->msg.set_const(text);                                            \
    Jdb_tbuf::commit_entry();                                           \
  } while (0)

/*
 * Kernel instrumentation macro used by fm3. Do not remove!
 */
#define LOG_MSG_3VAL(context, text, v1, v2, v3)                         \
  do {                                                                  \
    /* The cpu_lock is needed since virq::hit() depends on it */        \
    auto guard = lock_guard(cpu_lock);                                  \
    Tb_entry_ke_reg *tb = Jdb_tbuf::new_entry<Tb_entry_ke_reg>();       \
    tb->set(context, Proc::program_counter());                          \
    tb->v[0] = v1; tb->v[1] = v2; tb->v[2] = v3;                        \
    tb->msg.set_const(text);                                            \
    Jdb_tbuf::commit_entry();                                           \
  } while (0)

#endif // !CONFIG_JDB

#if defined(CONFIG_JDB) && defined(CONFIG_JDB_ACCOUNTING)

#define CNT_CONTEXT_SWITCH      Jdb_tbuf::status()->kerncnts[Kern_cnt_context_switch]++;
#define CNT_ADDR_SPACE_SWITCH   Jdb_tbuf::status()->kerncnts[Kern_cnt_addr_space_switch]++;
#define CNT_IRQ                 Jdb_tbuf::status()->kerncnts[Kern_cnt_irq]++;
#define CNT_PAGE_FAULT          Jdb_tbuf::status()->kerncnts[Kern_cnt_page_fault]++;
#define CNT_IO_FAULT            Jdb_tbuf::status()->kerncnts[Kern_cnt_io_fault]++;
#define CNT_SCHEDULE            Jdb_tbuf::status()->kerncnts[Kern_cnt_schedule]++;
#define CNT_EXC_IPC             Jdb_tbuf::status()->kerncnts[Kern_cnt_exc_ipc]++;

// FIXME: currently unused entries below
#define CNT_SHORTCUT_FAILED     Jdb_tbuf::status()->kerncnts[Kern_cnt_shortcut_failed]++;
#define CNT_SHORTCUT_SUCCESS    Jdb_tbuf::status()->kerncnts[Kern_cnt_shortcut_success]++;
#define CNT_IPC_LONG            Jdb_tbuf::status()->kerncnts[Kern_cnt_ipc_long]++;
#define CNT_TASK_CREATE         Jdb_tbuf::status()->kerncnts[Kern_cnt_task_create]++;
#define CNT_IOBMAP_TLB_FLUSH    Jdb_tbuf::status()->kerncnts[Kern_cnt_iobmap_tlb_flush]++;

#else

#define CNT_CONTEXT_SWITCH	do { } while (0)
#define CNT_ADDR_SPACE_SWITCH	do { } while (0)
#define CNT_IRQ			do { } while (0)
#define CNT_PAGE_FAULT		do { } while (0)
#define CNT_IO_FAULT		do { } while (0)
#define CNT_SCHEDULE		do { } while (0)
#define CNT_EXC_IPC             do { } while (0)

// FIXME: currently unused entries below
#define CNT_SHORTCUT_FAILED	do { } while (0)
#define CNT_SHORTCUT_SUCCESS	do { } while (0)
#define CNT_IPC_LONG		do { } while (0)
#define CNT_TASK_CREATE		do { } while (0)
#define CNT_IOBMAP_TLB_FLUSH	do { } while (0)

#endif // CONFIG_JDB && CONFIG_JDB_ACCOUNTING




