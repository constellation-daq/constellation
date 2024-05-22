/**
 * @file
 * @brief Compatibility functions for Windows
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

namespace constellation::utils {

#ifdef _WIN32

    inline std::wstring to_platform_string(std::string string) {
        // First get size for new wstring
        int size = MultiByteToWideChar(CP_UTF8, DWORD(0), string.data(), string.size(), nullptr, 0);
        std::wstring wstring {};
        wstring.resize(size);
        // Then convert from UTF-8
        MultiByteToWideChar(CP_UTF8, DWORD(0), string.data(), string.size(), wstring.data(), size);
        return wstring;
    }

    inline std::string to_std_string(std::wstring wstring) {
        // First get size for new string
        int size = WideCharToMultiByte(CP_UTF8, DWORD(0), wstring.data(), wstring.size(), nullptr, 0, NULL, NULL);
        std::string string {};
        string.resize(size);
        // Then convert to UTF-8
        WideCharToMultiByte(CP_UTF8, DWORD(0), wstring.data(), wstring.size(), string.data(), size, NULL, NULL);
        return string;
    }

#else

    inline std::string to_platform_string(std::string string) {
        return string;
    }

    inline std::string to_std_string(std::string string) {
        return string;
    }

#endif

} // namespace constellation::utils
