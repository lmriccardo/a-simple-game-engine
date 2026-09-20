#include "TransformPropagationSystem.hpp"

#include <ASGE/Game/Components/Transform.hpp>
#include <ASGE/Game/Components/Hierarchy.hpp>

namespace
{

using namespace asge::ecs;
using namespace asge::game::components;
using namespace asge::math;

void ApplyPropagation( Transform& inChild, Transform const& inParentWorld ) noexcept
{
    inChild.m_WorldRotation = inParentWorld.m_WorldRotation + inChild.m_LocalRotation;

    inChild.m_WorldScale = Float2 {
        inParentWorld.m_WorldScale.x() * inChild.m_LocalScale.x(),
        inParentWorld.m_WorldScale.y() * inChild.m_LocalScale.y()
    };

    Float2 const scaledLocal { 
        inChild.m_LocalCoordinates.x() * inParentWorld.m_WorldScale.x(),
        inChild.m_LocalCoordinates.y() * inParentWorld.m_WorldScale.y()
    };

    auto rotation = Rotate(scaledLocal, inParentWorld.m_WorldRotation);
    inChild.m_WorldCoordinates = inParentWorld.m_WorldCoordinates + rotation;
    inChild.m_Dirty = false;
}

void PropagateToChildren( 
    Registry& inRegistry, Entity inParent, Transform const& inParentWorld, bool inParentMoved 
) noexcept
{
    auto parentResult = inRegistry.GetComponent<Hierarchy>(inParent);
    if ( !parentResult ) return;

    auto const& parentH = parentResult.Value().get();
    Entity child = parentH.m_FirstChild;
    while ( child != Entity::Null() )
    {
        // A child is only visited if it has a Transform; ForEachChild
        // just walks the sibling links regardless of what components exist.
        auto transformR = inRegistry.GetComponent<Transform>( child );
        if ( transformR.IsOk() )
        {
            auto childT = transformR.Value().get();
            bool childMoved = childT.m_Dirty || inParentMoved;
            if ( childMoved )
            {
                ApplyPropagation( childT, inParentWorld );
            }

            PropagateToChildren(inRegistry, child, childT, childMoved);
        }

        auto hierarchyR = inRegistry.GetComponent<Hierarchy>(child);
        if ( !hierarchyR ) break;
        child = hierarchyR.Value().get().m_NextSibling;
    }
}

}

void asge::game::systems::TransformPropagationSystem( asge::ecs::Registry& inRegistry ) noexcept
{
    for ( auto [entity, t] : inRegistry.View<asge::game::components::Transform>() )
    {
        auto& transform = t.get();
        auto h = inRegistry.GetComponent<asge::game::components::Hierarchy>(entity);
        if ( !h || h.Value().get().m_Parent == asge::ecs::Entity::Null() )
        {
            bool const wasDirty = transform.m_Dirty;

            if ( transform.m_Dirty )
            {
                transform.m_WorldCoordinates = transform.m_LocalCoordinates;
                transform.m_WorldRotation = transform.m_LocalRotation;
                transform.m_WorldScale = transform.m_LocalScale;
                transform.m_Dirty = false;
            }

            if ( h )
            {
                PropagateToChildren( inRegistry, entity, transform, wasDirty );
            }
        }
    }
}