#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include <ASGE/Core/Errors.hpp>

/** @brief One scene of the project a C++ project gets a state for: its display name and `.asgescene` path. */
struct CppScene
{
    std::string           m_Name;
    std::filesystem::path m_Path;
};

/** @brief What the generator needs to know about an ASGE project. */
struct CppProjectInput
{
    std::filesystem::path              m_ProjectFile; // <Folder>/<Name>.asgeproject
    std::vector<CppScene>              m_Scenes;      // in project order
    std::vector<std::filesystem::path> m_MountDirs;   // real directories of the VFS mounts
};

/** @brief The folder a project's C++ project lives in: `<project folder>/code`. */
[[nodiscard]] std::filesystem::path CppProjectDir( std::filesystem::path const& inProjectFile ) noexcept;

/** @brief True once `New C++ Project` has written `code/CMakeLists.txt` for inProjectFile. */
[[nodiscard]] bool IsCppProjectLinked( std::filesystem::path const& inProjectFile ) noexcept;

/** @brief inSceneName as a C++ identifier ("Main Menu" -> "MainMenu", "2nd level" -> "Scene2ndLevel"). */
[[nodiscard]] std::string CppStateName( std::string const& inSceneName );

/**
 * @brief Writes a new CMake project into `<project folder>/code`: CMakeLists.txt and main.cpp
 *        (written once, yours to edit), the generated Game/StateId/SceneState files, and one
 *        state class per scene, plus a `.vscode/settings.json` (only if missing) pointing CMake
 *        Tools at code/. inAsgeSourceDir is where ASGE's own CMake tree lives.
 *        Fails if the project has no scenes or `code/CMakeLists.txt` already exists.
 */
asge::BoolResult CreateCppProject(
    CppProjectInput const& inInput, std::filesystem::path const& inAsgeSourceDir ) noexcept;

/**
 * @brief Brings an existing C++ project in line with the ASGE project: rewrites the generated
 *        files (StateId.hpp, SceneState.hpp, Game.hpp/.cpp, project_files.cmake) and creates a
 *        state file for every scene that lacks one. Never overwrites or deletes a state file.
 */
asge::BoolResult UpdateCppProject( CppProjectInput const& inInput ) noexcept;

/**
 * @brief Opens inFolder in Visual Studio Code by running `code` from the PATH. Returns false
 *        (nothing happens) if `code` is not installed or the folder path cannot be quoted safely.
 */
bool OpenInVSCode( std::filesystem::path const& inFolder ) noexcept;
