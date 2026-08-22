#pragma once

#include <string>
#include <typeinfo>

// Platform/Compiler specific includes for demangling
#if defined(__GNUG__) || defined(__clang__)
    #include <cxxabi.h>
    #include <memory>
    #include <cstdlib>
#elif defined(_WIN32)
    #ifndef NOMINMAX
        #define NOMINMAX // Prevent windows.h from defining min()/max() macros
    #endif
    #include <windows.h>
    #include <dbghelp.h>
    #pragma comment(lib, "dbghelp.lib")
#endif

namespace asge::rtti
{

/**
 * @brief Returns the human-readable, demangled name of type @c T.
 *
 * Wraps compiler-specific RTTI demangling (@c abi::__cxa_demangle on
 * GCC/Clang, @c UnDecorateSymbolName on MSVC) behind a single portable call.
 * Falls back to the raw @c typeid(T).name() on unsupported compilers or if
 * demangling fails.
 *
 * @tparam T Type to name.
 * @return The demangled type name.
 */
template <typename T>
std::string GetDemangledName() noexcept
{
    const char* raw_name = typeid(T).name();

#if defined(__GNUG__) || defined(__clang__)
    // GCC / Clang demangling via abi::__cxa_demangle
    int status = -1;
    std::unique_ptr<char, void(*)(void*)> demangled{
        abi::__cxa_demangle(raw_name, nullptr, nullptr, &status),
        std::free
    };
    return (status == 0) ? std::string(demangled.get()) : raw_name;

#elif defined(_MSC_VER)
    // MSVC typeid(T).name() is already human-readable for types.
    // If raw_name starts with '?', it's a mangled symbol needing UnDecorateSymbolName.
    if (raw_name[0] == '?') {
        char buffer[256];
        if (UnDecorateSymbolName(raw_name, buffer, sizeof(buffer), UNDNAME_COMPLETE)) {
            return std::string(buffer);
        }
    }
    return std::string(raw_name);

#else
    // Fallback for other compilers
    return std::string(raw_name);
#endif
}

}
