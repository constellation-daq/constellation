# Building External Satellites

External satellites are satellites that have their implementation not in the Constellation repository, but in some downstream
code base.

## Installing Constellation

External satellites usually require an installation of Constellation. By default, the `meson install` command installs to
`/usr/local`. This can be changed via:

```sh
meson configure build -Dprefix=CNSTLN_PREFIX # set installation directory here, e.g. `$(pwd)/usr`
meson install -C build
```

## Setting the pkg-config Search Path

Constellation exports its dependency using pkg-config, which can be easily used in many build systems, including CMake.
In order to find Constellation via pkg-config in a non-standard location when building external satellites, the prefix path
needs to be exported:

```sh
export CNSTLN_PREFIX=$(pwd)/usr
export PKG_CONFIG_PATH="$CNSTLN_PREFIX/lib64/pkgconfig:$CNSTLN_PREFIX/usr/share/pkgconfig"
```

Note that the platform specific part (`lib64`) might be different depending on your platform, e.g. it is
`lib/x86_64-linux-gnu` for Debian/Ubuntu. It can be sound be checking the content of the prefix path.

## Creating the Satellite Generator Interface

In Constellation, all C++ satellites are shared libraries, which allows to share the executable interface between all
satellites. To find the satellite symbol within the shared library, a generator function has to be included. In the
Constellation repository, this is done automatically. For external satellites, a file containing this code needs to be added
to the satellite library:

```c++
// SPDX-License-Identifier: EUPL-1.2 OR CC0-1.0

#include <memory>
#include <string_view>

#include <constellation/build.hpp>
#include <constellation/satellite/Satellite.hpp>

#include "ExampleSatellite.hpp" // <-- Replace with satellite header here

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wreturn-type-c-linkage"
#endif

// generator function for loading satellite from shared library
extern "C" {
CNSTLN_DLL_EXPORT
std::shared_ptr<constellation::satellite::Satellite> generator(std::string_view type, std::string_view name) {
    return std::make_shared<ExampleSatellite>(type, name); // <-- Replace with satellite class here
}
}

#ifdef __clang__
#pragma clang diagnostic pop
#endif
```

## CMake Integration

External satellites can be build with CMake using this template:

```cmake
# Find Constellation via pkg-config
find_package(PkgConfig REQUIRED)
pkg_check_modules(CNSTLN_SAT REQUIRED ConstellationSatellite)
pkg_check_modules(CNSTLN_EXEC REQUIRED ConstellationExec)

# Create shared library for satellite
add_library(ExampleSatellite SHARED "ExampleSatellite.cpp" "generator.cpp")

# Add project internal dependencies
target_link_libraries(ExampleSatellite PUBLIC example-lib)

# Ensure compilation with (at least) C++20
set_target_properties(ExampleSatellite PROPERTIES CXX_STANDARD 20)

# Add Constellation dependencies
target_include_directories(ExampleSatellite PUBLIC ${CNSTLN_SAT_INCLUDE_DIRS})
target_link_directories(ExampleSatellite PUBLIC ${CNSTLN_SAT_LIBRARY_DIRS})
target_link_libraries(ExampleSatellite PUBLIC ${CNSTLN_SAT_LIBRARIES})
target_compile_options(ExampleSatellite PUBLIC ${CNSTLN_SAT_CFLAGS_OTHER})

# Set library output name to satellite type (used in `Satellite -t`)
set_target_properties(ExampleSatellite PROPERTIES LIBRARY_OUTPUT_NAME "Example")

# Install satellite to Constellation prefix
set(CNSTLN_SAT_INSTALL_DIR "${CNSTLN_EXEC_LIBDIR}/ConstellationSatellites")
message(STATUS "Installing satellite to: \"${CNSTLN_SAT_INSTALL_DIR}\"")
install(TARGETS
    ExampleSatellite
    LIBRARY DESTINATION ${CNSTLN_SAT_INSTALL_DIR}
)
```

````{note}
This does not generate an executable for your satellite, instead it needs to be launch with the `Satellite` executable
installed with Constellation:

```sh
Satellite -t Example
```
````
