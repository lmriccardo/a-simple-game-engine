#include "SceneManager.hpp"

#include <algorithm>
#include <string>
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

void asge::game::scene::SceneManager::SuspendScene(str::String const &inSceneId) noexcept
{
    ecs::Registry snapshot;
    for ( auto entity : EntitiesInScene( inSceneId ) )
    {
        auto newEntity = snapshot.CreateEntity();
        if ( !newEntity ) { newEntity.LogError(); continue; }

        CopyEntityComponents( m_Registry, entity, snapshot, newEntity.Value() );
        if ( auto result = m_Registry.DestroyEntity( entity ); !result ) result.LogError();
    }

    m_Snapshots.insert_or_assign( inSceneId, std::move( snapshot ) );
}

void asge::game::scene::SceneManager::RestoreScene(
    str::String const &inSceneId, ecs::Registry const &inSnapshot) noexcept
{
    for ( auto entity : inSnapshot.AllEntities() )
    {
        auto newEntity = m_Registry.CreateEntity();
        if ( !newEntity ) { newEntity.LogError(); continue; }

        CopyEntityComponents( inSnapshot, entity, m_Registry, newEntity.Value() );
        if ( auto tagResult = m_Registry.AddComponent<SceneId>( newEntity.Value(), SceneId{ inSceneId } ); !tagResult )
        {
            tagResult.LogError();
        }
    }
}

asge::BoolResult asge::game::scene::SceneManager::LoadScene(str::String const &inVirtualPath) noexcept
{
    return LoadSceneCommon( inVirtualPath, [&]( ecs::Registry& inRegistry )
    {
        return m_Serializer.Load( inRegistry, inVirtualPath );
    } );
}

asge::BoolResult asge::game::scene::SceneManager::LoadSceneFromFile(filesystem::Path const &inPath) noexcept
{
    return LoadSceneCommon( inPath.string(), [&]( ecs::Registry& inRegistry )
    {
        return m_Serializer.LoadFromFile( inRegistry, inPath );
    } );
}

asge::BoolResult asge::game::scene::SceneManager::LoadSceneCommon(
    str::String const &inSceneId, std::function<BoolResult( ecs::Registry& )> const &inLoad) noexcept
{
    if ( m_CurrentScenePath && *m_CurrentScenePath == inSceneId )
        return BoolResult::Ok(); // already active

    // Snapshotted from an earlier visit -- restore its live-mutated state
    // in-memory instead of rereading (and losing that state) from disk.
    if ( auto snapshotIt = m_Snapshots.find( inSceneId ); snapshotIt != m_Snapshots.end() )
    {
        auto snapshot = std::move( snapshotIt->second );
        m_Snapshots.erase( snapshotIt );

        if ( m_CurrentScenePath ) SuspendScene( *m_CurrentScenePath );

        RestoreScene( inSceneId, snapshot );
        m_CurrentScenePath = inSceneId;
        return BoolResult::Ok();
    }

    // Not resident anywhere -- load straight into the shared Registry,
    // alongside whatever's still live from the outgoing scene (if any).
    // SceneSerializer::Load/LoadFromFile only ever create new entities and
    // only ever roll back ones they created this call on failure, so a
    // failed load can't disturb the outgoing scene's still-live entities.
    auto const before = m_Registry.AllEntities();
    auto result = inLoad( m_Registry );
    if ( !result ) return result;

    // Tag every entity the load just created (present now, absent before)
    // with this scene's identity, so EntitiesInScene()/eviction/unload can
    // find them again.
    for ( auto entity : m_Registry.AllEntities() )
    {
        bool const isNew = std::find( before.begin(), before.end(), entity ) == before.end();
        if ( !isNew ) continue;

        if ( auto tagResult = m_Registry.AddComponent<SceneId>( entity, SceneId{ inSceneId } ); !tagResult )
        {
            tagResult.LogError(); // not fatal to the load itself, but leaves this entity untaggable
        }
    }

    // Only now, with the new scene confirmed loaded, suspend the outgoing
    // one -- keeps only one scene's entities live in the Registry at a time
    // without disturbing anything on a failed load.
    if ( m_CurrentScenePath ) SuspendScene( *m_CurrentScenePath );

    m_CurrentScenePath = inSceneId;
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
    m_Snapshots.erase( inVirtualPath );
}

void asge::game::scene::SceneManager::ClearCache() noexcept
{
    m_Snapshots.clear();
}

std::size_t asge::game::scene::SceneManager::CachedSceneCount() const noexcept
{
    return m_Snapshots.size();
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

asge::Result<asge::ecs::Entity> asge::game::scene::SceneManager::CreateEntity() noexcept
{
    if ( !m_CurrentScenePath )
        return Result<ecs::Entity>::Err( make_error_code( errors::SceneError::NoActiveScene ) );

    auto entity = m_Registry.CreateEntity();
    if ( !entity ) return entity;

    if ( auto tagResult = m_Registry.AddComponent<SceneId>( entity.Value(), SceneId{ *m_CurrentScenePath } ); !tagResult )
        return Result<ecs::Entity>::Err( tagResult.Error() );

    return entity;
}

asge::Result<asge::ecs::Entity> asge::game::scene::SceneManager::DuplicateEntity( ecs::Entity inEntity ) noexcept
{
    if ( !m_CurrentScenePath )
        return Result<ecs::Entity>::Err( make_error_code( errors::SceneError::NoActiveScene ) );

    auto const alive = m_Registry.AllEntities();
    if ( std::find( alive.begin(), alive.end(), inEntity ) == alive.end() )
    {
        return Result<ecs::Entity>::Err(
            make_error_code( errors::EcsError::EntityIsNotAlive ),
            "Id " + std::to_string( inEntity.m_Index ) );
    }

    auto entity = m_Registry.CreateEntity();
    if ( !entity ) return entity;

    CopyEntityComponents( m_Registry, inEntity, m_Registry, entity.Value() );

    if ( auto tagResult = m_Registry.AddComponent<SceneId>( entity.Value(), SceneId{ *m_CurrentScenePath } ); !tagResult )
        return Result<ecs::Entity>::Err( tagResult.Error() );

    return entity;
}
