#include "SceneManager.hpp"

#include <algorithm>
#include <unordered_set>
#include <utility>

#include <ASGE/Game/Components.hpp>

void asge::game::scene::SceneManager::CopyEntityComponents(
    ecs::Registry const &inSrc, ecs::Entity inSrcEntity,
    ecs::Registry &inDst, ecs::Entity inDstEntity) const noexcept
{
    std::apply( [&]( auto ... component )
        {
            ( [&]
              {
                  using T = decltype(component);
                  if ( inSrc.HasComponent<T>(inSrcEntity) )
                  {
                      inDst.AddComponent<T>( inDstEntity, inSrc.GetComponent<T>(inSrcEntity).Value().get() );
                  }
              }(), ... );
        }, components::SerializableComponents{} );
}

asge::BoolResult asge::game::scene::SceneManager::LoadScene(str::String const &inVirtualPath) noexcept
{
    if ( m_CurrentScenePath && *m_CurrentScenePath == inVirtualPath )
        return BoolResult::Ok(); // already active

    // Already resident from an earlier load -- just switch which SceneId
    // counts as active. No Registry work at all.
    if ( !EntitiesInScene( inVirtualPath ).empty() )
    {
        m_CurrentScenePath = inVirtualPath;
        return BoolResult::Ok();
    }

    // Not resident -- load straight into the shared Registry.
    // SceneSerializer::Load only ever creates new entities and only ever
    // rolls back ones it created this call on failure, so this can't
    // disturb any other resident scene, active or not.
    auto const before = m_Registry.AllEntities();
    auto result = m_Serializer.Load( m_Registry, inVirtualPath );
    if ( !result ) return result;

    // Tag every entity Load just created (present now, absent before) with
    // this scene's identity, so EntitiesInScene()/eviction/unload can find
    // them again.
    for ( auto entity : m_Registry.AllEntities() )
    {
        bool const isNew = std::find( before.begin(), before.end(), entity ) == before.end();
        if ( !isNew ) continue;

        if ( auto tagResult = m_Registry.AddComponent<SceneId>( entity, SceneId{ inVirtualPath } ); !tagResult )
        {
            tagResult.LogError(); // not fatal to the load itself, but leaves this entity untaggable
        }
    }

    m_CurrentScenePath = inVirtualPath;
    return BoolResult::Ok();
}

void asge::game::scene::SceneManager::UnloadScene() noexcept
{
    if ( !m_CurrentScenePath ) return;

    for ( auto entity : EntitiesInScene( *m_CurrentScenePath ) )
    {
        if ( auto result = m_Registry.DestroyEntity( entity ); !result ) result.LogError();
    }

    m_CurrentScenePath.reset();
}

asge::BoolResult asge::game::scene::SceneManager::SaveScene(filesystem::Path const &inPath) const noexcept
{
    // SceneSerializer::Save has no notion of "just these entities" -- it
    // serializes a whole Registry -- so build a scratch one holding a copy
    // of just the active scene's entities and hand that to it instead.
    ecs::Registry snapshot;
    for ( auto entity : ActiveEntities() )
    {
        auto newEntity = snapshot.CreateEntity();
        if ( !newEntity ) return BoolResult::Err( newEntity.Error() );
        CopyEntityComponents( m_Registry, entity, snapshot, newEntity.Value() );
    }

    return m_Serializer.Save( snapshot, inPath );
}

void asge::game::scene::SceneManager::EvictCachedScene(str::String const &inVirtualPath) noexcept
{
    if ( m_CurrentScenePath && *m_CurrentScenePath == inVirtualPath ) return; // active scene isn't "cached"

    for ( auto entity : EntitiesInScene( inVirtualPath ) )
    {
        if ( auto result = m_Registry.DestroyEntity( entity ); !result ) result.LogError();
    }
}

void asge::game::scene::SceneManager::ClearCache() noexcept
{
    for ( auto entity : m_Registry.AllEntities() )
    {
        auto sceneId = m_Registry.GetComponent<SceneId>( entity );
        if ( !sceneId ) continue; // not scene-tagged -- not this call's concern
        if ( m_CurrentScenePath && sceneId.Value().get().m_Path == *m_CurrentScenePath ) continue; // keep active

        if ( auto result = m_Registry.DestroyEntity( entity ); !result ) result.LogError();
    }
}

std::size_t asge::game::scene::SceneManager::CachedSceneCount() const noexcept
{
    std::unordered_set<str::String> distinctPaths;
    for ( auto entity : m_Registry.AllEntities() )
    {
        auto sceneId = m_Registry.GetComponent<SceneId>( entity );
        if ( !sceneId ) continue;
        if ( m_CurrentScenePath && sceneId.Value().get().m_Path == *m_CurrentScenePath ) continue;
        distinctPaths.insert( sceneId.Value().get().m_Path );
    }
    return distinctPaths.size();
}

void asge::game::scene::SceneManager::RequestLoad(str::String const &inVirtualPath) noexcept
{
    m_PendingKind = PendingKind::Load;
    m_PendingPath = inVirtualPath;
}

void asge::game::scene::SceneManager::RequestUnload() noexcept
{
    m_PendingKind = PendingKind::Unload;
    m_PendingPath.clear();
}

bool asge::game::scene::SceneManager::HasPendingTransition() const noexcept
{
    return m_PendingKind != PendingKind::None;
}

asge::BoolResult asge::game::scene::SceneManager::ApplyPendingTransition() noexcept
{
    switch ( m_PendingKind )
    {
    case PendingKind::None:
        return BoolResult::Ok();

    case PendingKind::Unload:
        m_PendingKind = PendingKind::None;
        UnloadScene();
        return BoolResult::Ok();

    case PendingKind::Load:
    {
        auto path = std::move( m_PendingPath );
        m_PendingKind = PendingKind::None;
        m_PendingPath.clear();
        return LoadScene( path );
    }
    }

    return BoolResult::Ok(); // unreachable -- silences a spurious "not all paths return" warning
}

std::vector<asge::ecs::Entity> asge::game::scene::SceneManager::EntitiesInScene(str::String const &inVirtualPath) const noexcept
{
    std::vector<ecs::Entity> result;
    for ( auto entity : m_Registry.AllEntities() )
    {
        auto sceneId = m_Registry.GetComponent<SceneId>( entity );
        if ( sceneId && sceneId.Value().get().m_Path == inVirtualPath ) result.push_back( entity );
    }
    return result;
}

std::vector<asge::ecs::Entity> asge::game::scene::SceneManager::ActiveEntities() const noexcept
{
    return m_CurrentScenePath ? EntitiesInScene( *m_CurrentScenePath ) : std::vector<ecs::Entity>{};
}
