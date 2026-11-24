#pragma once

#include <ASGE/Core/ECS/Registry.hpp>
#include <ASGE/Core/Filesystem/VirtualFileSystem.hpp>
#include <ASGE/Game/Assets/AssetManager.hpp>
#include <ASGE/Video/Graphics/Renderer.hpp>

#include <string>

/**
 * @brief Loads the prefab at inVirtualPath into a private registry and opens the Edit Prefab modal.
 *
 * The prefab is edited apart from the open scene, so nothing in the scene changes until it is
 * saved. Logs and does nothing if the prefab cannot be loaded.
 */
void OpenPrefabEditor(
    asge::filesystem::VirtualFileSystem const& inVfs, asge::game::asset::AssetManager& inAssets,
    asge::video::IRenderer& inRenderer, std::string const& inVirtualPath ) noexcept;

/**
 * @brief Draws the Edit Prefab modal, if open: the prefab's entity tree, an inspector for the
 *        selected entity, and Save and Cancel buttons.
 *
 * Save writes the prefab file back, reloads the asset through inAssets (inSceneRegistry's
 * components that use it are un-resolved) and closes the modal. Instances already in a scene
 * are copies and keep their old data. Call once per frame.
 */
void DrawPrefabEditor(
    asge::filesystem::VirtualFileSystem const& inVfs, asge::game::asset::AssetManager& inAssets,
    asge::video::IRenderer& inRenderer, asge::ecs::Registry& inSceneRegistry ) noexcept;
