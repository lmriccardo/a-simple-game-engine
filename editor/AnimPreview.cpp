#include "AnimPreview.hpp"

#include <optional>
#include <vector>

#include <ASGE/Game/Components/Animator.hpp>
#include <ASGE/Game/Components/Sprite.hpp>

namespace
{

using asge::game::components::Animator;
using asge::game::components::Sprite;

struct Saved
{
    asge::ecs::Entity         m_Entity;
    Animator                  m_Animator;
    std::optional<asge::math::Rect> m_Rect;
};

std::vector<Saved> g_Saved;
bool               g_Active = false;

}

void BeginAnimPreview( asge::ecs::Registry& inRegistry ) noexcept
{
    if ( g_Active ) return;
    g_Active = true;
    g_Saved.clear();
    for ( auto [ e, a, s ] : inRegistry.View<Animator, Sprite>() )
    {
        auto& animator = a.get();
        g_Saved.push_back( Saved{ e, animator, s.get().m_SourceRect } );
        animator.m_Elapsed = 0.0f;
        animator.m_Frame = 0;
        animator.m_Flags = 0;
        animator.m_Triggers = 0;
        animator.m_State = 0;
        animator.m_Status = asge::game::components::AnimStatus::Playing;
        animator.m_Dirty = true;
        animator.m_Started = false;
        animator.m_OwnsRect = false;
        animator.m_RestRect.reset();
    }
}

void EndAnimPreview( asge::ecs::Registry& inRegistry ) noexcept
{
    if ( !g_Active ) return;
    for ( auto& saved : g_Saved )
    {
        auto animator = inRegistry.GetComponent<Animator>( saved.m_Entity );
        auto sprite = inRegistry.GetComponent<Sprite>( saved.m_Entity );
        if ( !animator || !sprite ) continue;
        animator.Value().get() = saved.m_Animator;
        sprite.Value().get().m_SourceRect = saved.m_Rect;
    }
    g_Saved.clear();
    g_Active = false;
}

bool IsAnimPreviewing() noexcept { return g_Active; }
