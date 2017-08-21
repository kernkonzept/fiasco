# The Fiasco.OC Microkernel Repository

This repository contains the source code of the L4Re microkernel also known as
the Fiasco.OC microkernel. User level applications are not included in this
package.

Fiasco is used to construct flexible systems that support running real-time,
time-sharing and virtualization workloads concurrently on one system. The
kernel scales from big and complex systems down to small and embedded
applications. It supports the following architectures:

| Architecture | 32 bit | 64 bit |
|:------------:|:------:|:------:|
|      x86     |    x   |   x    |
|      ARM     |    x   |   x    |
|      MIPS    |    x   |   x    |

For a full list of the supported platforms and features see the [feature
list](https://l4re.org/fiasco/features.html).

We welcome contributions to the microkernel. Please see our contributors guide on
[how to contribute](CONTRIBUTING.md).

## Reporting vulnerabilities

We encourage responsible disclosure of vulnerabilities you may discover. Please
disclose them privately via **security@kernkonzept.com** to us.

# Building

Fiasco.OC can be built using a recent version of gcc (>=4.7) or clang (>=3.7),
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

Fiasco is licensed under the GPLv2. If you require a different licensing scheme
please contact us at **info@kernkonzept.com**.
