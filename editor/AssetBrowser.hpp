#pragma once

#include <ASGE/Core/ECS/Registry.hpp>
#include <ASGE/Core/Filesystem/VirtualFileSystem.hpp>

#include <string>
#include <vector>

struct SDL_Window;

/** @brief Which kind of asset an AssetBrowser entry the user clicked names. */
enum class AssetPickKind { None, Texture, Animation };

/**
 * @brief This frame's pick (if any) from DrawAssetBrowserPanel -- which
 *        entry the user clicked, for the caller (main.cpp) to inspect via
 *        DrawAssetInspectorPanel. Clicking an entry here no longer assigns
 *        it to whatever entity is selected; see editor/Inspector.hpp's
 *        DrawInspectorPanel for how a Sprite actually gets a texture now.
 */
struct AssetPick
{
    AssetPickKind m_Kind = AssetPickKind::None;
    std::string   m_VirtualPath;
};

/**
 * @brief Panel listing every texture/animation-clip virtual path known to
 *        the editor: every distinct, non-empty Sprite::m_VirtualPath/
 *        Animation::m_ClipPath currently in inRegistry, unioned with
 *        whatever's been explicitly imported via "Load Asset..." -- the
 *        latter exists so an empty, freshly-opened scene (no entities, so
 *        nothing to derive usage from) still has a way to bring an asset
 *        into the list before any entity references it.
 *
 * "Load Asset..." first asks which mount to browse (native dialogs have no
 * notion of a virtual root, so the mount picks the real starting directory
 * and supplies the prefix for the resulting virtual path), then opens a
 * native open-file dialog scoped to that mount's real directory. The picked
 * file is classified the same way a directory scan would (image extension
 * -> texture; ".toml" containing a "[FrameTable]" table -> animation clip).
 *
 * Clicking an entry (of either kind) just returns it here -- the caller
 * shows it in DrawAssetInspectorPanel, it doesn't assign anything.
 */
AssetPick DrawAssetBrowserPanel(
    asge::ecs::Registry& inRegistry, asge::filesystem::VirtualFileSystem const& inVfs, SDL_Window* inWindow ) noexcept;

/**
 * @brief The same texture virtual paths DrawAssetBrowserPanel's "Textures"
 *        section would list (scene usage unioned with "Load Asset..."
 *        imports), for editor/Inspector.hpp's Sprite-add flow to pick from
 *        -- a Sprite must be given one of these up front rather than
 *        starting with a blank, unresolved m_VirtualPath.
 */
std::vector<std::string> KnownTexturePaths( asge::ecs::Registry& inRegistry ) noexcept;

/** @brief Same as KnownTexturePaths, but the "Animation Clips" section's paths. */
std::vector<std::string> KnownAnimationPaths( asge::ecs::Registry& inRegistry ) noexcept;

/**
 * @brief Registers every distinct, non-empty Sprite::m_VirtualPath/
 *        Animation::m_ClipPath currently in inRegistry as if each had been
 *        explicitly "Load Asset..."-ed.
 *
 * Call once, right after a scene finishes loading -- without this, an
 * asset that only ever appeared here because some entity referenced it
 * (never actually imported) vanishes from the panel the moment that
 * entity's component is removed or the entity is deleted, even though
 * nothing about the asset itself changed. An asset's presence in the panel
 * shouldn't depend on whether anything currently happens to be using it.
 */
void RegisterSceneAssets( asge::ecs::Registry& inRegistry ) noexcept;

/**
 * @brief Adds inTexturePaths/inAnimationPaths to the browser's persistent
 *        "known asset" set, as if each had been explicitly "Load Asset..."-ed.
 *
 * Lets Open Session restore assets that were imported but never assigned to
 * any entity -- RegisterSceneAssets alone can't recover those purely from
 * the reloaded scene's registry, since they were never referenced by it in
 * the first place. A path already present is just a no-op insert (std::set
 * semantics), not a duplicate entry.
 */
void ImportAssets(
    std::vector<std::string> const& inTexturePaths, std::vector<std::string> const& inAnimationPaths ) noexcept;
