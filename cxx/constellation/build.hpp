/**
 * @file
 * @brief Compile-time configuration
 *
 * @copyright Copyright (c) 2023 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#if defined _WIN32 || defined __CYGWIN__
#define CNSTLN_DLL_EXPORT __declspec(dllexport)
#define CNSTLN_DLL_IMPORT __declspec(dllimport)
#define CNSTLN_DLL_LOCAL
#else
#define CNSTLN_DLL_EXPORT __attribute__((__visibility__("default")))
#define CNSTLN_DLL_IMPORT __attribute__((__visibility__("default")))
#define CNSTLN_DLL_LOCAL __attribute__((__visibility__("hidden")))
#endif

#ifndef CNSTLN_STATIC
#ifdef CNSTLN_BUILDLIB
#define CNSTLN_API CNSTLN_DLL_EXPORT
#define CNSTLN_LOCAL CNSTLN_DLL_LOCAL
#else
#define CNSTLN_API CNSTLN_DLL_IMPORT
#define CNSTLN_LOCAL CNSTLN_DLL_LOCAL
#endif
#else
#define CNSTLN_API
#define CNSTLN_LOCAL
#endif

// NOLINTBEGIN(cppcoreguidelines-macro-usage)

#define CNSTLN_VERSION "@version@"

#define CNSTLN_LIBDIR "@libdir@"

#define CNSTLN_BUILDDIR "@builddir@"

#define CNSTLN_DSO_SUFFIX "@dso_suffix@"

// NOLINTEND(cppcoreguidelines-macro-usage)
