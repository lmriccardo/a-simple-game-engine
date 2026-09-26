#pragma once

#include "AssetBrowser.hpp" // AssetPick/AssetPickKind

#include <ASGE/Core/ECS/Registry.hpp>
#include <ASGE/Core/Filesystem/VirtualFileSystem.hpp>
#include <ASGE/Game/Assets/AssetManager.hpp>
#include <ASGE/Video/Graphics/Renderer.hpp>
#include <ASGE/Audio/AudioDevice.hpp>

/**
 * @brief Floating panel showing generic info -- absolute path, mountpoint,
 *        file size, and (for textures) pixel dimensions -- for whichever
 *        asset was last clicked in the Assets panel (editor/AssetBrowser),
 *        plus a thumbnail preview below a full-width separator for textures
 *        (no preview for animation clips) or a playable preview (path,
 *        duration, size, Play/Pause + Rewind) for audio clips.
 *
 * The preview texture is loaded through AssetManager::GetTexture, the same
 * path-cached call Sprite resolution uses -- later calls for an already-
 * inspected path just return the cached ITexture* rather than reloading it.
 * The audio preview similarly resolves through AssetManager::GetAudio and
 * plays back via a dedicated AudioStream from inAudioDevice, independent of
 * any entity's own AudioSource/AudioSystem playback.
 *
 * A texture with an associated animation clip (one of KnownAnimationPaths
 * whose own FrameTable::m_OriginAsset names this texture) shows "Open Clip"
 * in place of "Create Clip", switching straight to that clip's own preview
 * instead of the slicer modal; recomputed fresh every frame, so unloading
 * that clip from the Assets panel reverts the button back to "Create Clip"
 * with no extra bookkeeping needed here.
 *
 * @param ioSelectedAsset Cleared back to AssetPickKind::None if the user
 *        closes the panel via its title-bar (x) button -- same
 *        by-reference "caller owns the selection, this just edits it in
 *        place" convention as DrawEntityListPanel's ioSelected.
 * @param inRegistry Only used for KnownAnimationPaths, to find a texture's
 *        associated clip (see above).
 */
void DrawAssetInspectorPanel(
    AssetPick& ioSelectedAsset, asge::filesystem::VirtualFileSystem const& inVfs,
    asge::game::asset::AssetManager& inAssets, asge::video::IRenderer& inRenderer,
    asge::audio::AudioDevice& inAudioDevice, asge::ecs::Registry& inRegistry ) noexcept;
