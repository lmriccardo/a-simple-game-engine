#pragma once

#include "AssetBrowser.hpp" // AssetPick/AssetPickKind

#include <ASGE/Core/Filesystem/VirtualFileSystem.hpp>
#include <ASGE/Game/Assets/AssetManager.hpp>
#include <ASGE/Video/Graphics/Renderer.hpp>

/**
 * @brief Floating panel showing generic info -- absolute path, mountpoint,
 *        file size, and (for textures) pixel dimensions -- for whichever
 *        asset was last clicked in the Assets panel (editor/AssetBrowser),
 *        plus a thumbnail preview below a full-width separator for textures
 *        (no preview for animation clips). A no-op if nothing's been picked
 *        yet this session.
 *
 * The preview texture is loaded through the same AssetManager::GetImage +
 * CreateTexture path Sprite resolution uses, cached and only reloaded when
 * the inspected path changes -- CreateTexture allocates a fresh GPU texture
 * on every call, so calling it once per frame regardless would leak one
 * every frame.
 *
 * @param ioSelectedAsset Cleared back to AssetPickKind::None if the user
 *        closes the panel via its title-bar (x) button -- same
 *        by-reference "caller owns the selection, this just edits it in
 *        place" convention as DrawEntityListPanel's ioSelected.
 */
void DrawAssetInspectorPanel(
    AssetPick& ioSelectedAsset, asge::filesystem::VirtualFileSystem const& inVfs,
    asge::game::asset::AssetManager& inAssets, asge::video::IRenderer& inRenderer ) noexcept;
