#include "Hierarchy.hpp"

#include <unordered_map>
#include <unordered_set>
#include <vector>

void asge::ecs::components::AttachChild(
    ecs::Registry &inReg, ecs::Entity inParent, ecs::Entity inChild)
{
    if ( inParent == inChild ) return;
    if ( IsAncestor( inReg, inChild, inParent ) ) return;

    auto& parentH = inReg.GetOrAddComponent<Hierarchy>( inParent ).get();
    auto& childH  = inReg.GetOrAddComponent<Hierarchy>( inChild ).get();

    if ( childH.m_Parent != ecs::Entity::Null() )
    {
        // re-parenting: unlink from old parent first
        DetachChild( inReg, inChild );
    }

    childH.m_Parent = inParent;
    childH.m_PrevSibling = parentH.m_LastChild;
    childH.m_NextSibling = ecs::Entity::Null();

    if ( parentH.m_LastChild != ecs::Entity::Null() )
    {
        inReg.GetComponent<Hierarchy>( parentH.m_LastChild ).Value().get().m_NextSibling = inChild;
    }
    else
    {
        // parent had no children before this
        parentH.m_FirstChild = inChild;
    }

    parentH.m_LastChild = inChild;
}

void asge::ecs::components::DetachChild(ecs::Registry &inReg, ecs::Entity inChild)
{
    auto childHResult = inReg.GetComponent<Hierarchy>(inChild);
    if ( !childHResult || ( childHResult.Value().get().m_Parent == ecs::Entity::Null() ) ) return;
    
    auto& childH = childHResult.Value().get();
    
    // Splice out the sibiling list.
    if ( childH.m_PrevSibling != ecs::Entity::Null() )
    {
        auto& prevH = inReg.GetComponent<Hierarchy>( childH.m_PrevSibling ).Value().get();
        prevH.m_NextSibling = childH.m_NextSibling;
    }

    if ( childH.m_NextSibling != ecs::Entity::Null() )
    {
        auto& nextH = inReg.GetComponent<Hierarchy>( childH.m_NextSibling ).Value().get();
        nextH.m_PrevSibling = childH.m_PrevSibling;
    }

    auto& parentH = inReg.GetComponent<Hierarchy>( childH.m_Parent ).Value().get();
    if ( parentH.m_FirstChild == inChild ) parentH.m_FirstChild = childH.m_NextSibling;
    if ( parentH.m_LastChild == inChild ) parentH.m_LastChild = childH.m_PrevSibling;

    childH.m_Parent = ecs::Entity::Null();
    childH.m_NextSibling = ecs::Entity::Null();
    childH.m_PrevSibling = ecs::Entity::Null();
}

bool asge::ecs::components::IsAncestor(
    ecs::Registry &inReg, ecs::Entity inPotentialAncestor, ecs::Entity inEntity)
{
    auto hResult = inReg.GetComponent<Hierarchy>( inEntity );
    if ( !hResult ) return false;

    ecs::Entity current = hResult.Value().get().m_Parent;
    while ( current != ecs::Entity::Null() )
    {
        if ( current == inPotentialAncestor ) return true;
        auto currentHResult = inReg.GetComponent<Hierarchy>( current );
        if ( !currentHResult ) break;
        current = currentHResult.Value().get().m_Parent;
    }

    return false;
}

void asge::ecs::components::SanitizeHierarchy(
    ecs::Registry &inReg, std::vector<ecs::Entity> const &inEntities )
{
    std::vector<ecs::Entity> members;
    for ( auto entity : inEntities )
    {
        if ( inReg.HasComponent<Hierarchy>( entity ) ) members.push_back( entity );
    }
    std::unordered_set<ecs::Entity> const memberSet( members.begin(), members.end() );

    // Everything is read from the stored links before any of them is reset.
    auto const stored = [&]( ecs::Entity inEntity ) -> Hierarchy const&
    { return inReg.GetComponent<Hierarchy>( inEntity ).Value().get(); };

    // Intended parent: only another member counts, never the entity itself.
    std::unordered_map<ecs::Entity, ecs::Entity> parentOf;
    for ( auto entity : members )
    {
        auto const parent = stored( entity ).m_Parent;
        parentOf[entity] = ( parent != entity && memberSet.contains( parent ) ) ? parent : ecs::Entity::Null();
    }

    // Sibling order: each parent's stored first-child chain, for as long as
    // it stays consistent (every link a member of that parent, none repeated,
    // bounded by the member count so a cycle can't spin).
    std::unordered_map<ecs::Entity, std::vector<ecs::Entity>> children;
    std::unordered_set<ecs::Entity> placed;
    for ( auto parent : members )
    {
        auto current = stored( parent ).m_FirstChild;
        for ( std::size_t steps = 0; current != ecs::Entity::Null() && steps < members.size(); ++steps )
        {
            auto const it = parentOf.find( current );
            if ( it == parentOf.end() || it->second != parent || !placed.insert( current ).second ) break;
            children[parent].push_back( current );
            current = stored( current ).m_NextSibling;
        }
    }

    // Children a chain didn't reach still belong to their parent, after it.
    for ( auto entity : members )
    {
        if ( parentOf[entity] != ecs::Entity::Null() && !placed.contains( entity ) )
        {
            children[parentOf[entity]].push_back( entity );
        }
    }

    // Rebuild from scratch. AttachChild refuses to close a cycle, so any
    // multi-entity loop is broken here, leaving the later child a root.
    for ( auto entity : members ) inReg.GetComponent<Hierarchy>( entity ).Value().get() = Hierarchy{};
    for ( auto parent : members )
    {
        auto const it = children.find( parent );
        if ( it == children.end() ) continue;
        for ( auto child : it->second ) AttachChild( inReg, parent, child );
    }
}

void asge::ecs::components::DestroyEntityGraph(ecs::Registry &inReg, ecs::Entity inRoot)
{
    std::vector<ecs::Entity> children;
    ForEachChild( inReg, inRoot, [&](ecs::Entity child) { children.push_back(child); } );
    for ( auto child : children ) DestroyEntityGraph( inReg, child );
    if ( auto result = inReg.DestroyEntity( inRoot ); !result )
    {
        result.LogError();
    }
}