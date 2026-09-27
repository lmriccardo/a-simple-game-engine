#include "SceneSerializer.hpp"
#include <vector>
#include <ASGE/Core/Configuration/TOML_Builder.hpp>
#include <ASGE/Game/Components.hpp>

#include "Serialize.hpp"
#include "IdContext.hpp"

asge::BoolResult asge::game::scene::SceneSerializer::Save(
    ecs::Registry const &inRegistry, filesystem::Path const &inPath) const noexcept
{
    config::toml::TOMLBuilder builder;
    
    // First I need to create the SaveContext and save newly created ids
    std::uint32_t nextId{ 0 };
    SaveContext ctx;
    inRegistry.ForEachEntity( [&](ecs::Entity const& e) {
        ctx.m_Ids[e] = nextId++;
    });

    // Then we need to serialize each entity and its connected components
    inRegistry.ForEachEntity( [&](ecs::Entity const& entity)
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
    } );

    return builder.SaveToFile( inPath );
}

asge::BoolResult asge::game::scene::SceneSerializer::Load(
    ecs::Registry &dstRegistry, str::String const &inVirtualPath) const noexcept
{
    // Resolve the input virtual path
    auto resolveResult = m_Vfs.Resolve( inVirtualPath );
    if ( !resolveResult ) return BoolResult::Err( resolveResult.Error() );
    return LoadResolved( dstRegistry, resolveResult.Value() );
}

asge::BoolResult asge::game::scene::SceneSerializer::LoadFromFile(
    ecs::Registry &dstRegistry, filesystem::Path const &inPath) const noexcept
{
    return LoadResolved( dstRegistry, inPath );
}

asge::BoolResult asge::game::scene::SceneSerializer::LoadResolved(
    ecs::Registry &dstRegistry, filesystem::Path const &scenePath) const noexcept
{
    // Read the content of the file
    auto readResult = filesystem::ReadText( scenePath );
    if ( !readResult ) return BoolResult::Err( readResult.Error() );

    // Parse the TOML file to construct the TOML Table
    auto parseResult = config::toml::Parse( readResult.Value() );
    if ( !parseResult ) return BoolResult::Err( parseResult.Error() );
    auto sceneTable = config::toml::TOMLTableView( parseResult.Value() );

    // Only entities *this call* creates get rolled back on failure --
    // whatever was already in dstRegistry before Load() was called is
    // never touched, whether Load ultimately succeeds or fails.
    std::vector<ecs::Entity> createdThisCall;
    auto rollback = [&]() noexcept
    {
        for ( auto entity : createdThisCall )
        {
            if ( auto destroyResult = dstRegistry.DestroyEntity( entity ); !destroyResult )
            {
                destroyResult.LogError();
            }
        }
    };

    std::size_t entityIndex{0};
    LoadContext ctx;

    // First we need to load all entities and save them into the load context
    do {
        str::String tableName = "entity[" + std::to_string(entityIndex++) + "]";
        auto getResult = sceneTable.GetTable( tableName );
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

        config::toml::TOMLTableView entityTable = getResult.Value();
        auto createResult = dstRegistry.CreateEntity();
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
    // Walks createdThisCall (not dstRegistry.AllEntities()) and re-starts
    // entityIndex from 0 -- both loops address the same "entity[N]" tables
    // in the same order, and a caller loading into a non-empty registry must
    // never touch entities the first loop didn't itself just create.
    entityIndex = 0;
    for ( auto entity : createdThisCall )
    {
        str::String tableName = "entity[" + std::to_string(entityIndex++) + "]";
        auto entityTableResult = sceneTable.GetTable(tableName);
        if ( !entityTableResult )
        {
            rollback();
            return BoolResult::Err( entityTableResult.Error() );
        }
        config::toml::TOMLTableView entityTable = entityTableResult.Value();
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
                    auto addResult = dstRegistry.AddComponent<T>( entity, c );
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

    return BoolResult::Ok();
}
