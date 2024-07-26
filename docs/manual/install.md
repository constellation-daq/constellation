# Installing from Source

This document describes the procedure of how to install (and possibly compile) the Constellation framework from source.
The source code can be obtained from [the Constellation Git repository](https://gitlab.desy.de/constellation/constellation):

```sh
git clone https://gitlab.desy.de/constellation/constellation.git
```

Constellation has two separate implementations, one in C++ and one in Python. The tabs below discuss the installation
procedure for the two versions separately, but installing both in parallel on the same machine is possible and encouraged.

::::::{tab-set}
:::::{tab-item} C++

## Prerequisites for C++

The C++ version of Constellation requires:

- [Meson](https://mesonbuild.com/) 0.61 or newer
- C++20 capable compiler like GCC 12 or newer and clang 16 or newer
- C++20 enabled standard library (GCC's libstdc++ 12 or newer and LLVM's libc++ 18 or newer)

The prerequisites can be installed as follows:

::::{tab-set}
:::{tab-item} Debian/Ubuntu

Starting with Ubuntu 24.04 and Debian 12 or newer, the official packages for GCC and Meson can be used:

```sh
sudo apt install meson g++
```

Ubuntu 22.04 requires a newer version of GCC than installed by default. Version 12 is recommended and available in the
regular package repositories:

```sh
sudo apt install meson g++-12
export CXX="g++-12"
```

Ubuntu 20.04 requires newer versions of GCC and Meson than available from the standard package repositories. They are available in official PPAs:

```sh
sudo add-apt-repository ppa:ubuntu-toolchain-r/test
sudo add-apt-repository ppa:ubuntu-support-team/meson
sudo apt update
sudo apt install meson g++-13
export CXX="g++-13"
```

:::
:::{tab-item} ALMA/Fedora

```sh
sudo dnf install meson clang
export CXX=clang++
```

:::
:::{tab-item} MacOS 14

MacOS requires an installation of Meson and LLVM, e.g. via [Homebrew](https://brew.sh/):

```sh
brew install meson
brew install llvm
```

Assuming `${HOMEBREW_PREFIX}` is set (likely `/opt/homebrew`, can otherwise be found by typing e.g. `which meson`):

``` sh
export CXX="${HOMEBREW_PREFIX}/opt/llvm/bin/clang++"
export CC="${HOMEBREW_PREFIX}/opt/llvm/bin/clang"
export LDFLAGS="-L${HOMEBREW_PREFIX}/opt/llvm/lib/c++ -Wl,-rpath,${HOMEBREW_PREFIX}/opt/llvm/lib/c++"
export CXXFLAGS="-fexperimental-library -DASIO_HAS_SNPRINTF"
```

:::
:::{tab-item} Windows

TODO

:::
::::

## Building the C++ Version

```sh
meson setup build -Dbuildtype=debugoptimized
meson compile -C build
```

This is already it!

:::::
:::::{tab-item} Python

## Prerequisites for Python

The Python version of Constellation requires

- Python 3.11 or newer
- The Python [`venv`](https://docs.python.org/3/library/venv.html) module

It is also recommended to install the HDF5 development libraries to store data with the H5DataReceiverWriter satellite.

The prerequisites can be installed as follows:

::::{tab-set}
:::{tab-item} Debian/Ubuntu

```sh
sudo apt install python3-venv libhdf5-dev
```

:::
:::{tab-item} ALMA/Fedora

TODO

:::
:::{tab-item} MacOS

TODO

:::
:::{tab-item} Windows

TODO

:::
::::

## Setting up a Python Virtual Environment

```sh
python3 -m venv venv
source venv/bin/activate
pip install meson-python meson ninja
```

## Installing the Constellation Package

```sh
pip install --no-build-isolation --editable .
```

To install optional components of the framework, you can install those by replacing `.` with `.[component]`.
A recommended installation includes the `cli` and `hdf5` components:

```sh
pip install --no-build-isolation --editable .[cli,hdf5]
```

:::::
::::::
