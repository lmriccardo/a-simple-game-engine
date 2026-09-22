#pragma once

#include <filesystem>
#include <optional>

#include <ASGE/Core/ECS/Registry.hpp>
#include <ASGE/Core/Errors.hpp>
#include <ASGE/Core/Filesystem/VirtualFileSystem.hpp>
#include <ASGE/Game/Assets/AssetManager.hpp>
#include <ASGE/Game/Scene/SceneManager.hpp>
#include <ASGE/Video/Graphics/Renderer.hpp>

/**
 * @brief Loads the scene at inRealPath into inSceneManager without binding
 *        any lasting mount -- SceneManager::LoadScene only takes a virtual
 *        path (SceneSerializer resolves it through the vfs), unlike
 *        SaveScene, which takes a real path directly. A private root is
 *        mounted just long enough to resolve this one file and unmounted
 *        again immediately after, so callers never leave a stray mount
 *        sitting in the VFS panel as a side effect.
 */
asge::BoolResult LoadSceneFromRealPath(
    asge::filesystem::VirtualFileSystem& inVfs, asge::game::scene::SceneManager& inSceneManager,
    std::filesystem::path const& inRealPath ) noexcept;

/**
 * @brief Writes every current VirtualFileSystem mount, every texture/
 *        animation path the Assets panel currently knows about (scene usage
 *        unioned with "Load Asset..." imports -- see AssetBrowser.hpp's
 *        KnownTexturePaths/KnownAnimationPaths), and inCurrentScenePath
 *        (omitted if unset) to inPath as `.asges` TOML.
 */
asge::BoolResult SaveSession(
    asge::filesystem::VirtualFileSystem const& inVfs,
    asge::ecs::Registry& inRegistry,
    std::optional<std::filesystem::path> const& inCurrentScenePath,
    std::filesystem::path const& inPath ) noexcept;

/**
 * @brief Replaces inVfs's entire mount table with inPath's `[[Mount]]`
 *        entries (skipping any whose RealDirectory no longer exists, with a
 *        logged warning); restores every `[[Texture]]`/`[[Animation]]` path
 *        into the Assets panel's known-asset set (AssetBrowser::ImportAssets)
 *        so one imported but unused by any entity isn't lost just because it
 *        isn't in the reloaded scene's registry; unconditionally drops
 *        whatever scene was open; then, if `[Session].ScenePath` is present,
 *        loads it the same way Open Scene does. Replaces the whole working
 *        state outright, same no-dirty-check precedent as File > New / Open.
 */
asge::BoolResult LoadSession(
    asge::filesystem::VirtualFileSystem& inVfs, asge::game::scene::SceneManager& inSceneManager,
    asge::game::asset::AssetManager& inAssets, asge::video::IRenderer& inRenderer,
    std::filesystem::path const& inPath,
    std::optional<std::filesystem::path>& outCurrentScenePath ) noexcept;
