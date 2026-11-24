#include "ScenePrefab.hpp"

#include <limits>

#include <ASGE/Game/Assets/AssetErrors.hpp>
#include <ASGE/Game/Components/Hierarchy.hpp>

using namespace asge;
using namespace asge::game::asset;
using namespace asge::formats::toml;

constexpr str::StringView k_HierarchyTableName = 
    asge::game::scene::Serializer<asge::ecs::components::Hierarchy>::kTableName;

asge::game::asset::ScenePrefab::ScenePrefab(formats::toml::TOMLTableView inTableView)
    : m_Table( std::move(inTableView) )
{
}

Result<ScenePrefab> asge::game::asset::ScenePrefab::Load(filesystem::Path const &inPath)
{
    auto parseResult = Parse( inPath );
    if ( !parseResult )
    {
        return Result<ScenePrefab>::Err( parseResult.Error() );
    }

    TOMLTableView table( std::move(parseResult.Value()) );

    // Sanitize the scene prefab table. There can be a generic number of entities, 
    // but each of them must be a child of another prefab entity, except for the
    // only possible root. If it is not, then the prefab is invalid.

    std::size_t n_RootEntities{0}, n_Entities{0}, n_NonHierarchyEntities{0};
    bool hasHierarchyDefined{false};

    do {
        auto const result = table.GetTable("entity", n_Entities);
        if ( !result ) break; // No more entities to parse
        auto& entityTable = result.Value();

        // Check for hierarchy component attached or not
        auto const hResult = entityTable.GetTable(str::String(k_HierarchyTableName));
        if ( !hResult ) ++n_NonHierarchyEntities;
        else
        {
            if ( !hasHierarchyDefined ) hasHierarchyDefined = true;
            if ( hResult.Value().Get<int>( "m_Parent", -1 ) < 0 )
            {
                ++n_RootEntities;
            }
        }

        ++n_Entities;
    } while ( true );

    if ( n_Entities < 1 )
    {
        // At least one entity must be defined in the scene prefab
        return Result<ScenePrefab>::Err( 
            make_error_code( asge::errors::AssetError::ScenePrefab_AtLeastOneEntity ) 
        );
    }
    
    if ( n_RootEntities < 1 && hasHierarchyDefined )
    {
        // In this case there is at least one entity with Hierarchy defined, but
        // none of them has parent Entity::Null, or -1 in the TOML file.
        return Result<ScenePrefab>::Err( 
            make_error_code( asge::errors::AssetError::ScenePrefab_MissingRootEntity ) 
        );
    }

    if ( n_RootEntities + n_NonHierarchyEntities > 1 )
    {
        // There are too many roots. A root is either an entity without any
        // Hierarchy component attached at all, or with a m_Parent value of -1.
        return Result<ScenePrefab>::Err( 
            make_error_code( asge::errors::AssetError::ScenePrefab_TooManyRoots ) 
        );
    }

    return Result<ScenePrefab>::Ok( ScenePrefab( table ) );
}