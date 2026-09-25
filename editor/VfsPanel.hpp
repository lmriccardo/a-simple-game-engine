#pragma once

#include <ASGE/Core/ECS/Registry.hpp>
#include <ASGE/Core/Filesystem/VirtualFileSystem.hpp>
#include <ASGE/Video/Graphics/Renderer.hpp>
#include <ASGE/Game/Assets/AssetManager.hpp>

struct SDL_Window;

/**
 * @brief Always-on panel (not tied to a loaded scene): lists inVfs's current
 *        mounts, lets the user add new ones (a name plus a native folder
 *        picker), and surfaces any virtual root the currently-loaded
 *        scene's Sprite/Animation/AudioSource paths reference but isn't
 *        mounted yet.
 *
 * That last part exists because a scene's own asset paths are only
 * meaningful relative to whatever mounts the *original* project's own code
 * set up (e.g. `vfs.Mount("assets", "C:/myproject/assets")` in that
 * project's `main()`) -- something baked into source the editor has no way
 * to see, and not inferable from the scene file's own location on disk (the
 * whole point of a virtual path is that it's decoupled from physical
 * layout). So rather than guess, this panel asks.
 *
 * Calls inAssets.ResolveAssets(inRegistry, inRenderer) itself right after
 * any successful mount -- AssetManager/AssetPool never cache a resolve
 * failure (see AssetPool::GetOrLoad's own doc comment), so anything that
 * was failing purely for lack of this mount resolves on the very next call,
 * no further plumbing needed.
 *
 * @param inHasProject The "Add mount:" row is disabled while false -- a
 *        mount only means anything relative to a project's own scenes/
 *        assets, and there's nowhere for a fresh mount to actually persist
 *        to without one (see main.cpp's SaveProject).
 */
void DrawVfsPanel(
    asge::filesystem::VirtualFileSystem& inVfs, asge::ecs::Registry& inRegistry,
    asge::game::asset::AssetManager& inAssets, asge::video::IRenderer& inRenderer,
    SDL_Window* inWindow, bool inHasProject ) noexcept;
