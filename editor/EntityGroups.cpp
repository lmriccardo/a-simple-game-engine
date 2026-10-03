#include "EntityGroups.hpp"

#include <algorithm>
#include <system_error>

#include <ASGE/Core/Configuration/TOML_Builder.hpp>
#include <ASGE/Core/Configuration/TOML_Parser.hpp>

namespace
{

std::unordered_map<std::string, EntityGroupList> g_Groups;

}

EntityGroupList& GroupsFor( std::string const& inScenePath ) noexcept
{
    return g_Groups[inScenePath];
}

void ClearAllGroups() noexcept
{
    g_Groups.clear();
}

void RenameGroupScene( std::string const& inOldScenePath, std::string const& inNewScenePath ) noexcept
{
    if ( inOldScenePath == inNewScenePath ) return;

    auto node = g_Groups.extract( inOldScenePath );
    if ( node.empty() ) return;

    g_Groups.insert_or_assign( inNewScenePath, std::move( node.mapped() ) );
}

void RemapGroupMembers(
    EntityGroupList& ioGroups, std::unordered_map<asge::ecs::Entity, asge::ecs::Entity> const& inRestored ) noexcept
{
    for ( auto& group : ioGroups )
    {
        std::vector<asge::ecs::Entity> remapped;
        for ( auto member : group.m_Members )
        {
            if ( auto it = inRestored.find( member ); it != inRestored.end() ) remapped.push_back( it->second );
        }
        group.m_Members = std::move( remapped );
    }
}

std::filesystem::path GroupsFilePath( std::filesystem::path const& inScenePath ) noexcept
{
    return std::filesystem::path( inScenePath.string() + ".groups" );
}

asge::BoolResult SaveGroupsFile(
    std::filesystem::path const& inScenePath, EntityGroupList const& inGroups,
    asge::game::scene::SaveContext const& inCtx ) noexcept
{
    auto const path = GroupsFilePath( inScenePath );

    if ( inGroups.empty() )
    {
        std::error_code ec;
        std::filesystem::remove( path, ec );
        return asge::BoolResult::Ok();
    }

    asge::config::toml::TOMLBuilder builder;
    for ( auto const& group : inGroups )
    {
        std::vector<int> ids;
        for ( auto member : group.m_Members )
        {
            if ( int const id = inCtx.Resolve( member ); id >= 0 ) ids.push_back( id );
        }

        auto table = builder.ArrayTable( "Group" );
        table.Set( "Name", group.m_Name );
        if ( !ids.empty() ) table.SetArray( "Members", ids );
    }

    return builder.SaveToFile( path );
}

EntityGroupList LoadGroupsFile(
    std::filesystem::path const& inScenePath, asge::game::scene::LoadContext const& inCtx ) noexcept
{
    EntityGroupList groups;

    auto const path = GroupsFilePath( inScenePath );
    std::error_code ec;
    if ( !std::filesystem::exists( path, ec ) ) return groups;

    auto parsed = asge::config::toml::Parse( path );
    if ( !parsed ) return groups;
    asge::config::toml::TOMLTableView const doc( parsed.Value() );

    for ( int i = 0;; ++i )
    {
        auto table = doc.GetTable( "Group", i );
        if ( !table ) break;

        EntityGroup group;
        group.m_Name = table.Value().Get<std::string>( "Name", std::string{} );
        for ( int id : table.Value().GetArray<int>( "Members" ) )
        {
            if ( auto entity = inCtx.Resolve( id ); entity != asge::ecs::Entity::Null() ) group.m_Members.push_back( entity );
        }
        groups.push_back( std::move( group ) );
    }

    return groups;
}
