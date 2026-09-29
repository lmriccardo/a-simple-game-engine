#include "Hierarchy.hpp"

#include "Transform.hpp"

void asge::game::components::AttachChild(
    ecs::Registry &inReg, ecs::Entity inParent, ecs::Entity inChild)
{
    ecs::components::AttachChild( inReg, inParent, inChild );

    // Mark dirty so the next TransformPropagationSystem pass recomputes
    // this child's world transform relative to its new parent — local
    // coordinates are unchanged, but their world composition now is.
    auto childTResult = inReg.GetComponent<Transform>(inChild);
    if ( childTResult.IsOk() )
    {
        childTResult.Value().get().m_Dirty = true;
    }
}

void asge::game::components::DetachChild(ecs::Registry &inReg, ecs::Entity inChild)
{
    ecs::components::DetachChild( inReg, inChild );

    // Detached subtree is now root-relative; mark dirty so its world
    // transform recomputes as local (no longer composed through the old parent)
    auto childTResult = inReg.GetComponent<Transform>(inChild);
    if ( childTResult.IsOk() )
    {
        childTResult.Value().get().m_Dirty = true;
    }
}

void asge::game::scene::Serializer<asge::ecs::components::Hierarchy>::ToToml(
    ecs::components::Hierarchy inHierarchy, asge::config::toml::TOMLTableView inTview, 
    SaveContext const& inCtx ) noexcept
{
    inTview.Table( str::String(kTableName) )
           .Set("m_Parent",       inCtx.Resolve(inHierarchy.m_Parent))
           .Set("m_FirstChild",   inCtx.Resolve(inHierarchy.m_FirstChild))
           .Set("m_LastChild",    inCtx.Resolve(inHierarchy.m_LastChild))
           .Set("m_PrevSibling", inCtx.Resolve(inHierarchy.m_PrevSibling))
           .Set("m_NextSibling", inCtx.Resolve(inHierarchy.m_NextSibling));
}

asge::ecs::components::Hierarchy 
asge::game::scene::Serializer<asge::ecs::components::Hierarchy>::FromToml(
    asge::config::toml::TOMLTableView inEnttView, LoadContext const& inCtx ) noexcept
{
    auto table = inEnttView.Table( str::String(kTableName) );
    ecs::components::Hierarchy result;
    result.m_Parent      = inCtx.Resolve( table.Get<int>( "m_Parent", -1 ) );
    result.m_FirstChild  = inCtx.Resolve( table.Get<int>( "m_FirstChild", -1 ) );
    result.m_LastChild   = inCtx.Resolve( table.Get<int>( "m_LastChild", -1 ) );
    result.m_PrevSibling = inCtx.Resolve( table.Get<int>( "m_PrevSibling", -1 ) );
    result.m_NextSibling = inCtx.Resolve( table.Get<int>( "m_NextSibling", -1 ) );
    return result;
}