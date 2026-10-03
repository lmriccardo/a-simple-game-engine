#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#include <ASGE/Core/ECS/Entity.hpp>
#include <ASGE/Core/Errors.hpp>
#include <ASGE/Game/Scene/IdContext.hpp>

/**
 * @brief A named folder of top-level entities in the Entities panel. Purely a
 *        view over the scene: it changes nothing in the engine or in the scene file.
 */
struct EntityGroup
{
    std::string                     m_Name;
    std::vector<asge::ecs::Entity>  m_Members;
    bool                            m_Open = true; // folder expanded in the panel
};

using EntityGroupList = std::vector<EntityGroup>;

/** @brief The groups of the scene identified by inScenePath (SceneManager::CurrentScenePath()), empty until first used. */
EntityGroupList& GroupsFor( std::string const& inScenePath ) noexcept;

/** @brief Forgets the groups of every scene -- used when the whole editor state is cleared. */
void ClearAllGroups() noexcept;

/** @brief Moves inOldScenePath's groups under inNewScenePath, for a scene saved under a new name. */
void RenameGroupScene( std::string const& inOldScenePath, std::string const& inNewScenePath ) noexcept;

/** @brief Replaces each member found in inRestored with its new handle and drops those not in it (see SceneManager::TakeRestoredEntities). */
void RemapGroupMembers( EntityGroupList& ioGroups, std::unordered_map<asge::ecs::Entity, asge::ecs::Entity> const& inRestored ) noexcept;

/** @brief Where a scene's groups are stored: its own path plus ".groups", next to it. */
std::filesystem::path GroupsFilePath( std::filesystem::path const& inScenePath ) noexcept;

/**
 * @brief Writes inGroups next to inScenePath, each member as its index in the scene file
 *        (from inCtx, as SceneManager::SaveScene filled it). Removes the file if there is
 *        nothing to store, so a scene without groups leaves no file behind.
 */
asge::BoolResult SaveGroupsFile(
    std::filesystem::path const& inScenePath, EntityGroupList const& inGroups,
    asge::game::scene::SaveContext const& inCtx ) noexcept;

/** @brief Reads inScenePath's groups back, resolving file indices to entities with inCtx; empty if there is no file. */
EntityGroupList LoadGroupsFile(
    std::filesystem::path const& inScenePath, asge::game::scene::LoadContext const& inCtx ) noexcept;
