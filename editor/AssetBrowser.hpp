#pragma once

#include <ASGE/Core/ECS/Registry.hpp>
#include <ASGE/Core/Filesystem/VirtualFileSystem.hpp>
#include <ASGE/Game/Assets/AssetManager.hpp>
#include <ASGE/Video/Graphics/Renderer.hpp>

#include <string>
#include <vector>

struct SDL_Window;

/** @brief Which kind of asset an AssetBrowser entry the user clicked names. */
enum class AssetPickKind { None, Texture, Animation, Audio, Font };

/**
 * @brief This frame's pick (if any) from DrawAssetExplorer -- which
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
 * @brief Phase 14: which action (if any) an asset row's right-click context
 *        menu requested this frame -- CreateEntity makes a new entity with
 *        the asset's corresponding component (Sprite/Animation/AudioSource);
 *        AttachTo does the same to an existing entity, picked from the
 *        menu's own submenu, instead of creating one; ReloadTexture (texture
 *        rows only) re-reads the file so entities show its updated pixels.
 */
enum class AssetContextAction { None, CreateEntity, AttachTo, ReloadTexture };

/**
 * @brief Where the bottom panel's explorer is looking: which mountpoint is
 *        selected in the mount list, and which folder below it is open.
 *        Shared by the mount list (which sets m_Root) and the explorer.
 */
struct AssetExplorerState
{
    std::string m_Root;   // selected virtual root; empty if nothing is mounted
    std::string m_Folder; // open folder below m_Root, no leading/trailing slash; empty is the root itself
};

/**
 * @brief DrawAssetExplorer's full per-frame report. m_Pick is the
 *        pre-Phase-14 "clicked an entry to preview it" signal, unchanged;
 *        m_OpenCreateClip and m_ContextAction/m_ContextKind/m_ContextPath/
 *        m_ContextTarget (Phase 14) come from a row's right-click menu
 *        instead, and are mutually exclusive with m_Pick and each other in
 *        practice (one user gesture per frame).
 */
struct AssetBrowserResult
{
    AssetPick          m_Pick;
    bool               m_OpenCreateClip = false;                          // a texture row's "Create Clip" -- see DrawAssetInspectorPanel's inOpenCreateClip
    AssetContextAction m_ContextAction  = AssetContextAction::None;
    AssetPickKind      m_ContextKind    = AssetPickKind::None;             // asset kind m_ContextAction applies to
    std::string        m_ContextPath;                                     // that asset's virtual path
    asge::ecs::Entity  m_ContextTarget  = asge::ecs::Entity::Null();       // AttachTo only: the entity picked from the submenu
};

/**
 * @brief The asset explorer: a file-explorer view of the selected mountpoint
 *        (ioState.m_Root), one folder at a time, drawn into the current ImGui
 *        window or child (BottomPanel hosts it).
 *
 * It lists every distinct, non-empty Sprite::m_VirtualPath/
 * Animation::m_ClipPath/AudioSource::m_VirtualClipPath/UILabel::m_FontPath
 * currently in inRegistry (see asset::CollectAssetRefs), unioned with
 * whatever's been explicitly imported via "Load Asset..." -- the latter exists
 * so an empty, freshly-opened scene (no entities, so nothing to derive usage
 * from) still has a way to bring an asset into the list before any entity
 * references it. Only the paths below ioState.m_Root's mountpoint show.
 *
 * Each cell is an icon with the item's name under it: a folder icon for a
 * folder (clicking it opens it, and the path bar's up button and crumbs lead
 * back to the mountpoint), a thumbnail for a texture, and a drawn icon for an
 * animation clip, audio clip or font. Clicking a file returns it in m_Pick --
 * the caller shows it in DrawAssetInspectorPanel, nothing is assigned.
 * Right-clicking a texture/animation/audio cell opens Create Entity/Attach To
 * plus Create Clip and Reload Asset (textures only) -- same "this file only
 * reports what was asked, the caller (main.cpp) performs the actual Registry
 * calls" separation DrawEntityListPanel's EntityListResult already uses. A
 * font has no such menu: a font path alone isn't a component gameplay code
 * would ever attach -- it only exists as part of a UILabel, which "Create UI
 * Element" (see editor/Inspector.hpp) authors. A cell that was imported by
 * hand also offers "Remove from list".
 *
 * "Load Asset..." first asks which mount to browse (native dialogs have no
 * notion of a virtual root, so the mount picks the real starting directory
 * and supplies the prefix for the resulting virtual path), then opens a
 * native open-file dialog scoped to that mount's real directory. The picked
 * file is classified the same way a directory scan would (image extension ->
 * texture, via media::Image::IsSupportedFile; ".toml" containing a
 * "[FrameTable]" table -> animation clip, via asset::FrameTable::IsFrameTable;
 * ".wav"/".ogg" -> audio clip, matching media::AudioClip::Load's own
 * extension dispatch; ".ttf" -> font, matching media::Font::Load's own
 * documented "loads a TTF file" contract).
 *
 * @param inAssets, inRenderer Used to load the textures the thumbnails show;
 *        AssetManager caches them by path.
 * @param inHasProject "Load Asset...", "Create Entity" and "Attach To" are
 *        disabled while false -- a created/attached-to entity gets no
 *        SceneId to tag it into anything without an active project/scene,
 *        same reasoning as the Entities panel's own "Create Entity" button.
 * @param ioState Which mountpoint and folder to show; the explorer moves
 *        m_Folder when a folder is entered or a crumb clicked.
 * @param inSelected The asset last picked, drawn highlighted.
 */
AssetBrowserResult DrawAssetExplorer(
    asge::ecs::Registry& inRegistry, asge::filesystem::VirtualFileSystem const& inVfs,
    asge::game::asset::AssetManager& inAssets, asge::video::IRenderer& inRenderer, SDL_Window* inWindow,
    bool inHasProject, AssetExplorerState& ioState, AssetPick const& inSelected ) noexcept;

/**
 * @brief The same texture virtual paths DrawAssetExplorer's "Textures"
 *        section would list (scene usage unioned with "Load Asset..."
 *        imports), for editor/Inspector.hpp's Sprite dropdown (both at
 *        Add-Component time and when editing an existing Sprite) to pick
 *        from -- a Sprite must be given one of these rather than typing an
 *        unresolved m_VirtualPath by hand.
 */
std::vector<std::string> KnownTexturePaths( asge::ecs::Registry& inRegistry ) noexcept;

/** @brief Same as KnownTexturePaths, but the "Animation Clips" section's paths. */
std::vector<std::string> KnownAnimationPaths( asge::ecs::Registry& inRegistry ) noexcept;

/** @brief Same as KnownTexturePaths, but the "Audio Clips" section's paths. */
std::vector<std::string> KnownAudioPaths( asge::ecs::Registry& inRegistry ) noexcept;

/** @brief Same as KnownTexturePaths, but the "Fonts" section's paths (Phase 16). */
std::vector<std::string> KnownFontPaths( asge::ecs::Registry& inRegistry ) noexcept;

/**
 * @brief Registers every distinct, non-empty Sprite::m_VirtualPath/
 *        Animation::m_ClipPath/AudioSource::m_VirtualClipPath currently in
 *        inRegistry as if each had been explicitly "Load Asset..."-ed.
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
 * @brief Adds inTexturePaths/inAnimationPaths/inAudioPaths/inFontPaths to
 *        the browser's persistent "known asset" set, as if each had been
 *        explicitly "Load Asset..."-ed.
 *
 * Lets Open Session restore assets that were imported but never assigned to
 * any entity -- RegisterSceneAssets alone can't recover those purely from
 * the reloaded scene's registry, since they were never referenced by it in
 * the first place. A path already present is just a no-op insert (std::set
 * semantics), not a duplicate entry.
 */
void ImportAssets(
    std::vector<std::string> const& inTexturePaths,
    std::vector<std::string> const& inAnimationPaths,
    std::vector<std::string> const& inAudioPaths,
    std::vector<std::string> const& inFontPaths = {} ) noexcept;

/**
 * @brief Empties the browser's persistent "known asset" set entirely.
 *
 * Call when switching projects -- a freshly created/opened project must
 * start with no leftover imports from whatever project was open before,
 * the same way its own [[Texture]]/[[Animation]]/[[Audio]] entries (or lack
 * of them) are about to replace what ImportAssets last populated.
 */
void ClearKnownAssets() noexcept;
