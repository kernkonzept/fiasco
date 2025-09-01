// vi:set ft=cpp: -*- Mode: C++ -*-
/* SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom */
/*
 * Copyright (C) 2021-2024 Kernkonzept GmbH.
 * Author(s): Philipp Eppelt <philipp.eppelt@kernkonzept.com>
 */
INTERFACE:

#include <cxx/type_traits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>

#include "global_data.h"
#include "kdb_ke.h"
#include "kip.h"
#include "kmem_slab.h"
#include "l4_error.h"
#include "lock_guard.h"
#include "panic.h"
#include "per_cpu_data.h"
#include "processor.h"
#include "platform_control.h"
#include "spin_lock.h"
#include "timer.h"
#include "thread_object.h"
#include "thread_state.h"
#include "unique_ptr.h"
#include "workload.h"


extern "C" void cov_print(void) __attribute__((weak));

template <typename T, unsigned ALIGN = __alignof(T)>
struct Kmem_slab_t_singleton
{
  static Kmem_slab_t<T, ALIGN> instance;

  static void *alloc() { return instance.alloc(); }

  template<typename Q> static
  void *q_alloc(Q *q) { return instance.template q_alloc<Q>(q); }

  static void free(void *e) { instance.free(e); }

  template<typename Q> static
  void q_free(Q *q, void *e) { instance.template q_free<Q>(q, e); }

  template<typename ...ARGS> static
  T *new_obj(ARGS &&...args)
  { return instance.new_obj(cxx::forward<ARGS>(args)...); }

  template<typename Q, typename ...ARGS> static
  T *q_new(Q *q, ARGS &&...args)
  { return instance.q_new(q, cxx::forward<ARGS>(args)...); }

  static void del(T *e)
  { instance.del(e); }

  template<typename Q> static
  void q_del(Q *q, T *e)
  { instance.q_del(q, e); }
};

template <typename T, unsigned ALIGN>
Kmem_slab_t<T, ALIGN> Kmem_slab_t_singleton<T, ALIGN>::instance;

template <unsigned SIZE, unsigned ALIGN = 8>
struct Kmem_slab_for_size_singleton
{
  static Kmem_slab_for_size<SIZE, ALIGN> instance;

  static void *alloc() { return instance.alloc(); }

  template<typename Q> static
  void *q_alloc(Q *q) { return instance.template q_alloc<Q>(q); }

  static void free(void *e) { instance.free(e); }

  template<typename Q> static
  void q_free(Q *q, void *e) { instance.template q_free<Q>(q, e); }

  static Slab_cache *slab() { return instance.slab(); }
};

template <unsigned SIZE, unsigned ALIGN>
Kmem_slab_for_size<SIZE, ALIGN> Kmem_slab_for_size_singleton<SIZE, ALIGN>::instance;

/// Utest namespace for constants
struct Utest
{
  /// Constants to use with the UTEST_ macros.
  enum : bool { Assert = true, Expect = false };

  template <typename F>
  struct Thread_args
  {
    Thread_args() = delete;
    Thread_args(F const &f) : fn(f) {}
    bool started = false;
    F const &fn;
  };

  /**
   * Maintain multiple Kmem_slab_t and Kmem_slab_for_size instances.
   *
   * Usually this would be done using C++ singletons but global static data (the
   * data instance and the corresponding guard variable) would create global
   * static data in .bss/.data which is forbidden if CONFIG_AMP is enabled. At
   * least the guard variable cannot be wrapped using Global_data<>.
   */
  class Slab_store
  {
    enum { Max_slabs_for_size_align = 32 };

    struct Slab_for_size_align
    {
      ///< Size of the slab type.
      size_t size;
      ///< Alignment of the slab type.
      size_t align;
      ///< 0: Store in Kmem_slab_for_size slab, 1: Store in Kmem_slab_t<T> slab.
      bool for_type;
      ///< Raw pointer to the slab.
      void *slab;
    };

  public:
    template <typename T, unsigned ALIGN = alignof(T)>
    Kmem_slab_t<T, ALIGN> *
    lookup(bool panic_on_fail)
    {
      using Kmem_slab_type = Kmem_slab_t<T, ALIGN>;
      auto g = lock_guard(_lock);
      return _lookup<Kmem_slab_type>(sizeof(T), ALIGN, true, panic_on_fail);
    }

    template <typename T, unsigned ALIGN = alignof(T)>
    Kmem_slab_t<T, ALIGN> *
    create()
    {
      using Kmem_slab_type = Kmem_slab_t<T, ALIGN>;
      auto g = lock_guard(_lock);
      return _create<Kmem_slab_type>(sizeof(T), ALIGN, true);
    }

    template <size_t SIZE, unsigned ALIGN = 8>
    Kmem_slab_for_size<SIZE, ALIGN> *
    lookup(bool panic_on_fail)
    {
      using Kmem_slab_type = Kmem_slab_for_size<SIZE, ALIGN>;
      auto g = lock_guard(_lock);
      return _lookup<Kmem_slab_type>(SIZE, ALIGN, false, panic_on_fail);
    }

    template <size_t SIZE, unsigned ALIGN = 8>
    Kmem_slab_for_size<SIZE, ALIGN> *
    create()
    {
      using Kmem_slab_type = Kmem_slab_for_size<SIZE, ALIGN>;
      auto g = lock_guard(_lock);
      return _create<Kmem_slab_type>(SIZE, ALIGN, false);
    }

  private:
    template <typename Kmem_slab_type>
    Kmem_slab_type *
    _create(size_t size, size_t align, bool for_type)
    {
      if (auto *slab = _lookup<Kmem_slab_type>(size, align, for_type, false))
        return slab;

      for (unsigned i = 0; i < Max_slabs_for_size_align; ++i)
        if (_slabs_for_size_align[i].size == 0)
          {
            // Kmem_alloc::alloc() requires to allocate at least 8 bytes and
            // sizeof(Kmem_slab_t<T>) with sizeof(T) >= 0x400 is only 1 byte!
            void *p = Kmem_alloc::allocator()
                        ->alloc(Bytes(max<size_t>(8, sizeof(Kmem_slab_type))));
            if (p)
              {
                new (p) Kmem_slab_type();
                _slabs_for_size_align[i].size = size;
                _slabs_for_size_align[i].align = align;
                _slabs_for_size_align[i].slab = p;
                _slabs_for_size_align[i].for_type = for_type;
              }
            return static_cast<Kmem_slab_type *>(p);
          }

      panic("Utest::Slab_store::create: Cannot create slab for size=%#zx align=%#zx",
            size, align);
    }

    template <typename Kmem_slab_type>
    Kmem_slab_type *
    _lookup(size_t size, size_t align, bool for_type, bool panic_on_fail)
    {
      for (unsigned i = 0; i < Max_slabs_for_size_align; ++i)
        if (_slabs_for_size_align[i].size == size
            && _slabs_for_size_align[i].align == align
            && _slabs_for_size_align[i].for_type == for_type)
          return static_cast<Kmem_slab_type *>(_slabs_for_size_align[i].slab);

      if (panic_on_fail)
        panic("Utest::Slab_store::lookup: Cannot lookup slab for size=%#zx align=%#zx",
              size, align);
      return nullptr;
    }

    Slab_for_size_align _slabs_for_size_align[Max_slabs_for_size_align];
    Spin_lock<> _lock;
  };

  template <typename T>
  struct Deleter
  {
    void operator()(T *s) const
    {
      if (s)
        Kmem_slab_t_singleton<T>::del(s);
    }
  };

  /**
   * Dynamically allocated array of bools with the size equal to the maximal
   * number of CPUs.
   */
  struct Bool_cpu_array
  {
    typedef cxx::array<bool, Cpu_number, Config::Max_num_cpus> array_type;

    Bool_cpu_array()
    {
      array = Utest::kmem_create_clear<array_type>();
    }

    bool &operator[](Cpu_number const &cpu)
    {
      return (*array)[cpu];
    }

    bool const &operator[](Cpu_number const &cpu) const
    {
      return (*array)[cpu];
    }

  private:
    cxx::unique_ptr<array_type, Utest::Deleter<array_type>> array;
  };

  enum : Unsigned32
  {
    Timer_period = Config::Scheduler_one_shot
                   ? Unsigned32{Config::Rcu_grace_period}
                   : Unsigned32{Config::Scheduler_granularity},
  };

  /// Priority of the main test thread, unless the test makes use of
  /// init_unittest_exclusive.
  static constexpr unsigned short Main_thread_prio = Config::Default_prio;

  /// Support for running tests with disabled timer tick.
  struct Tick_disabler
  {
    Tick_disabler();
    ~Tick_disabler();

    static bool supported();
    static void wait(unsigned long ms);
    static void wait_timer_periods(unsigned long periods);
    static bool wait_for_event(bool const *done, unsigned long timeout);
    static bool wait_for_events(Bool_cpu_array const &done, unsigned long timeout);

  private:
    static void wait_us(Unsigned64 us);
    static Unsigned64 timestamp();

    static Per_cpu<Unsigned32> count_prev;
    static Per_cpu<Unsigned64> count_epoch;
  };

  /**
   * Returns a value which is changed by the timer interrupt handler.
   *
   * If the returned values of two invocations are equal, no timer interrupt was
   * triggered between the two function invocations.
   */
  static Unsigned64 timer_interrupt_indicator();
  static Global_data<Slab_store> slab_store;
};

/**
 * Format class useful for printf-formatted statements in assertion text.
 *
 * As a side effect, this class allocates the string buffer using kmem_create()
 * to save (rare) kernel stack. The buffer has a fixed size which should be
 * sufficient for regular usage.
 */
struct Utest_fmt
{
  struct Buffer { char b[1024]; };
  using Buffer_ptr = decltype(Utest::kmem_create<Buffer>());

  __attribute__((format(printf,2,3))) Utest_fmt(char const *fmt, ...)
  {
    va_list args;

    va_start(args, fmt);
    (void)vsnprintf(msg->b, sizeof(Buffer), fmt, args);
    va_end(args);
  }

  char const * operator()() const
  { return msg->b; }

  Buffer_ptr msg = Utest::kmem_create<Buffer>();
};

/*
 * Printf wrapper filtering depending on verbosity setting.
 */
class Utest_debug
{
public:
  static int printf(char const *fmt, ...) __attribute__((format(printf,1,2)));
};

/**
 * Printer for tap output.
 *
 * The class provides convenience functions for test assertions and
 * other TAP output and counts the tests to be able to provide an appropriate
 * TAP footer.
 *
 * To use the class, instantiate it once and call start() to create an
 * appropriate TAP header. Then the various print functions may be used.
 * At the end finish() needs to be called exactly once to print the footer.
 */
class Utest_fw
{
public:
  /// Data structure to store the unit test information.
  struct External_info
  {
    // Action to execute after test finished
    enum Action : unsigned
    {
      None = 0,
      Reboot = 1,
      Shutdown = 2,
    };

    // command line parameters
    bool verbose = false;
    Action on_finished = Action::None;
    bool debug   = false;
//     bool record  = false; // unused, but specified user-land interface

    // Hardware information
    Unsigned32 cores = 0;
  };

  static Global_data<Utest_fw> tap_log;
  static Global_data<External_info> ext_info;

  void test_name(char const *name) { _test_name = name; }
  void group_name(char const *name) { _group_name = name; }

  char const *test_name() const
  { return _test_name ? _test_name : "No_name"; }

  char const *group_name() const
  { return _group_name ? _group_name : "No_group"; }

private:
  int _num_tests = 0;
  int _sum_failed = 0;
  unsigned _instance_counter = 0;
  // prevent printing of multiple TAP lines per test
  bool _tap_line_printed = false;
  char const *_group_name = nullptr;
  char const *_test_name = nullptr;
  char const *_test_uuid = nullptr;
};

// --- unit test macros -------------------------------------------------------

/**
 * F:    Utest::Assert or Utest::Expect.
 * ACT:  Actual expression to test.
 * MSG:  Message to be printed in case of failure.
 */
#define UTEST_TRUE(F, ACT, MSG)                                              \
  do                                                                         \
    {                                                                        \
      auto act_res = static_cast<bool>(ACT);                                 \
      Utest_fw::tap_log->binary_cmp(F, true == act_res, "true", #ACT, true,  \
                                    act_res, "==", MSG, __FILE__, __LINE__); \
    } while (false)

#define UTEST_FALSE(F, ACT, MSG)                                               \
  do                                                                           \
    {                                                                          \
      auto act_res = static_cast<bool>(ACT);                                   \
      Utest_fw::tap_log->binary_cmp(F, false == act_res, "false", #ACT, false, \
                                    act_res, "==", MSG, __FILE__, __LINE__);   \
    } while (false)

/**
 * F:    Utest::Assert or Utest::Expect.
 * LHS:  Left-hand side operand.
 * RHS:  Right-hand side operand.
 * MSG:  Message to be printed in case of failure.
 */
#define UTEST_EQ(F, LHS, RHS, MSG)                                             \
  do                                                                           \
    {                                                                          \
      auto rhs_res = (RHS);                                                    \
      auto lhs_res = (LHS);                                                    \
      Utest_fw::tap_log->binary_cmp(F, lhs_res == rhs_res, #LHS, #RHS, lhs_res,\
                                    rhs_res, "==", MSG, __FILE__, __LINE__);   \
    } while (false)

#define UTEST_NE(F, LHS, RHS, MSG)                                             \
  do                                                                           \
    {                                                                          \
      auto rhs_res = (RHS);                                                    \
      auto lhs_res = (LHS);                                                    \
      Utest_fw::tap_log->binary_cmp(F, lhs_res != rhs_res, #LHS, #RHS, lhs_res,\
                                    rhs_res, "!=", MSG, __FILE__, __LINE__);   \
    } while (false)

#define UTEST_LT(F, LHS, RHS, MSG)                                            \
  do                                                                          \
    {                                                                         \
      auto rhs_res = (RHS);                                                   \
      auto lhs_res = (LHS);                                                   \
      Utest_fw::tap_log->binary_cmp(F, lhs_res < rhs_res, #LHS, #RHS, lhs_res,\
                                    rhs_res, "<", MSG, __FILE__, __LINE__);   \
    } while (false)

#define UTEST_LE(F, LHS, RHS, MSG)                                             \
  do                                                                           \
    {                                                                          \
      auto rhs_res = (RHS);                                                    \
      auto lhs_res = (LHS);                                                    \
      Utest_fw::tap_log->binary_cmp(F, lhs_res <= rhs_res, #LHS, #RHS, lhs_res,\
                                    rhs_res, "<=", MSG, __FILE__, __LINE__);   \
    } while (false)

#define UTEST_GE(F, LHS, RHS, MSG)                                             \
  do                                                                           \
    {                                                                          \
      auto rhs_res = (RHS);                                                    \
      auto lhs_res = (LHS);                                                    \
      Utest_fw::tap_log->binary_cmp(F, lhs_res >= rhs_res, #LHS, #RHS, lhs_res,\
                                    rhs_res, ">=", MSG, __FILE__, __LINE__);   \
    } while (false)

#define UTEST_GT(F, LHS, RHS, MSG)                                             \
  do                                                                           \
    {                                                                          \
      auto rhs_res = (RHS);                                                    \
      auto lhs_res = (LHS);                                                    \
      Utest_fw::tap_log->binary_cmp(F, lhs_res > rhs_res, #LHS, #RHS, lhs_res, \
                                    rhs_res, ">", MSG, __FILE__, __LINE__);    \
    } while (false)

#define UTEST_NOERR(F, ACT, MSG)                                               \
  do                                                                           \
    {                                                                          \
      auto act_res = (ACT).ok();                                               \
      Utest_fw::tap_log->binary_cmp(F, true == act_res, "true", #ACT ".ok()",  \
                                    true, act_res, "==", MSG, __FILE__,        \
                                   __LINE__);                                  \
    } while (false)

//---------------------------------------------------------------------------
IMPLEMENTATION:

#include <feature.h>
#include "mem.h"
#include "poll_timeout_kclock.h"
#include "timer_tick.h"

DEFINE_GLOBAL Global_data<Utest::Slab_store> Utest::slab_store;

/**
 * Define a feature placeholder for external information filled in by
 * bootstrap. "utest_opts=" is intended to recognize the placeholder.
 * structure: verbose, restart, debug, reserved flags
 *            4 digit core count
 */
KIP_KERNEL_FEATURE(
    "utest_opts=aaaaaaaa");

/**
 * Single instance of the test interface.
 */
DEFINE_GLOBAL Global_data<Utest_fw> Utest_fw::tap_log;

/**
 * External information structure.
 */
DEFINE_GLOBAL Global_data<Utest_fw::External_info> Utest_fw::ext_info;

/**
 * Static members for emulating 64bit timestamps
 * on platforms that only provide 32bit timestamps
 * in hardware.
 */
DEFINE_PER_CPU Per_cpu<Unsigned32> Utest::Tick_disabler::count_prev;
DEFINE_PER_CPU Per_cpu<Unsigned64> Utest::Tick_disabler::count_epoch;

/**
 * Parse the KIP feature prefixed with utest_opts containing external
 * information for the framework.
 */
PUBLIC inline
void
Utest_fw::parse_feature_string()
{
  char const *prefix = "utest_opts=";
  unsigned const prefix_len = strlen(prefix);

  // features strings start behind the version string.
  char const *feature =  Kip::k()->version_string();
  feature += strlen(feature) + 1; // advance ptr to first feature string.

  for (; *feature; feature += strlen(feature) + 1)
    {
      if (0 != strncmp(feature, prefix, prefix_len))
        continue;

      feature += prefix_len; // first placeholder character;

      // placeholder not changed, return.
      if (feature[0] == 'a')
        return;

      // decode the string
      ext_info->verbose = feature[0] == '1';
      ext_info->on_finished =
        static_cast<External_info::Action>(feature[1] - '0');
      ext_info->debug   = feature[2] == '1';
      // unused, but specified user-land interface
//      ext_info->record  = feature[3] == '1';

      if (feature[4] != 'a')
        {
          // parse a 4 digit number from single characters
          ext_info->cores =
            (feature[4] - '0') * 1000 + (feature[5] - '0') * 100
            + (feature[6] - '0') * 10 + (feature[7] - '0');
        }

      break;
    }

  Utest_debug::printf("Utest_fw: external config\n"
                      "verbose %d, on_finished %d, debug %d, cores %u\n",
                      ext_info->verbose, ext_info->on_finished,
                      ext_info->debug, ext_info->cores);
}

/**
 * Print the TAP header and set a group and test name.
 *
 * This function needs to be called once before any tests are executed.
 */
PUBLIC inline
void
Utest_fw::start(char const *group = nullptr, char const *test = nullptr)
{
  parse_feature_string();
  name_group_test(group, test);
  printf("\nKUT TAP TEST START\n");
}

/**
 * Print the TAP footer.
 *
 * This function needs to be called once after all tests have run.
 */
PUBLIC inline
void
Utest_fw::finish(char const *skip_msg = nullptr)
{
  // finish previous test if there is one.
  if (_num_tests > 0)
    test_done();

  if (cov_print)
    cov_print();

  if (_sum_failed && ext_info->debug)
    kdb_ke("Test finish with failure & Debug flag set.");

  if (skip_msg)
    printf("\nKUT 1..%i #SKIP %s\n", _num_tests, skip_msg);
  else
    printf("\nKUT 1..%i\n", _num_tests);

  printf("\nKUT TAP TEST FINISHED\n");

  // QEMU platforms terminate after the FINISH line. Only hardware tests
  // progress to this point and might want to shutdown or restart the HW
  // platform.
  switch (ext_info->on_finished)
  {
  case External_info::Action::Reboot:
    Platform_control::system_reboot();
    break;
  case External_info::Action::Shutdown:
    Platform_control::system_off();
    break;
  default:
    // Nothing
    break;
  }

  // Exit kernel without calling destructors.
  _exit(_sum_failed);
}

/// Use a new `group` and `test` name for subsequent TAP output.
PUBLIC inline
void
Utest_fw::name_group_test(char const *group, char const *test)
{
  _group_name = group;
  _test_name = test;
}

/**
 * Start a new test with new `group` and `test` name and a mandatory UUID.
 *
 * When the same group and test name are used as the previous test, an instance
 * counter is incremented.
 *
 * \note It is forbidden to use the same group and test name for
 *       non-consecutive tests. This also contradicts a shuffle feature.
 */
PUBLIC inline
void
Utest_fw::new_test(char const *group, char const *test,
                   char const *uuid)
{
  // finish previous test if this isn't the first.
  if (_num_tests > 0)
    test_done();

  ++_num_tests;
  // initialize for new test.
  _tap_line_printed = false;

  test_uuid(uuid);

  // same group and test name as before.
  if (   group && test && _group_name && _test_name
      && !strcmp(group, _group_name) && !strcmp(test, _test_name))
    ++_instance_counter;
  else
    {
      _instance_counter = 0;
      name_group_test(group, test);
    }

  Utest_debug::printf("New test %s::%s/%u\n", group, test, _instance_counter);
}

/// Emit a TAP line for the current test, if the TAP Line wasn't printed before.
PUBLIC inline
void
Utest_fw::test_done()
{
  // tap_msg() checks if a TAP line was already printed, if it wasn't the test
  // was successful.
  tap_msg(true);
}

/// Set UUID of the current test
PUBLIC inline
void
Utest_fw::test_uuid(char const *uuid)
{
  _test_uuid = uuid;
}

/**
 * Print a TAP result line.
 *
 * \param success    Print an ok or not ok prefix.
 * \param msg        (Optional) A message to print with the `todo_skip` marker.
 * \param todo_skip  (Optional) A TAP TODO or SKIP marker.
 *
 * \note This is printed once for each test. The first failing statement prints
 *       this line.
 */
PUBLIC inline
void
Utest_fw::tap_msg(bool success,
                  char const *msg = nullptr,
                  char const *todo_skip = nullptr)
{
  // Print a TAP line only once per test.
  if (_tap_line_printed)
    return;

  _tap_line_printed = true;

  // Print in a single printf statement to avoid line splitting in SMP setups.
  if (_instance_counter == 0)
    printf("\nKUT %s %i %s::%s%s%s%s\n", success ? "ok" : "not ok", _num_tests,
           group_name(), test_name(),
           (todo_skip || msg) ? " " : "",
           todo_skip ? todo_skip : "",
           msg ? msg : "");
  else
    printf("\nKUT %s %i %s::%s/%u%s%s%s\n", success ? "ok" : "not ok", _num_tests,
           group_name(), test_name(),
           _instance_counter,
           (todo_skip || msg) ? " " : "",
           todo_skip ? todo_skip : "",
           msg ? msg : "");

  if (_test_uuid)
    printf("\nKUT # Test-uuid: %s\n\n", _test_uuid);
}

/**
 * Print a TAP TODO message.
 *
 * \param msg  Message to print.
 *
 * A TODO test is always considered a failure. The failure count is
 * increased accordingly.
 *
 * \note tap_todo() must be called after new_test() and before the first
 *       UTEST_ macro.
 */
PUBLIC inline
void
Utest_fw::tap_todo(char const *msg)
{
  // If tap_msg() was called before for this test, this call has no effect.
  tap_msg(false, msg, "# TODO ");

  ++_sum_failed;
}

/**
 * Print a TAP SKIP message.
 *
 * \param msg  Message to print.
 *
 * A SKIP test is always considered sucessful.
 *
 * \note tap_skip() must be called after new_test() and before the first
 *       UTEST_ macro.
 */
PUBLIC inline
void
Utest_fw::tap_skip(char const *msg)
{
  // If tap_msg() was called before for this test, this call has no effect.
  tap_msg(true, msg, "# SKIP ");
}

/**
 * Immediately print a TAP message with result 'not ok' and a result
 * explanation.
 *
 * \param lhs      First operand.
 * \param rhs      Second operand.
 * \param lhs_str  String representation of the first operand.
 * \param op       Operand as string.
 * \param rhs_str  String representation of the second operand.
 * \param msg      Message to print with the TAP output.
 */
PUBLIC template <typename A, typename B> inline
void
Utest_fw::tap_msg_bad(A &&lhs, B &&rhs,
                      char const *lhs_str, char const *op,
                      char const *rhs_str, char const *msg,
                      char const *file, int line)
{
  tap_msg(false);

  printf("\nKUT # Assertion failure: %s:%i\n", file, line);
  print_eval("LHS", cxx::forward<A>(lhs), lhs_str);
  print_eval("RHS", cxx::forward<B>(rhs), rhs_str);

  printf("\nKUT #\t%s %s %s\n", lhs_str, op, rhs_str);
  printf("\nKUT # %s\n", msg);
}

/**
 * Comparison function for binary comparison of two operands.
 *
 * Serves as back end implementation for UTEST_EQ/NE/... macros.
 *
 * \param finish_on_failure  Finish the test run on failure.
 * \param result             Result of the comparison operation `op`.
 * \param lhs_str            String representation of `lhs`.
 * \param rhs_str            String representation of `rhs`.
 * \param lhs                First operand.
 * \param rhs                Second operand.
 * \param op                 Operand as string.
 * \param msg                Message to print with the TAP output.
 * \param file               File name of caller aka __FILE__.
 * \param line               Line number of caller aka __LINE__.
 */
PUBLIC template <typename A, typename B> inline
void
Utest_fw::binary_cmp(bool finish_on_failure, bool result,
                     char const *lhs_str, char const *rhs_str,
                     A &&lhs, B &&rhs,
                     char const *op, char const *msg,
                     char const *file, int line)
{
  if (result)
    {
      // Print debug output, not a TAP line.
      Utest_debug::printf("%s::%s/%u - %s (line %i)\n", group_name(),
                          test_name(), _instance_counter, msg, line);
    }
  else
    {
      tap_msg_bad(cxx::forward<A>(lhs), cxx::forward<B>(rhs), lhs_str, op,
                  rhs_str, msg, file, line);
      ++_sum_failed;
      if (finish_on_failure)
        finish();
    }
}

/**
 * \copydoc Utest_fw::binary_cmp.
 *
 * This function accepts the reference to a Utest_fmt object allowing to
 * specify a message string constructed at runtime.
 */
PUBLIC template <typename A, typename B> inline
void
Utest_fw::binary_cmp(bool finish_on_failure, bool result,
                     char const *lhs_str, char const *rhs_str,
                     A &&lhs, B &&rhs,
                     char const *op, Utest_fmt const &fmt,
                     char const *file, int line)
{
  binary_cmp(finish_on_failure, result, lhs_str, rhs_str, lhs, rhs, op, fmt(),
             file, line);
}

PUBLIC template <typename A> inline
void
Utest_fw::print_eval(char const *eval, A &&val, char const *str) const
{
  printf("\nKUT # \t%s: ", eval);
  utest_format_print_value(val);
  printf("\t(%s)\n", str);
}

/**
 * Print an error message and abort the test if `obj` is null.
 *
 * This function shall be used to verify conditions in unit test setup /
 * cleanup code which are not relevant for the actual test.
 *
 * \param obj     Pointer which must be non-null.
 * \param msg     Message to be printed in case of failure.
 */
PUBLIC template <typename T, typename D> static inline
void
Utest_fw::chk(cxx::unique_ptr<T, D> const &obj, char const *msg)
{
  return chk(obj != nullptr, msg);
}

/**
 * Print an error message and abort the test if `obj` is null.
 *
 * This function shall be used to verify conditions in unit test setup /
 * cleanup code which are not relevant for the actual test.
 *
 * \param obj     Pointer which must be non-null.
 * \param msg     Message to be printed in case of failure.
 */
PUBLIC template <typename T, typename D> static inline
void
Utest_fw::chk(cxx::unique_ptr<T, D> const &obj, Utest_fmt const &msg)
{
  return chk(obj != nullptr, msg);
}

/**
 * Print an error message and abort the test if `result` is false.
 *
 * This function shall be used to verify conditions in unit test setup /
 * cleanup code which are not relevant for the actual test.
 *
 * \param result  Result which must be true.
 * \param msg     Message to be printed in case of failure.
 */
PUBLIC static inline
void
Utest_fw::chk(bool result, char const *msg)
{
  if (EXPECT_FALSE(!result))
    Utest_fw::tap_log->check_failed(msg);
}

/**
 * Print an error message and abort the test if `result` is false.
 *
 * This function shall be used to verify conditions in unit test setup /
 * cleanup code which are not relevant for the actual test.
 *
 * \param result  Result which must be true.
 * \param msg     Message to be printed in case of failure.
 */
PUBLIC static inline
void
Utest_fw::chk(bool result, Utest_fmt const &msg)
{
  if (EXPECT_FALSE(!result))
    Utest_fw::tap_log->check_failed(msg());
}

/**
 * Handle failures during test setup and immediately abort the tests.
 *
 * Serves as back end implementation for utest_check().
 *
 * \param msg  Message to print with TAP output.
 */
PUBLIC
void
Utest_fw::check_failed(char const *msg)
{
  tap_msg(false);
  printf("\nKUT # Failure in test setup: %s\n", msg);
  ++_sum_failed;
  finish();
}

IMPLEMENT inline
int
Utest_debug::printf(char const *fmt, ...)
{
  if (!Utest_fw::ext_info->verbose)
    return 0;

  int ret;
  va_list args;

  va_start(args, fmt);
  ret = vprintf(fmt, args);
  va_end(args);

  return ret;
}

template <typename T> inline
void
utest_format_print_value(T const &val)
{
  if constexpr (cxx::is_pointer_v<T> || cxx::is_null_pointer_v<T>)
    utest_format_print_value(reinterpret_cast<unsigned long>(val));
  else
    utest_format_print_value(static_cast<unsigned long>(cxx::int_value<T>(val)));
}

inline
void
utest_format_print_value(L4_error const &val) { printf("0x%lx\n", val.raw()); }

inline
void
utest_format_print_value(int val) { printf("%d", val); }

inline
void
utest_format_print_value(long val) { printf("%li", val); }

inline
void
utest_format_print_value(long long val) { printf("%lli", val); }

inline
void
utest_format_print_value(unsigned val) { printf("%u", val); }

inline
void
utest_format_print_value(unsigned long val) { printf("0x%lx", val); }

inline
void
utest_format_print_value(unsigned long long val) { printf("0x%llx", val); }

inline
void
utest_format_print_value(char val) { printf("%c", val); }

inline
void
utest_format_print_value(unsigned char val) { printf("%u", val); }

inline
void
utest_format_print_value(short val) { printf("%d", val); }

inline
void
utest_format_print_value(unsigned short val) { printf("%u", val); }

inline
void
utest_format_print_value(bool val) { printf("%d", val); }

inline
void
utest_format_print_value(char const *val) { printf("%s", val); }

inline
void
utest_format_print_value(char *val) { printf("%s", val); }

/**
 * Return the next online CPU starting at `cpu`.
 *
 * \param[in,out] cpu  First CPU to consider as the next online CPU.
 *                     Returns the next online CPU, if any is found.
 *
 * \retval True  A next online CPU was found.
 * \retval False No next online CPU was found.
 */
PUBLIC static inline
bool
Utest::next_online_cpu(Cpu_number *cpu)
{
  for (; *cpu < Config::max_num_cpus(); ++(*cpu))
    if (Cpu::present_mask().get(*cpu))
      return true;

  return false;
}

/**
 * Return the nth online CPU starting at the first CPU.
 *
 * \param n  Number of the online CPU (0 = first, 1 = second, ...)
 * \return The CPU number of the nth online CPU or Cpu_number::nil if the nth
 *         online CPU was not found.
 */
PUBLIC static inline
Cpu_number
Utest::nth_online_cpu(unsigned n)
{
  Cpu_number cpu = Cpu_number::first();
  for (;;)
    {
      if (!next_online_cpu(&cpu))
        return Cpu_number::nil();
      if (!n--)
        return cpu;
      ++cpu;
    }
}

/**
 * Wait until all app CPUs are done booting.
 */
PUBLIC static
void
Utest::wait_for_app_cpus()
{
  /* Loop until the map of online CPUs doesn't change anymore */
  enum { Silent_period = 500 /*ms*/ };
  auto online_mask = Cpu::online_mask();
  for (;;)
    {
      Utest::wait(Silent_period);

      auto new_online_mask = Cpu::online_mask();
      auto cpu = Cpu_number::first();
      for (; cpu < Config::max_num_cpus(); ++cpu)
        if (new_online_mask.get(cpu) != online_mask.get(cpu))
          break;

      if (cpu >= Config::max_num_cpus())
        // Loop was not terminated early, thus the online state of all CPUs was
        // identical in the old and the new online_mask.
        return;

      online_mask = new_online_mask;
    }
}

/**
 * Wait for a defined period of time.
 *
 * \param ms  Milliseconds to wait.
 */
PUBLIC static
void
Utest::wait(unsigned long ms)
{
  // convert to system_clock us resolution.
  Unsigned64 us = static_cast<Unsigned64>(ms) * 1000;
  Unsigned64 ctime = Timer::system_clock();

  // Busy waiting is ok, as nothing else happens on this core anyway.
  while (Timer::system_clock() - ctime < us)
    Proc::pause();
}

/**
 * Wait for an event with a timeout.
 *
 * The event is indicated by a different thread by setting a shared
 * variable to true.
 *
 * \param done    Shared variable that is set by a different thread to
 *                indicate the event.
 * \param timeout Timeout for waiting (in ms).
 *
 * \retval True  The event occurred (before the timeout).
 * \retval False The timeout occurred before the event.
 */
PUBLIC static
bool
Utest::wait_for_event(bool const *done, unsigned long timeout)
{
  Poll_timeout_kclock pt(timeout * 1000);

  while (pt.test(!access_once(done)))
    Proc::pause();

  return !pt.timed_out();
}

/**
 * Wait for an event on all CPUs (except the current CPU) with a timeout.
 *
 * The event is indicated by a different thread on each CPU by setting
 * the appropriate index in a shared array to true. The current CPU and
 * any offline CPUs are skipped for this purpose.
 *
 * Note that we wait for the CPUs sequentially, thus the worst case wait
 * time is the timeout multiplied by the number of CPUs (minus one and minus
 * the number of offline CPUs).
 *
 * \param done    Shared array where each index is set by a different thread
 *                on a given CPU to indicate the event.
 * \param timeout Timeout for waiting (in ms).
 *
 * \retval True  The event occurred on all CPUs (before the timeout).
 * \retval False The timeout occurred before the event at least on one
 *               of the CPUs.
 */
PUBLIC static
bool
Utest::wait_for_events(Bool_cpu_array const &done, unsigned long timeout)
{
  bool occurred = true;

  for (auto i = Cpu_number::first(); i < Config::max_num_cpus(); ++i)
    {
      // Skip the current and offline CPUs.
      if (i == current_cpu() || !Cpu::online(i))
        continue;

      if (!wait_for_event(&done[i], timeout))
        {
          // The waiting has timed out on the i-th CPU.
          occurred = false;
          break;
        }
    }

  return occurred;
}

/**
 * Worker function for Utest::start_thread().
 */
PRIVATE static
template <typename F>
void
Utest::thread_fn()
{
  auto *args = reinterpret_cast<Thread_args<F> *>(current_thread()->user_ip());
  F fn = args->fn;
  write_now(&args->started, true);

  // Cf. Thread::user_invoke_generic(): Release CPU lock explicitly, because
  // * the context that switched to us holds the CPU lock
  // * we run on a newly-created stack without a CPU lock guard
  cpu_lock.clear();

  fn();

  kill_current_thread();
}

/**
 * Kill the current thread.
 */
PUBLIC static
void
Utest::kill_current_thread()
{
  auto guard = lock_guard(cpu_lock);
  Sched_context::rq.current().deblock(current()->sched(),
                                      current()->sched());
  Thread::do_leave_and_kill_myself();
}

/**
 * Start a kernel thread.
 *
 * \param fn    The thread code.
 * \param cpu   The CPU number to run this thread on.
 * \param prio  The priority of the thread.
 * \param thr   Pre-allocated thread, may be nullptr.
 * \retval true Thread was successfully started.
 * \retval false It was not possible to start the thread.
 *
 * \post This function affects memory allocation.
 *
 * The corresponding thread resources are automatically destroyed when the
 * thread function returns. Actually the thread destruction is delayed (RCU
 * grace period) so the thread remains visible for some time after termination.
 */
PUBLIC static
template <typename F>
bool
Utest::start_thread(F const &fn, Cpu_number cpu,
                    unsigned short prio, Thread_object *thr = nullptr)
{
  Thread_object *t = thr;
  if (!t)
    t = new (Ram_quota::root) Thread_object(Ram_quota::root);
  if (!t)
    return false;

  Thread_args<F> args(fn);

  // Undo prepare_switch_to() from Thread::Thread() to get a proper stack
  // alignment.
  t->reset_kernel_sp();
  t->prepare_switch_to(thread_fn<F>);

  // Fixed-priority scheduler only so far!
  L4_sched_param_fixed_prio sp;
  sp.sched_class = L4_sched_param_fixed_prio::Class;
  sp.quantum = Config::Default_time_slice;
  sp.prio = prio;

  Thread::Migration info;
  info.cpu = cpu;
  info.sp = &sp;

  // This call has two purposes:
  //  1. Set the user IP to the address of the local thread parameters. The
  //     user IP is not used (thread never leaves the kernel), hence that
  //     address is used to pass the required parameters.
  //  2. Remove the 'Thread_dead' state bit.
  t->ex_regs(reinterpret_cast<Address>(&args), 0UL, nullptr, nullptr, nullptr, 0);
  t->activate();

    {
      auto guard = lock_guard(cpu_lock);
      t->migrate(&info);
    }

  // Only leave this scope when 'args' was fully evaluated by 'thread_fn()'.
  while (!access_once(&args.started))
    Proc::pause();

  return true;
}

PUBLIC
template <typename T, Unsigned8 FILL, typename...A>
static cxx::unique_ptr<T, Utest::Deleter<T>>
Utest::kmem_create_fill(A&&... args)
{
  void *p = Kmem_slab_t_singleton<T>::alloc();
  if (p)
    {
      memset(p, FILL, sizeof(T));
      Mem::barrier(); // prevent the compiler from optimizing memset() away
      new (p) T(cxx::forward<A>(args)...);
    }
  return cxx::unique_ptr<T, Utest::Deleter<T>>(static_cast<T *>(p));
}


/**
 * Use the kernel allocator to create an object encapsulated by unique_ptr
 * where the object storage is initialized with 0x55 before the constructor is
 * called.
 *
 * Initializing the memory with 0x55 is supposed to trigger cases where the
 * object constructor does not initialize all class members. Code accessing
 * such uninitialized members could read zeroed memory which isn't guaranteed
 * in production.
 *
 * Alignment: `Kmem_slab_t` uses the buddy allocator for objects >= 1K and the
 * slab allocator for smaller objects. The `Kmem_slab_t` objects are aligned
 * with their size, for example a 16-byte chunk is 16-byte aligned.
 */
PUBLIC
template <typename T, typename... A>
static cxx::unique_ptr<T, Utest::Deleter<T>>
Utest::kmem_create(A&&... args)
{
  return kmem_create_fill<T, 0x55, A...>(cxx::forward<A>(args)...);
}

/**
 * Use the kernel allocator to create an object encapsulated by unique_ptr
 * where the object storage is cleared before the constructor is called.
 *
 * This is required for objects which are normally stored in the BSS and would
 * be implicitly initialized at kernel boot time.
 *
 * Alignment: `Kmem_slab_t` uses the buddy allocator for objects >= 1K and the
 * slab allocator for smaller objects. The `Kmem_slab_t` objects are aligned
 * with their size, for example a 16-byte chunk is 16-byte aligned.
 */
PUBLIC
template <typename T, typename... A>
static cxx::unique_ptr<T, Utest::Deleter<T>>
Utest::kmem_create_clear(A&&... args)
{
  return kmem_create_fill<T, 0x00, A...>(cxx::forward<A>(args)...);
}

IMPLEMENT_DEFAULT
Unsigned64
Utest::timer_interrupt_indicator()
{
  return Kip::k()->clock();
}

/**
 * Construct object that disables kernel tick. The timer
 * tick is reenabled again then the object is destructed
 * (e.g. when the object on stack goes out of scope).
 */
IMPLEMENT
Utest::Tick_disabler::Tick_disabler()
{
  auto guard = lock_guard(cpu_lock);
  Timer_tick::disable(current_cpu());
}

/**
 * Reenable kernel tick again.
 */
IMPLEMENT
Utest::Tick_disabler::~Tick_disabler()
{
    {
      auto guard = lock_guard(cpu_lock);
      Timer_tick::enable(current_cpu());
    }

  if (!cpu_lock.test())
    {
      // Wait for system clock to be updated once after re-enabling timer tick,
      // otherwise an immediately following `Utest::wait()` might wait much
      // shorter than expected.
      Utest::wait(1);
    }

  // In one-shot mode we need to re-program the timer. Trigger it immediately.
  // This might be too early but the timeout handler will take care.
  Timer::update_timer(Timer::system_clock() + 1);
}

/**
 * Wait for a defined period of time. This method is guaranteed
 * to work even if the kernel tick is disabled.
 *
 * \param ms  Milliseconds to wait.
 */
IMPLEMENT static
void
Utest::Tick_disabler::wait(unsigned long ms)
{
  wait_us(static_cast<Unsigned64>(ms) * 1000UL);
}

/**
 * Wait for a defined number of timer periods. This method is guaranteed
 * to work even if the kernel tick is disabled.
 *
 * \param periods Timer periods to wait.
 */
IMPLEMENT static
void
Utest::Tick_disabler::wait_timer_periods(unsigned long periods)
{
  Unsigned64 period = static_cast<Unsigned64>(Utest::Timer_period);
  Unsigned64 count = static_cast<Unsigned64>(periods);

  wait_us(period * count);
}

/**
 * Wait for a defined period of time in us. This method is guaranteed
 * to work even if the kernel tick is disabled.
 *
 * \param us  Microseconds to wait.
 */
IMPLEMENT static
void
Utest::Tick_disabler::wait_us(Unsigned64 us)
{
  Unsigned64 ctime = timestamp();

  // Busy waiting is ok, as nothing else happens on this core anyway.
  while (timestamp() - ctime < us)
    Proc::pause();
}

/**
 * Wait for an event with a timeout. This method is guaranteed
 * to work even if the kernel tick is disabled.
 *
 * The event is indicated by a different thread by setting a shared
 * variable to true.
 *
 * \param done    Shared variable that is set by a different thread to
 *                indicate the event.
 * \param timeout Timeout for waiting (in ms).
 *
 * \retval True  The event occurred (before the timeout).
 * \retval False The timeout occurred before the event.
 */
IMPLEMENT static
bool
Utest::Tick_disabler::wait_for_event(bool const *done, unsigned long timeout)
{
  Unsigned64 us = static_cast<Unsigned64>(timeout) * 1000;
  Unsigned64 ctime = timestamp();

  while (timestamp() - ctime < us)
    if (access_once(done))
      return true;

  return false;
}

/**
 * Wait for an event on all CPUs (except the current CPU) with a timeout.
 * This method is guaranteed to work even if the kernel tick is disabled.
 *
 * The event is indicated by a different thread on each CPU by setting
 * the appropriate index in a shared array to true. The current CPU and
 * any offline CPUs are skipped for this purpose.
 *
 * Note that we wait for the CPUs sequentially, thus the worst case wait
 * time is the timeout multiplied by the number of CPUs (minus one and minus
 * the number of offline CPUs).
 *
 * \param done    Shared array where each index is set by a different thread
 *                on a given CPU to indicate the event.
 * \param timeout Timeout for waiting (in ms).
 *
 * \retval True  The event occurred on all CPUs (before the timeout).
 * \retval False The timeout occurred before the event at least on one
 *               of the CPUs.
 */
IMPLEMENT static
bool
Utest::Tick_disabler::wait_for_events(Bool_cpu_array const &done,
                                      unsigned long timeout)
{
  bool occurred = true;

  for (auto i = Cpu_number::first(); i < Config::max_num_cpus(); ++i)
    {
      // Skip the current and offline CPUs.
      if (i == current_cpu() || !Cpu::online(i))
        continue;

      if (!Tick_disabler::wait_for_event(&done[i], timeout))
        {
          // The waiting has timed out on the i-th CPU.
          occurred = false;
          break;
        }
    }

  return occurred;
}

/**
 * Indicate whether tickless operation is supported.
 *
 * For the tickless operation to be supported, we need a reliable
 * plaform-specific timestamp counter that is independent from the
 * Timer_tick. We indicate no support by default and override the
 * implementation on a platform-specific basis.
 *
 * \return Indication whether tickless operation is supported.
 */
IMPLEMENT_DEFAULT static
bool
Utest::Tick_disabler::supported()
{
  return false;
}

/// Dummy timestamp implementation.
IMPLEMENT_DEFAULT static
Unsigned64
Utest::Tick_disabler::timestamp()
{
  return 0;
}

extern "C" void init_unittest() __attribute__((weak));

/**
 * Entry point defined by tests that require to be executed in the idle thread
 * of the boot CPU. Use with caution.
 */
extern "C" void init_unittest_exclusive() __attribute__((weak));

extern "C" void
init_unittest_threaded()
{
  if (init_unittest_exclusive)
    init_unittest_exclusive();
  else if (init_unittest)
    {
      Thread_object *t = new (Ram_quota::root) Thread_object(Ram_quota::root);
      Utest_fw::chk(t, "Create thread for executing the kunit tests");

      // Keep reference until thread has terminated.
      Ref_ptr<Thread_object> thread_ref(t);
      Utest_fw::chk(Utest::start_thread(&init_unittest, current_cpu(),
                                        Utest::Main_thread_prio, t),
                    "Start kernel thread executing the kunit tests");

      while (!(t->state() & Thread_dead))
        Proc::pause();
    }
  else
    panic("Neither init_unittest() nor init_unittest_exclusive() defined!");
}

INIT_WORKLOAD(INIT_WORKLOAD_PRIO_UNIT_TEST, init_unittest_threaded);

//---------------------------------------------------------------------------
IMPLEMENTATION[ia32 || amd64]:

/// Tickless operation is supported on IA-32 and AMD64.
IMPLEMENT_OVERRIDE static
bool
Utest::Tick_disabler::supported()
{
  return true;
}

/**
 * Get current timestamp in us.
 *
 * \return Current timestamp in us.
 */
IMPLEMENT_OVERRIDE static
Unsigned64
Utest::Tick_disabler::timestamp()
{
  return Cpu::cpus.cpu(current_cpu()).time_us();
}

//---------------------------------------------------------------------------
IMPLEMENTATION[mips]:

/// Tickless operation is supported on MIPS.
IMPLEMENT_OVERRIDE static
bool
Utest::Tick_disabler::supported()
{
  return true;
}

/**
 * Get current timestamp in us.
 *
 * \return Current timestamp in us.
 */
IMPLEMENT_OVERRIDE static
Unsigned64
Utest::Tick_disabler::timestamp()
{
  Unsigned32 count = Mips::mfc0_32(Mips::Cp0_count);

  /*
   * This is a crude overflow solution that creates an illusion
   * that the counter increases monotonically not only up to 2^32 - 1
   * (which wraps around in a magnitude of seconds), but up to 2^64 - 1
   * (which wraps around in a magnitude of hundreds of years).
   *
   * Of course, if this method is called with a period longer than
   * the underlying wraparound period, the wraparound event could be
   * completely missed and the actual number of the wraparound events
   * cannot be known. This leads to inaccurate time measurements
   * (slower time passage than in reality). However, we assume that
   * this method is always called in a tight busy loop and therefore
   * the potentially missed wraparound events are not a real problem.
   */
  if (count_prev.current() > count)
    count_epoch.current() += static_cast<Unsigned32>(~0UL);

  count_prev.current() = count;

  Unsigned64 freq = Cpu::frequency() / 2;

  return ((count_epoch.current() + static_cast<Unsigned64>(count)) * 1000000UL)
         / freq;
}

//---------------------------------------------------------------------------
IMPLEMENTATION[arm && arm_generic_timer]:

/// Tickless operation is supported on ARM with a generic timer.
IMPLEMENT_OVERRIDE static
bool
Utest::Tick_disabler::supported()
{
  return true;
}

/**
 * Get current timestamp in us.
 *
 * \return Current timestamp in us.
 */
IMPLEMENT_OVERRIDE static
Unsigned64
Utest::Tick_disabler::timestamp()
{
  Unsigned64 count = Generic_timer::Gtimer::counter();
  Unsigned64 freq = Generic_timer::Gtimer::frequency();

  return (count * 1000000UL) / freq;
}

//---------------------------------------------------------------------------
IMPLEMENTATION[riscv]:

/// Tickless operation is supported on RISC-V.
IMPLEMENT_OVERRIDE static
bool
Utest::Tick_disabler::supported()
{
  return true;
}

/**
 * Get current timestamp in us.
 *
 * \return Current timestamp in us.
 */
IMPLEMENT_OVERRIDE static
Unsigned64
Utest::Tick_disabler::timestamp()
{
  Unsigned32 frequency = Kip::k()->platform_info.arch.timebase_frequency;
  return (Cpu::rdtime() * 1000000ULL) / (frequency);
}

//---------------------------------------------------------------------------
IMPLEMENTATION[sync_clock]:

IMPLEMENT_OVERRIDE
Unsigned64
Utest::timer_interrupt_indicator()
{
  return Timer::system_clock_at_last_timer_tick();
}
