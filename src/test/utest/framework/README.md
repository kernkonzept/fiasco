# Unit Test Framework

## File system structure

The `src/test/utest/` directory shall contain subdirectories containing one
`Modules.utest` each.
Further subdirectories within these subdirectories are not supported.
A Subdirectory will be called "test directory" henceforth.

The test directory names must not conflict with any .cpp file anywhere in the
system or any generated make targets.
If the test directory name conflicts, please append a `_ut` to the test
directory name.

All tests files in a test directory must start with `test_`.
Please group tests with the help of the naming structure, e.g.
`test_<unit>_<class>_<...>.cpp`


## Test directory

The `Modules.utest` file within a test directory is expected to at most contain
the following lines:
```
INTERFACES_UTEST +=
UTEST_SUPPL +=
```

`INTERFACES_UTEST` shall list all test files explicitly. Modules starting with
`test_` are considered as independent tests. Modules starting with `common_`
shall contain implemented classes relevant for multiple test modules.

`UTEST_SUPPL` shall list supplemental files required for test execution. The
following files are supported:

- `config_` files contain configuration information. The current supported use
  case is output matching of the test case by specifying a `TEST_TAP_PLUGINS`
  line. The parameters contain a reference to an expected file.

- `expected_` files contain the output the test case is expected to generate.

These files shall have the name of the test case where `test_` is replaced by
`config_` or `expected_`. The files can be architecture and bit-width specific.
See [Matching expected output of tests](#matching-expected-output-of-tests).

If a test file needs additional files, an
`\_IMPL` variable must be added. This variable must list all source files in
order. Example:

```
INTERFACES_UTEST += test_foo

test_foo_IMPL = test_foo test_foo-bar
```

### Dealing with architecture specifics

Some tests cannot be written architecture agnostic, some tests can only be
written for one architecture and some tests need to differentiate between
32-bit and 64-bit architectures.


#### Tests for only a single architecture

When you can write a unit test only for a specific architecture you do not need
to add any architecture identifier to the file name.
Instead add the file to one of the `UTEST_ARCH-` variables and extend the
`INTERFACES_UTEST` list as shown below.

```
UTEST_ARCH-arm =
UTEST_ARCH-mips =
UTEST_ARCH-ia32 =
UTEST_ARCH-amd64 =

INTERFACES_UTEST += $(UTEST_ARCH-$(CONFIG_XARCH))
```


#### Feature differences on the example of bit-sizes

If you need a bit-size specific function implementation, place them into
different IMPLEMENTATION sections in the corresponding architecture file.
If the specific function is not architecture dependent, the IMPLEMENTATION
sections are allowed to be in the test file.

```
IMPLEMENTATION [32bit]:
// ...

IMPLEMENTATION [64bit]:
// ...
```


#### Architecture specific parts for all tests

In general, the public interfaces should not differ in behavior across
architectures.
The above mentioned features define the behavior.
If there is only the architecture as behavior definition, this hints in
direction of a missing feature flag.

If this turns out to be the case, please record the feature flag and create
an IMPLEMENTATION section with a comment noting which feature flag is missing
there and that the architecture boundary is the current, pragmatic way to deal
with it.

Please keep the architecture specific part as small as possible.

```
INTERFACE:

// necessary includes and function declaration


# feature flag missing: <feature flag proposal>
IMPLEMENTATION [arm, mips]:
// ...

# feature flag missing: <feature flag proposal>
IMPLEMENTATION [ia32, amd64]:
// ...
```

If these IMPLEMENTATION sections grow beyond one simple function or constant,
please move them to separate, architecture specific files.

The architecture specific file must follow the naming convention
`test_foo_bar-$(CONFIG_XARCH).cpp`.
Then add the file to Modules.utest and create a `_IMPL` variable for the
corresponding test. Example:

```
INTERFACES_UTEST += test_foo_bar

test_foo_bar_IMPL := test_foo_bar test_foo_bar-$(CONFIG_XARCH)
```

There must be one `test_foo_bar-$(CONFIG_XARCH)` file for each architecture
present in the file system.


## test/utest/Modules.utest

The original Modules.utest file contains the code to collect and include all
Modules.utest files in subdirectories to test/utest.

No tests shall be added directly to this file.


## utest framework

The utest framework file `utest_fw.cpp` contains the interface to print TAP
formatted output as well as helper functions and macros to test the results of
an operation.

The format printer functions can be extended to print user-defined data types
within ASSERT/EXPECT output.
Just add an overload of `utest_format_print_value` to the test's .cpp file.

The same overload possibility applies to `Utest_binary_ops::eval`.

The framework terminates the test program on a failing assert statement.

It does not do any memory allocation and prints the result of each ASSERT
statement directly.
To allow later tooling to recognize the TAP report between other output, all
TAP messages are prefixed with "KUT ".

The TAP messages are structured as follows (without prefix):

```
ok <number> <group name>::<test name> - <msg>

not ok <number> <group name>::<test name> - <msg>
\# Assertion failure:
\#   Expected: ...
\#   Actual: ...
\#   <string representation of the performed comparison>
```

The group and test name structure is known from Atkins/gtest.
Note: all assert statements generate a single TAP line with the same
'group::test' identifier, just the number and <msg> will be different.

## Write a test

Define the symbol `void init_unittest()` and place your code in this function.

```
#include "utest_fw.h"

void
init_unittest()
{
  Utest_fw::tap_log.start();
  Utest_fw::new_test("group_name", "test_name");

  UTEST_EQ(Utest::Assert, A, B, <MSG>);

  Utest_fw::new_test("group_nameX", "test_nameX");

  UTEST_EQ(Utest::Assert, C, D, <MSG>);

  Utest_fw::tap_log.finish();
}
```

Instead of `Assert` you can use `Expect` as first macro parameter. Then the
test will not terminate, if the comparison returns false.

To allow for post-processing please provide a group and a test name to the
framework with `Utest_fw::tap_log.new_test()`.
This allows to create a TAP line for each finished test.
For tooling purposes, either directly define the `group_name` in the call to
`new_test()` or use a variable of `static char const *` type.

You need to call `Utest_fw::tap_log.finish()` at the end of the test suite.

All supported `UTEST_` macros are listed in `utest_fw.cpp`.

After you decided on a test filename which must start with "test_" add the
filename to `INTERFACES_UTEST` list in the `Modules.utest` file of your test
directory.


## Tests using multiple cores

To use multiple cores in your test you need to define `void
init_unittest_app_core()`.
Each core which is not the boot core will invoke this function if it is
defined.
When `init_unittest_app_core()` returns the core goes into idle.

Your test needs to decide which core shall do what.
It can decide to only use a single additional core, or all that register with
this function.
See the `Cpu` class on how to identify a core.

Currently, there is no central knowledge on how many cores the system contains,
so the test needs to be able to handle this.


## Matching expected output of tests

In some cases it might make sense that it's more convenient for the test case
writer to only a limited number of UTEST assert macros and to rather compare
the generated output of the test case with expected pattern, in particular when
the test case output can differ between the kernel architectures.

The L4Re tool `gen_kunit_test` detects the presence of `config_foo` in the
`utest/` subdirectory of the kernel build directory belonging to `test_foo.cpp`
and starts the unit test accordingly.

The `config_foo` file contains configuration information like this:
```
TEST_TAP_PLUGINS=TaggedOutputMatch:tag=mapdb,file=expected_mapdb,literal=1
```

For details about the syntax have a look at the `TaggedOutputMatch` plugin
in the L4Re tool directory.

To generate the referenced `expected_foo` file, it is required to start the
test without specifying the `expected_foo` file (e.g. by removing `config_foo`
from the `utest/` directory. The captured test output can be stored in an
`expected_foo` file.

It is possible to provide a single default `expected` file per test case, an
`expected-XARCH` file and an `expected-XARCH-BITS` file where `XARCH` and
`BITS` are replaced according to the kernel configuration.
