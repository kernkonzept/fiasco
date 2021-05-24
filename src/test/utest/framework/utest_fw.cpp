// vi:set ft=cpp: -*- Mode: C++ -*-
/* SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom */
/*
 * Copyright (C) 2021 Kernkonzept GmbH.
 * Author(s): Philipp Eppelt <philipp.eppelt@kernkonzept.com>
 */
INTERFACE:

#include <cxx/type_traits>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "timer.h"
#include "l4_error.h"
#include "processor.h"
#include "kip.h"
#include "reset.h"
#include "kdb_ke.h"


extern "C" void gcov_print() __attribute__((weak));


/// Utest namespace for constants
struct Utest
{
  /// Constants to use with the UTEST_ macros.
  enum : bool { Assert = true, Expect = false };
};

/*
 * Printf wrapper filtering depending on verbosity setting.
 */
class Utest_debug
{
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
    // command line parameters
    bool verbose = false;
    bool restart = false;
    bool debug   = false;

    // Hardware information
    Unsigned32 cores = 0;
  };

  static Utest_fw tap_log;
  static External_info ext_info;

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
};

// --- unit test macros -------------------------------------------------------

/**
 * F:    Utest::Assert or Utest::Expect.
 * ACT:  Actual expression to test.
 * MSG:  Message to be printed in case of failure.
 */
#define UTEST_TRUE(F, ACT, MSG)                                             \
  do                                                                        \
    {                                                                       \
      auto act_res = static_cast<bool>(ACT);                                \
      Utest_fw::tap_log.binary_cmp(F, true == act_res, "true", #ACT, true,  \
                                   act_res, "==", MSG, __FILE__, __LINE__); \
    } while (false)

#define UTEST_FALSE(F, ACT, MSG)                                              \
  do                                                                          \
    {                                                                         \
      auto act_res = static_cast<bool>(ACT);                                  \
      Utest_fw::tap_log.binary_cmp(F, false == act_res, "false", #ACT, false, \
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
      auto rhs_res = RHS;                                                      \
      auto lhs_res = LHS;                                                      \
      Utest_fw::tap_log.binary_cmp(F, lhs_res == rhs_res, #LHS, #RHS, lhs_res, \
                                   rhs_res, "==", MSG, __FILE__, __LINE__);    \
    } while (false)

#define UTEST_NE(F, LHS, RHS, MSG)                                             \
  do                                                                           \
    {                                                                          \
      auto rhs_res = RHS;                                                      \
      auto lhs_res = LHS;                                                      \
      Utest_fw::tap_log.binary_cmp(F, lhs_res != rhs_res, #LHS, #RHS, lhs_res, \
                                   rhs_res, "!=", MSG, __FILE__, __LINE__);    \
    } while (false)

#define UTEST_LT(F, LHS, RHS, MSG)                                            \
  do                                                                          \
    {                                                                         \
      auto rhs_res = RHS;                                                     \
      auto lhs_res = LHS;                                                     \
      Utest_fw::tap_log.binary_cmp(F, lhs_res < rhs_res, #LHS, #RHS, lhs_res, \
                                   rhs_res, "<", MSG, __FILE__, __LINE__);    \
    } while (false)

#define UTEST_LE(F, LHS, RHS, MSG)                                             \
  do                                                                           \
    {                                                                          \
      auto rhs_res = RHS;                                                      \
      auto lhs_res = LHS;                                                      \
      Utest_fw::tap_log.binary_cmp(F, lhs_res <= rhs_res, #LHS, #RHS, lhs_res, \
                                   rhs_res, "<=", MSG, __FILE__, __LINE__);    \
    } while (false)

#define UTEST_GE(F, LHS, RHS, MSG)                                             \
  do                                                                           \
    {                                                                          \
      auto rhs_res = RHS;                                                      \
      auto lhs_res = LHS;                                                      \
      Utest_fw::tap_log.binary_cmp(F, lhs_res >= rhs_res, #LHS, #RHS, lhs_res, \
                                   rhs_res, ">=", MSG, __FILE__, __LINE__);    \
    } while (false)

#define UTEST_GT(F, LHS, RHS, MSG)                                            \
  do                                                                          \
    {                                                                         \
      auto rhs_res = RHS;                                                     \
      auto lhs_res = LHS;                                                     \
      Utest_fw::tap_log.binary_cmp(F, lhs_res > rhs_res, #LHS, #RHS, lhs_res, \
                                   rhs_res, ">", MSG, __FILE__, __LINE__);    \
    } while (false)

// ---------------------------------------------------------------------------
IMPLEMENTATION:

#include <feature.h>
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
Utest_fw Utest_fw::tap_log;
/*
 * External information structure.
 */
Utest_fw::External_info Utest_fw::ext_info;

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
      ext_info.verbose = feature[0] == '1';
      ext_info.restart = feature[1] == '1';
      ext_info.debug   = feature[2] == '1';
      // feature[3] is reserved

      if (feature[4] != 'a')
        {
          // parse a 4 digit number from single characters
          ext_info.cores =
            (feature[4] - '0') * 1000 + (feature[5] - '0') * 100
            + (feature[6] - '0') * 10 + (feature[7] - '0');
        }

      break;
    }

  Utest_debug::printf("Utest_fw: external config\n"
                      "verbose %d, restart %d, debug %d, cores %u\n",
                      ext_info.verbose, ext_info.restart,
                      ext_info.debug, ext_info.cores);
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
Utest_fw::finish()
{
  // finish previous test if there is one.
  if (_num_tests > 0)
    test_done();

  if (gcov_print)
    gcov_print();

  if (_sum_failed && ext_info.debug)
    kdb_ke("Test finish with failure & Debug flag set.");

  printf("\nKUT 1..%i\n", _num_tests);
  printf("\nKUT TAP TEST FINISHED\n");

  // QEMU platforms terminate after the FINISH line. Only hardware tests
  // progress to this point and might want to restart the HW platform.
  if (ext_info.restart)
    platform_reset();

  exit(_sum_failed);
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
 * Start a new test with new `group` and `test` name.
 *
 * When the same group and test name are used as the previous test, an instance
 * counter is incremented.
 *
 * \note It is forbidden to use the same group and test name for
 *       non-consecutive tests. This also contradicts a shuffle feature.
 */
PUBLIC inline
void
Utest_fw::new_test(char const *group, char const *test)
{
  // finish previous test if this isn't the first.
  if (_num_tests > 0)
    test_done();

  ++_num_tests;
  // initialize for new test.
  _tap_line_printed = false;

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
    printf("\nKUT %s %i %s::%s %s%s\n", success ? "ok" : "not ok", _num_tests,
           group_name(), test_name(),
           todo_skip ? todo_skip : "",
           msg ? msg : "");
  else
    printf("\nKUT %s %i %s::%s/%u %s%s\n", success ? "ok" : "not ok", _num_tests,
           group_name(), test_name(),
           _instance_counter,
           todo_skip ? todo_skip : "",
           msg ? msg : "");
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

PUBLIC template <typename A> inline
void
Utest_fw::print_eval(char const *eval, A &&val, char const *str) const
{
  printf("\nKUT # \t%s: ", eval);
  utest_format_print_value(val);
  printf("\t(%s)\n", str);
}

PUBLIC static inline
int
Utest_debug::printf(char const *fmt, ...)
{
  if (!Utest_fw::ext_info.verbose)
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
  utest_format_print_value(cxx::int_value<T>(val));
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
utest_format_print_value(bool val) { printf("%d", val); }

inline
void
utest_format_print_value(char const *val) { printf("%s", val); }

inline
void
utest_format_print_value(char *val) { printf("%s", val); }

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
  while (Timer::system_clock() < ctime + us)
    Proc::pause();
}
