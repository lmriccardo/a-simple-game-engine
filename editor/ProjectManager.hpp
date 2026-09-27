#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <ASGE/Core/ECS/Registry.hpp>
#include <ASGE/Core/Errors.hpp>
#include <ASGE/Core/Filesystem/VirtualFileSystem.hpp>

/**
 * @brief One scene belonging to a Project -- its display name (also the
 *        file stem), its real on-disk `.asgescene` path, and whether it's
 *        been edited since its last load/save.
 *
 * m_Dirty only ever meaningfully applies to whichever scene is currently
 * active in the editor's SceneManager -- a scene the UI isn't showing can't
 * be mutated through the Inspector/viewport, so it can never actually go
 * dirty while inactive.
 */
struct ProjectScene
{
    std::string           m_Name;
    std::filesystem::path m_Path;
    bool                  m_Dirty = false;
};

/**
 * @brief What used to be an `.asges` "session" -- mounts, known assets,
 *        view settings, and (Phase 11) the list of scenes that belong to
 *        it. m_ActiveSceneIndex is editor-only state, never round-tripped
 *        through the `.asgeproject` file itself (see asge.session instead).
 */
struct Project
{
    std::filesystem::path     m_FilePath; // <Folder>/<Name>.asgeproject
    std::vector<ProjectScene> m_Scenes;
    int                       m_ActiveSceneIndex = -1;
};

/**
 * @brief Writes inProject's own fields -- every current VirtualFileSystem
 *        mount, every texture/animation/audio path the Assets panel
 *        currently knows about, inGridSpacing/inTargetWidth/inTargetHeight,
 *        and inProject.m_Scenes' paths -- to inProject.m_FilePath as
 *        `.asgeproject` TOML.
 *
 * Deliberately does NOT save any scene file itself -- an ordinary "Save"/
 * "Save As" on a project only ever touches project-level fields, even if a
 * scene is currently dirty; see main.cpp's own scene-save call sites for
 * where an active scene actually gets written.
 */
asge::BoolResult SaveProject(
    asge::filesystem::VirtualFileSystem const& inVfs,
    asge::ecs::Registry& inRegistry,
    Project const& inProject,
    float inGridSpacing, int inTargetWidth, int inTargetHeight ) noexcept;

/**
 * @brief Parses inPath as a `.asgeproject`: replaces inVfs's entire mount
 *        table (skipping any whose RealDirectory no longer exists, with a
 *        logged warning), restores every known texture/animation/audio
 *        path into the Assets panel (AssetBrowser::ImportAssets), and fills
 *        outProject (m_FilePath = inPath, m_Scenes from the file's Scenes
 *        list -- each entry's m_Name derived from its path's stem,
 *        m_ActiveSceneIndex left at -1). Does not load any scene itself --
 *        the caller decides which of outProject.m_Scenes (if any) to
 *        switch to.
 */
asge::BoolResult LoadProject(
    asge::filesystem::VirtualFileSystem& inVfs,
    std::filesystem::path const& inPath,
    Project& outProject,
    float& outGridSpacing, int& outTargetWidth, int& outTargetHeight ) noexcept;

/**
 * @brief Writes `asge.session` (ambient "what does the editor currently
 *        look like" state, distinct from a Project) to inPath: which
 *        project (if any) is open and which of its scenes is active, each
 *        omitted from the file entirely if unset.
 */
asge::BoolResult SaveEditorSession(
    std::optional<std::filesystem::path> const& inProjectPath,
    std::optional<std::filesystem::path> const& inActiveScenePath,
    std::filesystem::path const& inPath ) noexcept;

/**
 * @brief Reads `asge.session` from inPath. outProjectPath/outActiveScenePath
 *        are left untouched (not cleared) if the file doesn't set them, so
 *        a caller checking IsOk() first and their own already-nullopt
 *        defaults doesn't need special-casing here.
 */
asge::BoolResult LoadEditorSession(
    std::filesystem::path const& inPath,
    std::optional<std::filesystem::path>& outProjectPath,
    std::optional<std::filesystem::path>& outActiveScenePath ) noexcept;
