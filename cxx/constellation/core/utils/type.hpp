/**
 * @file
 * @brief Tags for type dispatching and run time type identification
 *
 * @copyright Copyright (c) 2024 DESY and the Constellation authors.
 * This software is distributed under the terms of the EUPL-1.2 License, copied verbatim in the file "LICENSE.md".
 * SPDX-License-Identifier: EUPL-1.2
 */

#pragma once

#include <cstdlib>
#include <cxxabi.h>
#include <memory>
#include <string>

namespace constellation::utils {
    /**
     * @brief Demangle the type to human-readable form if it is mangled
     * @param name The possibly mangled name
     * @param keep_prefix If true the constellation namespace prefix will be kept, otherwise it is removed
     */
    inline std::string demangle(const char* name, bool keep_prefix = false) {
        // Only demangled for GNU compiler
#ifdef __GNUG__
        // Try to demangle
        int status = -1;
        const std::unique_ptr<char, void (*)(void*)> res {abi::__cxa_demangle(name, nullptr, nullptr, &status), std::free};

        if(status == 0) {
            // Remove constellation prefix if necessary
            std::string str = res.get();
            if(!keep_prefix && str.find("constellation::") == 0) {
                return str.substr(15);
            }
            return str;
        }
#endif
        return name;
    }

    /**
     * @brief Demangle the type to human-readable form if it is mangled
     * @param type Type info of the mangled name
     * @param keep_prefix If true the constellation namespace prefix will be kept, otherwise it is removed
     */
    inline std::string demangle(const std::type_info& type, bool keep_prefix = false) {
        return demangle(type.name(), keep_prefix);
    }

} // namespace constellation::utils
