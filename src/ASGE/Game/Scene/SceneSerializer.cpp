#include "SceneSerializer.hpp"
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

    std::size_t entityIndex{0};
    do {
        str::String tableName = "entity[" + std::to_string(entityIndex++) + "]";
        auto entityTable = sceneTable.GetTable( tableName );
        if ( !entityTable ) break;

        ecs::Entity currEntity = dstRegistry.CreateEntity();

    } while ( true );

    return BoolResult::Ok();
}
