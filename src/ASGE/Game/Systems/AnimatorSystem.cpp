#include "AnimatorSystem.hpp"

#include <algorithm>
#include <cmath>

#include <ASGE/Core/Math/Geometry/Angle.hpp>
#include <ASGE/Game/Components/Sprite.hpp>
#include <ASGE/Game/Components/Animator.hpp>
#include <ASGE/Game/Events.hpp>

namespace
{

using namespace asge::game::components;
using namespace asge::game::asset;

struct AdvanceResult
{
    bool m_FrameChanged {false};
    bool m_Finished     {false};
};

void EnterState( Animator& ioAnim, std::uint16_t inState ) noexcept
{
    ioAnim.m_State   = inState;
    ioAnim.m_Frame   = 0;
    ioAnim.m_Elapsed = 0.0f;
    ioAnim.m_Status  = AnimStatus::Playing;
    ioAnim.m_Dirty   = true;
}

bool TakeSourceRect( Animator& ioAnimator, Sprite const& inSprite ) noexcept
{
    if ( ioAnimator.m_OwnsRect ) return false;
    ioAnimator.m_RestRect = inSprite.m_SourceRect;
    ioAnimator.m_OwnsRect = true;
    return true;
}

AdvanceResult AdvanceTime( 
    Animator& ioAnimator, AnimationStateMachine::State const& inState,
    AnimationClipSet::Binding const& inBinding, FrameTable const& inTable,
    float inDeltaTime ) noexcept
{
    AdvanceResult result;
    auto const count = inTable.m_Frames.size();
    
    if ( ioAnimator.m_Frame >= count )
    {
        ioAnimator.m_Frame = 0;
        result.m_FrameChanged = true;
    }

    auto const before = ioAnimator.m_Frame;
    ioAnimator.m_Elapsed += inDeltaTime * ioAnimator.m_PlaybackSpeed;
    while ( true )
    {
        float const duration = inTable.FrameDuration( ioAnimator.m_Frame, inBinding.m_FrameDuration );
        if ( duration <= 0.0f || ioAnimator.m_Elapsed < duration ) break;
        
        // Remove the duration from the elapsed time of the entire frame
        ioAnimator.m_Elapsed -= duration;
        if ( ++ioAnimator.m_Frame >= count )
        {
            // If the animator has reached the end of the frame table, it
            // needs to check if the current state when ending is either
            // loop or not. If it is loop when it just resets the frame
            // to the first value.
            if ( inState.m_End == EndMode::Loop ) { ioAnimator.m_Frame = 0; continue; }

            ioAnimator.m_Frame  = static_cast<std::uint32_t>( count - 1 );
            ioAnimator.m_Status = AnimStatus::Finished;
            ioAnimator.m_Dirty  = true;
            result.m_Finished   = true;
            break;
        }
    }

    result.m_FrameChanged = result.m_FrameChanged || ioAnimator.m_Frame != before;
    return result;
}

bool TryTransition( Animator& ioAnimator, AnimationStateMachine const& inMachine ) noexcept
{
    for ( auto const& t : inMachine.TransitionsOf(ioAnimator.m_State) )
    {
        if ( ( ioAnimator.m_Flags & t.m_FlagsSet ) != t.m_FlagsSet )            continue;
        if ( ( ioAnimator.m_Flags & t.m_FlagsClear ) != 0 )                     continue;
        if ( t.m_Trigger != 0 && ( ioAnimator.m_Triggers & t.m_Trigger ) == 0 ) continue;
        if ( t.m_AtEnd && ioAnimator.m_Status != AnimStatus::Finished )         continue;

        ioAnimator.m_Triggers &= ~t.m_Trigger;
        EnterState( ioAnimator, t.m_To );
        return true;
    }

    return false;
}

// a heading must be this much closer to a new variant to leave the current one
constexpr float kVariantBias = 1.2f;

struct PickedVariant
{
    FrameTable const* m_Table;
    bool              m_Changed;
};

PickedVariant PickVariant( Animator& ioAnimator, AnimationClipSet::Binding const& inBinding ) noexcept
{
    auto const count = inBinding.m_Variants.size();
    if ( count <= 1 ) return { &inBinding.m_Variants.front()->Get(), false };

    // Variants run clockwise from north; AngleOf is counter-clockwise from +X with +Y up.
    float const ccw = asge::math::AngleOf( ioAnimator.m_Heading.x(), -ioAnimator.m_Heading.y() );
    float const cw  = std::fmod( asge::math::kPi * 0.5f - ccw + asge::math::kTwoPi, asge::math::kTwoPi );

    int const variant = asge::math::QuantizeAngle(
        cw, static_cast<int>( count ), ioAnimator.m_Variant, kVariantBias );

    bool const changed = variant != ioAnimator.m_Variant;
    ioAnimator.m_Variant = static_cast<std::uint8_t>( variant );
    return { &inBinding.m_Variants[variant]->Get(), changed };
}

}

void asge::game::systems::AnimatorSystem(ecs::Registry &inRegistry, float inDeltaTime)
{
    for ( auto [ e, a, s ] : 
        inRegistry.View<components::Animator, components::Sprite>() )
    {
        auto& animator = a.get();
        auto& sprite = s.get();
        if ( !animator.m_ClipSet ) continue;
        
        auto const& set         = animator.m_ClipSet->Get();
        auto const& machine     = set.Machine();
        auto const  states      = machine.States();
        auto const  bindings    = set.Bindings();

        if ( !animator.m_Started )
        {
            // The animator has not started yet. In this case we need to set
            // the initial state into the state machine. If the serialized
            // start playing is false the animator must be immediately stopped.
            animator.m_Started = true;
            EnterState( animator, machine.Initial() );
            if ( !animator.m_StartPlaying ) animator.m_Status = components::AnimStatus::Stopped;
        }

        if ( animator.m_Status == components::AnimStatus::Stopped )
        {
            // If the animator is stopped and owned the source rect of the sprite
            // it must release and give the rect to the sprite.
            if ( animator.m_OwnsRect )
            {
                sprite.m_SourceRect = animator.m_RestRect;
                animator.m_OwnsRect = false;
            }
            continue;
        }

        if ( 
                (!sprite.m_Texture)
            ||  (components::IsFinished(animator) && !animator.m_Dirty)
        ) {
            continue;
        }

        auto const& state   = states[animator.m_State];
        auto const& binding = bindings[state.m_ClipId];
        auto const  picked  = PickVariant( animator, binding );
        auto const& table   = *picked.m_Table;
        auto const  count   = table.m_Frames.size();
        if ( count == 0 ) continue; // No frames in the current frame table

        bool write = picked.m_Changed, finishedNow = false, taken = false;
        if ( 
                animator.m_Status == components::AnimStatus::Playing 
            &&  animator.m_PlaybackSpeed > 0.0f )
        {
            // Advance the time and so the animation by increasing the current
            // animator clip index which will points to the correct frame table
            // clip to animate
            write = TakeSourceRect( animator, sprite ) || write;
            auto const r = AdvanceTime( animator, state, binding, table, inDeltaTime );
            write = write || r.m_FrameChanged;
            finishedNow = r.m_Finished;
        }

        if ( animator.m_Dirty )
        {
            taken = TryTransition( animator, machine );
            if ( taken ) { TakeSourceRect( animator, sprite ); write = true; }
            animator.m_Dirty = taken;
        }

        // Write the Sprite once. A finish with no transition applies 
        // Restore, never the last frame.
        if ( finishedNow && !taken && state.m_End == EndMode::Restore )
        {
            sprite.m_SourceRect = animator.m_RestRect;
            animator.m_OwnsRect = false;
        }
        else if ( write )
        {
            auto const& cur = *PickVariant( 
                animator, 
                bindings[ states[animator.m_State].m_ClipId ] ).m_Table;

            if ( !cur.m_Frames.empty() )
            {
                auto frameIdx = std::min<std::size_t>( animator.m_Frame, cur.m_Frames.size() - 1 );
                sprite.m_SourceRect = cur.m_Frames[frameIdx];
            }
        }

        if ( finishedNow ) events::OnAnimationFinished().Emit( e );
    }
}