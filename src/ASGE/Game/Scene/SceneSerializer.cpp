#include "SceneSerializer.hpp"
#include <vector>
#include <ASGE/Core/Configuration/TOML_Builder.hpp>
#include <ASGE/Game/Components.hpp>
#include <ASGE/Game/Components/Serialize.hpp>

asge::BoolResult asge::game::scene::SceneSerializer::Save(
    ecs::Registry const &inRegistry, filesystem::Path const &inPath) const noexcept
{
    config::TOMLBuilder builder;
    for ( auto const& entity : inRegistry.AllEntities() )
    {
        auto entityTable = builder.ArrayTable( "entity" );
        std::apply( [&]( auto ... component )
            {
                // Fold over the pack: one ToToml call per component type
                // the entity actually has, each writing into entityTable.
                ( [&]
                  {
                      using T = decltype(component);
                      if ( inRegistry.HasComponent<T>(entity) )
                      {
                          components::Serializer<T>::ToToml(
                              inRegistry.GetComponent<T>( entity ).Value().get(),
                              entityTable
                          );
                      }
                  }(), ... );
            }, components::SerializableComponents{} );
    }

    return builder.SaveToFile( inPath );
}

asge::BoolResult asge::game::scene::SceneSerializer::Load(
    ecs::Registry &dstRegistry, str::String const &inVirtualPath) const noexcept
{
    // Resolve the input virtual path
    auto resolveResult = m_Vfs.Resolve( inVirtualPath );
    if ( !resolveResult ) return BoolResult::Err( resolveResult.Error() );
    filesystem::Path scenePath = resolveResult.Value();

    // Read the content of the file
    auto readResult = filesystem::ReadText( scenePath );
    if ( !readResult ) return BoolResult::Err( readResult.Error() );

    // Parse the TOML file to construct the TOML Table
    auto parseResult = config::_internal::toml::Parse( readResult.Value() );
    if ( !parseResult ) return BoolResult::Err( parseResult.Error() );
    auto sceneTable = config::TOMLTableView( parseResult.Value() );

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

        config::TOMLTableView entityTable = getResult.Value();
        auto createResult = dstRegistry.CreateEntity();
        if ( !createResult )
        {
            rollback();
            return BoolResult::Err( createResult.Error() );
        }
        createdThisCall.push_back( createResult.Value() );

        bool result{true};
        errors::_internal::ErrorInfo errInfo;
        std::apply( [&]( auto ... component )
            {
                ( [&] 
                {
                    if ( !result ) return;
                    using T = decltype(component);
                    constexpr auto tName = components::Serializer<T>::kTableName;
                    if ( entityTable.HasTable( str::String(tName) ) )
                    {
                        T c = components::Serializer<T>::FromToml( entityTable );
                        auto addResult = dstRegistry.AddComponent<T>( createResult.Value(), c );
                        if ( !addResult )
                        {
                            result = false;
                            errInfo = addResult.Error();
                        }
                    } 
                }(), ...);
            }, components::SerializableComponents{} 
        );

        if ( !result )
        {
            rollback();
            return BoolResult::Err( errInfo );
        }

    } while ( true );

    return BoolResult::Ok();
}
