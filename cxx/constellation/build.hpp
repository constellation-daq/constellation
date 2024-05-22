/**
 * @file
 * @brief Compile-time configuration
 *
 * @copyright Copyright (c) 2023 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#if(defined _WIN32 && !defined __CYGWIN__)
#define CNSTLN_ABI __declspec(dllexport)
#if CNSTLN_BUILDLIB
#define CNSTLN_API CNSTLN_ABI
#else
#define CNSTLN_API __declspec(dllimport)
#endif
#else
#define CNSTLN_ABI __attribute__((__visibility__("default")))
#if CNSTLN_BUILDLIB
#define CNSTLN_API CNSTLN_ABI
#else
#define CNSTLN_API
#endif
#endif

// NOLINTBEGIN(cppcoreguidelines-macro-usage)

#define CNSTLN_VERSION "@version@"

#define CNSTLN_LIBDIR "@libdir@"

#define CNSTLN_BUILDDIR "@builddir@"

#define CNSTLN_DSO_SUFFIX "@dso_suffix@"

// NOLINTEND(cppcoreguidelines-macro-usage)
