# The L4Re Microkernel Repository

This repository contains the source code of the L4Re microkernel (also known as
the Fiasco microkernel). User level applications are not included in this
package.

The L4Re microkernel is used to construct flexible systems that support
running real-time, time-sharing and virtualization workloads concurrently on
one system. The kernel scales from big and complex systems down to small and
embedded applications. It supports the following architectures:

| Architecture | 32 bit | 64 bit | Status            |
|:------------:|:------:|:------:|:-----------------:|
|      x86     |    x   |   x    | ![Build check][3] |
|      ARM     |    x   |   x    | ![Build check][4] |
|      MIPS    |    x   |   x    | ![Build check][5] |
|     RISC-V   |    x   |   x    | ![Build check][6] |

For a full list of the supported platforms and features see the [feature
list][1].

We welcome contributions to the microkernel. Please see our contributors guide
on [how to contribute][2].

[1]: https://l4re.org/fiasco/features.html
[2]: https://kernkonzept.com/L4Re/contributing/fiasco
[3]: https://github.com/kernkonzept/fiasco/actions/workflows/check_build_x86.yml/badge.svg?branch=master
[4]: https://github.com/kernkonzept/fiasco/actions/workflows/check_build_arm.yml/badge.svg?branch=master
[5]: https://github.com/kernkonzept/fiasco/actions/workflows/check_build_mips.yml/badge.svg?branch=master
[6]: https://github.com/kernkonzept/fiasco/actions/workflows/check_build_riscv.yml/badge.svg?branch=master

## Reporting vulnerabilities

We encourage responsible disclosure of vulnerabilities you may discover. Please
disclose them privately via **security@kernkonzept.com** to us.

# Building

Fiasco.OC can be built using a recent version of gcc (>=7) or clang (>=9),
GNU binutils, GNU make and Perl (>=5.6).

Change to the top-level directory of this project and create a build directory
by typing
```
$ make BUILDDIR=/path/to/build
```

Change to the newly created build directory. You can now modify the default
configuration by typing
```
$ make menuconfig
```

Make the desired changes, save and exit the configuration. Now you can build
the kernel by typing
```
$ make
```

You can also build in parallel by providing a suitable ```-j``` option. If the
build completed successfully you can find the kernel binary as *fiasco* in
the build directory.

For further information please refer to our [detailed build
instructions](https://l4re.org/fiasco/build.html).

# License

The L4Re microkernel is licensed under the GPLv2.
For other licensing options, please contact **info@kernkonzept.com**.
