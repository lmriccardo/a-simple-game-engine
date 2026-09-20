#include "Hierarchy.hpp"

#include "Transform.hpp"

void asge::game::components::AttachChild(
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
        inReg.GetComponent<Hierarchy>( parentH.m_LastChild ).Value()
             .get().m_NextSibling = inChild;
    }
    else
    {
        // parent had no children before this
        parentH.m_FirstChild = inChild;
    }

    parentH.m_LastChild = inChild;

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

    // Detached subtree is now root-relative; mark dirty so its world
    // transform recomputes as local (no longer composed through the old parent)
    auto childTResult = inReg.GetComponent<Transform>(inChild);
    if ( childTResult.IsOk() )
    {
        childTResult.Value().get().m_Dirty = true;
    }
}

bool asge::game::components::IsAncestor(
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
