#pragma once

#include <optional>
#include <string>
#include <vector>

/** @brief One way to build the generated C++ project: a compiler/generator combination CMake can use. */
struct Toolchain
{
    std::string              m_Id;            // stable key saved in the preference file
    std::string              m_Name;          // shown in the menu
    std::vector<std::string> m_ConfigureArgs; // extra `cmake -S -B` arguments that select it
    bool                     m_Usable = true;
    std::string              m_Reason;        // why it cannot be used, when it cannot
};

/** @brief One Visual Studio installation reported by vswhere. */
struct VsInstall
{
    std::string m_DisplayName; // e.g. "Visual Studio Community 2026"
    std::string m_Version;     // e.g. "18.5.11612.150"
};

/** @brief Reads vswhere's default text output: one block of `key: value` lines per installation. */
[[nodiscard]] std::vector<VsInstall> ParseVsWhere( std::string const& inText );

/** @brief The CMake generator name for a Visual Studio version ("17.9.1" -> "Visual Studio 17 2022"), if known. */
[[nodiscard]] std::optional<std::string> VsGenerator( std::string const& inVersion );

/** @brief The major version in a `<compiler> --version` first line ("clang version 17.0.6" -> 17). */
[[nodiscard]] std::optional<int> CompilerMajorVersion( std::string const& inVersionLine );

/**
 * @brief Looks for build toolchains on this machine: the Visual Studio installations vswhere
 *        finds (x64) on Windows, g++ and clang++ on the PATH elsewhere. Runs external tools, so
 *        call it off the UI thread. An entry that cannot build ASGE has m_Usable false and a reason.
 */
[[nodiscard]] std::vector<Toolchain> ScanToolchains();
