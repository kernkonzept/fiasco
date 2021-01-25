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

#include "l4_error.h"


extern void gcov_print() __attribute__((weak));


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
private:
  bool _verbose = false;
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
  static Utest_fw tap_log;

  void test_name(char const *name) { _test_name = name; }
  void group_name(char const *name) { _group_name = name; }

  char const *test_name() const
  { return _test_name ? _test_name : "No_name"; }

  char const *group_name() const
  { return _group_name ? _group_name : "No_group"; }

private:
  int _done = 0;
  int _failed = 0;
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

/**
 * Single instance of the test interface.
 */
Utest_fw Utest_fw::tap_log;


/**
 * Print the TAP header and set a group and test name.
 *
 * This function needs to be called once before any tests are executed.
 */
PUBLIC inline
void
Utest_fw::start(char const *group = nullptr, char const *test = nullptr)
{
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
  printf("\nKUT 1..%i\n", _done);
  printf("\nKUT TAP TEST FINISHED\n");

  if (gcov_print)
    gcov_print();

  exit(_failed);
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
 * Print a TAP result line.
 *
 * \param success  Print an ok or not ok prefix.
 * \param msg      Test message to include in the output.
 */
PUBLIC inline
void
Utest_fw::tap_msg(bool success, char const *msg) const
{
  printf("\nKUT %s %i %s::%s - %s\n", success ? "ok" : "not ok", _done,
         group_name(), test_name(), msg);
}

/**
 * Print a TAP TODO message.
 *
 * \param msg  Message to print.
 *
 * A TODO test is always considered a failure. The test count and failure
 * count are increased accordingly.
 */
PUBLIC inline
void
Utest_fw::tap_todo(char const *msg)
{
  ++_done;
  ++_failed;

  printf("\nKUT not ok %s::%s %i # TODO %s\n", group_name(), test_name(), _done,
         msg);
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
                      char const *file, int line) const
{
  tap_msg(false, msg);

  printf("\nKUT # Assertion failure: %s:%i\n", file, line);
  print_eval("LHS", cxx::forward<A>(lhs), lhs_str);
  print_eval("RHS", cxx::forward<B>(rhs), rhs_str);

  printf("\nKUT #\t%s %s %s\n",lhs_str, op, rhs_str);
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
  ++_done;

  if (result)
    tap_msg(true, msg);
  else
    {
      tap_msg_bad(cxx::forward<A>(lhs), cxx::forward<B>(rhs), lhs_str, op,
                  rhs_str, msg, file, line);
      ++_failed;
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

PUBLIC inline
int
Utest_debug::printf(char const *fmt, ...) const
{
  if (!_verbose)
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
