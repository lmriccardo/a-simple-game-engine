#pragma once

#include "AssetBrowser.hpp" // AssetExplorerState

#include <ASGE/Core/ECS/Registry.hpp>
#include <ASGE/Core/Filesystem/VirtualFileSystem.hpp>
#include <ASGE/Video/Graphics/Renderer.hpp>
#include <ASGE/Game/Assets/AssetManager.hpp>

struct SDL_Window;

/**
 * @brief Draws, into the current ImGui window or child (BottomPanel hosts it),
 *        the list of inVfs's mountpoints: one equally tall, selectable row per
 *        virtual root.
 *
 * Clicking a row makes it ioState's selected root (and returns to its top
 * folder), which the asset explorer then shows. Right-clicking a row offers
 * to unmount each real directory behind it. A virtual root the loaded scene's
 * Sprite/Animation/AudioSource paths reference but nothing mounts is listed
 * after them in yellow, with a "Set..." button that picks its folder -- a
 * scene's asset paths are only meaningful relative to the mounts the original
 * project's own code set up, which the editor can't see, so it asks.
 *
 * Calls inAssets.ResolveAssets(inRegistry, inRenderer) itself right after
 * any successful mount -- AssetManager/AssetPool never cache a resolve
 * failure, so anything that was failing purely for lack of this mount
 * resolves on the very next call.
 *
 * @param inHasProject "+ Add mount" is disabled while false -- a mount only
 *        means anything relative to a project, and there's nowhere for it to
 *        persist to without one (see main.cpp's SaveProject).
 * @param ioState Its m_Root is kept valid: if it names a root that is no
 *        longer mounted it falls back to the first mounted one.
 */
void DrawMountList(
    asge::filesystem::VirtualFileSystem& inVfs, asge::ecs::Registry& inRegistry,
    asge::game::asset::AssetManager& inAssets, asge::video::IRenderer& inRenderer,
    SDL_Window* inWindow, bool inHasProject, AssetExplorerState& ioState ) noexcept;
