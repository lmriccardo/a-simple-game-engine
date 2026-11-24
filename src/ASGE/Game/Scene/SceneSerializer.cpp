#include "SceneSerializer.hpp"
#include <utility>
#include <vector>
#include <ASGE/Core/Formats/TOML/TOML_Builder.hpp>
#include <ASGE/Game/Components.hpp>
#include <ASGE/Game/Assets/AssetErrors.hpp>

#include "Serialize.hpp"
#include "IdContext.hpp"

asge::BoolResult asge::game::scene::SceneSerializer::Save(
    ecs::Registry const &inRegistry, filesystem::Path const &inPath, SaveContext* outCtx) const noexcept
{
    std::vector<ecs::Entity> entities;
    inRegistry.ForEachEntity( [&](ecs::Entity const& e) { entities.push_back( e ); } );
    return WriteEntities( inRegistry, entities, inPath, outCtx );
}

asge::BoolResult asge::game::scene::SceneSerializer::SavePrefab(
    ecs::Registry const &inRegistry, ecs::Entity inRoot, filesystem::Path const &inPath, SaveContext* outCtx) const noexcept
{
    // Root first (so it gets Id 0), then its descendants, walking each child list
    std::size_t const maxEntities = inRegistry.AllEntities().size(); // stops a corrupt cyclic list
    std::vector<ecs::Entity> subtree{ inRoot };
    for ( std::size_t i = 0; i < subtree.size(); ++i )
    {
        auto h = inRegistry.GetComponent<ecs::components::Hierarchy>( subtree[i] );
        if ( !h ) continue;

        ecs::Entity child = h.Value().get().m_FirstChild;
        while ( child != ecs::Entity::Null() && subtree.size() <= maxEntities )
        {
            subtree.push_back( child );
            auto ch = inRegistry.GetComponent<ecs::components::Hierarchy>( child );
            if ( !ch ) break;
            child = ch.Value().get().m_NextSibling;
        }
    }

    return WriteEntities( inRegistry, subtree, inPath, outCtx );
}

asge::BoolResult asge::game::scene::SceneSerializer::WriteEntities(
    ecs::Registry const &inRegistry, std::vector<ecs::Entity> const &inEntities,
    filesystem::Path const &inPath, SaveContext *outCtx) const noexcept
{
    formats::toml::TOMLBuilder builder;

    // First I need to create the SaveContext and save newly created ids. Links
    // to entities outside inEntities resolve to -1, so a subtree root is a root.
    std::uint32_t nextId{ 0 };
    SaveContext ctx;
    for ( auto const& e : inEntities ) ctx.m_Ids[e] = nextId++;

    // Then we need to serialize each entity and its connected components
    for ( auto const& entity : inEntities )
    {
        auto entityTable = builder.ArrayTable( "entity" );
        entityTable.Set( "Id", ctx.Resolve( entity ) );

        std::apply( [&]( auto ... component )
        {
            // Fold over the pack: one ToToml call per component type
            // the entity actually has, each writing into entityTable.
            ( [&]
                {
                    using T = decltype(component);
                    if ( inRegistry.HasComponent<T>(entity) )
                    {
                        Serializer<T>::ToToml(
                            inRegistry.GetComponent<T>( entity ).Value().get(),
                            entityTable,
                            ctx
                        );
                    }
                }(), ... );
        }, components::SerializableComponents{} );
    }

    if ( auto result = builder.SaveToFile( inPath ); !result ) return result;

    if ( outCtx ) *outCtx = std::move( ctx );
    return BoolResult::Ok();
}

asge::BoolResult asge::game::scene::SceneSerializer::Load(
    ecs::Registry &dstRegistry, str::String const &inVirtualPath, LoadContext* outCtx) const noexcept
{
    // Resolve the input virtual path
    auto resolveResult = m_Vfs.Resolve( inVirtualPath );
    if ( !resolveResult ) return BoolResult::Err( resolveResult.Error() );
    return LoadResolved( dstRegistry, resolveResult.Value(), outCtx );
}

asge::BoolResult asge::game::scene::SceneSerializer::LoadFromFile(
    ecs::Registry &dstRegistry, filesystem::Path const &inPath, LoadContext* outCtx) const noexcept
{
    return LoadResolved( dstRegistry, inPath, outCtx );
}

asge::Result<asge::ecs::Entity> asge::game::scene::SceneSerializer::Instantiate(
    ecs::Registry &outRegistry, game::asset::ScenePrefab const& inPrefab,
    std::optional<game::components::Transform> inTransform) const noexcept
{
    LoadContext ctx;

    if ( auto r = Populate( outRegistry, inPrefab.m_Table, &ctx ); !r )
    {
        return asge::Result<asge::ecs::Entity>::Err( r.Error() );
    }

    // Populate succeeded, so a failure from here on must undo the whole instance
    auto fail = [&]( errors::_internal::ErrorInfo inError ) noexcept
    {
        for ( auto const& [id, entity] : ctx.m_Entities )
        {
            if ( auto d = outRegistry.DestroyEntity( entity ); !d ) d.LogError();
        }
        return asge::Result<ecs::Entity>::Err( inError );
    };

    auto rootEntity = ctx.Resolve(0);
    if ( rootEntity == ecs::Entity::Null() )
    {
        return fail( { make_error_code( errors::AssetError::ScenePrefab_UnresolvedRootEntity ), {} } );
    }

    if ( inTransform.has_value() )
    {
        if ( auto r = outRegistry.AddComponent( rootEntity, *inTransform ); !r )
        {
            return fail( r.Error() );
        }
    }

    return asge::Result<ecs::Entity>::Ok( rootEntity );
}

asge::BoolResult asge::game::scene::SceneSerializer::LoadResolved(
    ecs::Registry &dstRegistry, filesystem::Path const &scenePath, LoadContext* outCtx) const noexcept
{
    // Read the content of the file
    auto readResult = filesystem::ReadText( scenePath );
    if ( !readResult ) return BoolResult::Err( readResult.Error() );

    // Parse the TOML file to construct the TOML Table
    auto parseResult = formats::toml::Parse( readResult.Value() );
    if ( !parseResult ) return BoolResult::Err( parseResult.Error() );
    auto sceneTable = formats::toml::TOMLTableView( parseResult.Value() );

    return Populate( dstRegistry, sceneTable, outCtx );
}

asge::BoolResult asge::game::scene::SceneSerializer::Populate(
    ecs::Registry &outRegistry, formats::toml::TOMLTableView inTv, LoadContext *outCtx) const noexcept
{
    // Only entities *this call* creates get rolled back on failure --
    // whatever was already in outRegistry before Populate() was called is
    // never touched, whether it ultimately succeeds or fails.
    std::vector<ecs::Entity> createdThisCall;
    auto rollback = [&]() noexcept
    {
        for ( auto entity : createdThisCall )
        {
            if ( auto destroyResult = outRegistry.DestroyEntity( entity ); !destroyResult )
            {
                destroyResult.LogError();
            }
        }
    };

    std::size_t entityIndex{0};
    LoadContext ctx;

    // First we need to load all entities and save them into the load context
    do {
        auto getResult = inTv.GetTable( "entity", entityIndex );
        if ( !getResult )
        {
            // Running out of [[entity]] elements is the normal, expected
            // way this loop ends -- not a real failure -- so only clean up
            // and propagate anything else.
            if ( getResult.Code() == make_error_code( errors::ConfError::TomlNoSubtable ) )
            {
                break;
            }
            rollback();
            return BoolResult::Err( getResult.Error() );
        }
        ++entityIndex;

        formats::toml::TOMLTableView entityTable = getResult.Value();
        auto createResult = outRegistry.CreateEntity();
        if ( !createResult )
        {
            rollback();
            return BoolResult::Err( createResult.Error() );
        }

        createdThisCall.push_back( createResult.Value() );
        int entityId = entityTable.Get<int>( "Id", int{} );
        ctx.m_Entities[entityId] = createResult.Value();

    } while ( true );

    // Then for each entity we shall take all components that are defined into
    // the input TOML configuration scene file and create it in the registry.
    // Walks createdThisCall (not outRegistry.AllEntities()) and re-starts
    // entityIndex from 0 -- both loops address the same "entity[N]" tables
    // in the same order, and a caller loading into a non-empty registry must
    // never touch entities the first loop didn't itself just create.
    entityIndex = 0;
    for ( auto entity : createdThisCall )
    {
        auto entityTableResult = inTv.GetTable("entity", entityIndex);
        if ( !entityTableResult )
        {
            rollback();
            return BoolResult::Err( entityTableResult.Error() );
        }
        ++entityIndex;
        formats::toml::TOMLTableView entityTable = entityTableResult.Value();
        bool result{true};
        errors::_internal::ErrorInfo errInfo;

        std::apply( [&]( auto ... component )
        {
            ( [&] 
            {
                if ( !result ) return;
                using T = decltype(component);
                constexpr auto tName = Serializer<T>::kTableName;
                if ( entityTable.HasTable( str::String(tName) ) )
                {
                    T c = Serializer<T>::FromToml( entityTable, ctx );
                    auto addResult = outRegistry.AddComponent<T>( entity, c );
                    if ( !addResult )
                    {
                        result = false;
                        errInfo = addResult.Error();
                    }
                } 
            }(), ...);
        }, components::SerializableComponents{});

        if ( !result )
        {
            rollback();
            return BoolResult::Err( errInfo );
        }
    }

    // A scene file's Hierarchy links are stored verbatim and can be stale or
    // cyclic (an entity its own parent hangs every walk up the parent chain),
    // so rebuild them from the parent pointers before anything walks them.
    ecs::components::SanitizeHierarchy( outRegistry, createdThisCall );

    if ( outCtx ) *outCtx = std::move( ctx );
    return BoolResult::Ok();
}
