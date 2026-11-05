#pragma once

#include "AssetBrowser.hpp"

#include <ASGE/Core/ECS/Registry.hpp>
#include <ASGE/Core/Filesystem/VirtualFileSystem.hpp>
#include <ASGE/Game/Assets/AssetManager.hpp>
#include <ASGE/Video/Graphics/Renderer.hpp>

struct SDL_Window;

/**
 * @brief The editor's bottom panel: the VFS mountpoints, the asset explorer and
 *        the Log side by side, anchored flush to the bottom and both side
 *        edges of the window.
 *
 * Its height is user-draggable and the whole panel can be minimized down
 * to its header row. The three columns are separated by draggable
 * splitters; the Log takes whatever width the other two leave.
 *
 * Returns the asset explorer's per-frame report unchanged, for the caller
 * (main.cpp) to act on.
 */
AssetBrowserResult DrawBottomPanel(
    asge::ecs::Registry& inRegistry, asge::filesystem::VirtualFileSystem& inVfs,
    asge::game::asset::AssetManager& inAssets, asge::video::IRenderer& inRenderer,
    SDL_Window* inWindow, bool inHasProject, AssetPick const& inSelectedAsset ) noexcept;
