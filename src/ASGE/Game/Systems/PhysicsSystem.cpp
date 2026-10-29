#include "PhysicsSystem.hpp"

#include "../Components/Transform.hpp"
#include "../Components/Velocity.hpp"
#include "../Components/Collider.hpp"
#include "../Components/Rigidbody.hpp"
#include "../Components/PathFollow.hpp"
#include "../Components/Hierarchy.hpp"
#include "../Events.hpp"

#include <ASGE/Core/Math/Geometry/Collision.hpp>
#include <ASGE/Core/Spatial/QuadTree.hpp>

#include "TransformPropagationSystem.hpp"

#include <ASGE/Core/Logger/Logger.hpp>

#include <cmath>
#include <optional>
#include <set>
#include <unordered_map>
#include <vector>

namespace
{
using namespace asge::game::components;
using namespace asge::ecs;
using namespace asge::math;

using AppliedCorrections = std::unordered_map<Entity, Float2>;

constexpr float         kGravity    = 980.0f; // pixel/s^2
constexpr std::size_t   kNodeBytes  = 72;
constexpr std::size_t   kBlockBytes = 16 + 16 * (16 + sizeof(asge::ecs::Entity));

constexpr std::size_t GetArenaSize() noexcept
{
    return ( kMaxEntities / 6 ) * ( kNodeBytes + kBlockBytes ) * 2 + 64 * 1024;
}

// Applies a positional correction and zeroes velocity on whichever axis moved.
// Updates both Local and World coordinates together (rather than just Local +
// m_Dirty) since collision resolution runs after TransformPropagationSystem
// in PhysicsUpdate -- deferring to next frame's propagation would leave this
// frame's rendering one step stale.
void ApplyCorrection( Transform& inT, Velocity& inV, asge::math::Float2 const& inDelta ) noexcept
{
    if ( inDelta.x() == 0.0f && inDelta.y() == 0.0f ) return;

    inT.m_LocalCoordinates.x() += inDelta.x();
    inT.m_LocalCoordinates.y() += inDelta.y();
    inT.m_WorldCoordinates.x() += inDelta.x();
    inT.m_WorldCoordinates.y() += inDelta.y();
    if ( inDelta.x() != 0.0f ) inV.m_DX = 0.0f;
    if ( inDelta.y() != 0.0f ) inV.m_DY = 0.0f;
}

// The entity that actually moves when inShape's collider is pushed: inShape
// itself, or its nearest ancestor with a Rigidbody and a Velocity -- so a
// compound body (a player with several child colliders) is pushed as a whole.
// With no such ancestor it's inShape itself (static geometry, or a lone body).
Entity BodyOf( Registry& inRegistry, Entity inShape ) noexcept
{
    for ( Entity current = inShape; current != Entity::Null(); )
    {
        if ( inRegistry.GetComponent<Rigidbody>( current ) && inRegistry.GetComponent<Velocity>( current ) )
            return current;

        auto hierarchy = inRegistry.GetComponent<asge::ecs::components::Hierarchy>( current );
        if ( !hierarchy ) break;
        current = hierarchy.Value().get().m_Parent;
    }

    return inShape;
}

// Moves every descendant's world position by inDelta, so a pushed body's child
// colliders and sprites follow it now rather than at the next propagation.
void ShiftDescendants( Registry& inRegistry, Entity inRoot, Float2 const& inDelta ) noexcept
{
    asge::ecs::components::ForEachChild( inRegistry, inRoot, [&]( Entity inChild )
    {
        if ( auto t = inRegistry.GetComponent<Transform>( inChild ) )
        {
            t.Value().get().m_WorldCoordinates.x() += inDelta.x();
            t.Value().get().m_WorldCoordinates.y() += inDelta.y();
        }
        ShiftDescendants( inRegistry, inChild, inDelta );
    } );
}

// The part of a correction inWanted along one axis not already applied
// (inApplied) to the same body this pass: two shapes of one body touching the
// same wall must push it out once, not twice.
float RemainingCorrection( float inApplied, float inWanted ) noexcept
{
    if ( inWanted * inApplied <= 0.0f ) return inWanted; // nothing applied yet, or the other way
    return std::abs( inWanted ) > std::abs( inApplied ) ? inWanted - inApplied : 0.0f;
}

/**
 * @brief Pushes e1/e2 apart along mtv, splitting the correction by mass
 *        ratio when both are movable — the actual push-out logic for a
 *        Solid/Solid overlap. Only a body with both Velocity and
 *        Rigidbody moves; a pair with neither is left alone.
 */
void ResolveSolidCollision(
    Registry &inRegistry, Entity e1, Transform& t1, Entity e2, Transform& t2, 
    Float2 const& mtv, AppliedCorrections& ioApplied ) noexcept
{
    auto vel1 = inRegistry.GetComponent<Velocity>( e1 );
    auto vel2 = inRegistry.GetComponent<Velocity>( e2 );
    auto rb1  = inRegistry.GetComponent<Rigidbody>( e1 );
    auto rb2  = inRegistry.GetComponent<Rigidbody>( e2 );

    bool const movable1 = static_cast<bool>(rb1) && static_cast<bool>( vel1 );
    bool const movable2 = static_cast<bool>(rb2) && static_cast<bool>( vel2 );

    if ( !movable1 && !movable2 ) return;

    float share1 = 1.0f;
    float share2 = 1.0f;

    if ( movable1 && movable2 )
    {
        float const m1 = rb1.Value().get().m_Mass;
        float const m2 = rb2.Value().get().m_Mass;
        float const totalMass = m1 + m2;
        share1 = m2 / totalMass; // heavier body yields less
        share2 = m1 / totalMass;
    }
    
    // Applies inWanted to inBody net of what this pass already moved it by,
    // then drags its descendants along.
    auto const push = [&]( Entity inBody, Transform& inT, Velocity& inV, Float2 const& inWanted )
    {
        auto& applied = ioApplied[inBody];
        Float2 const delta{ RemainingCorrection( applied.x(), inWanted.x() ),
                            RemainingCorrection( applied.y(), inWanted.y() ) };
        applied = Float2{ applied.x() + delta.x(), applied.y() + delta.y() };

        ApplyCorrection( inT, inV, delta );
        ShiftDescendants( inRegistry, inBody, delta );
    };

    if ( movable1 ) push( e1, t1, vel1.Value().get(), { mtv.x() * share1, mtv.y() * share1 } );
    if ( movable2 ) push( e2, t2, vel2.Value().get(), { -mtv.x() * share2, -mtv.y() * share2 } );
}

Rect ToBroadPhaseRect( ColliderShape const& inShape ) noexcept
{
    return std::visit( []( auto const& s ) -> Rect
    {
        using S = std::decay_t<decltype(s)>;
        if constexpr ( std::is_same_v<S, Rect> ) return s;
        else return BoundingRect( s );
    }, inShape);
}

// Smallest rect enclosing every collider, or nullopt when there are none.
std::optional<Rect> ComputeColliderExtension( Registry const& inRegistry ) noexcept
{
    std::optional<Rect> extension;

    for ( auto [ e, t, c ] : inRegistry.View<Transform, Collider>() )
    {
        Rect const r = ToBroadPhaseRect( asge::game::systems::WorldBounds( t.get(), c.get() ) );
        extension = extension ? Merge( *extension, r ) : r;
    }

    return extension;
}

// A trigger overlap found during traversal; emitted only once the traversal is over.
struct TriggerContact
{
    Entity m_First;
    Entity m_Second;
    bool   m_Enter;
};

std::optional<Float2> PenetrationVector( Transform const& t1, Collider const& c1,
    Transform const& t2, Collider const& c2) noexcept 
{
    auto const obj1 = asge::game::systems::WorldBounds( t1, c1 );
    auto const obj2 = asge::game::systems::WorldBounds( t2, c2 );

    return std::visit( []( auto const& shape1, auto const& shape2 )
    {
        return asge::math::PenetrationVector( shape1, shape2 );
    }, obj1, obj2);
}

bool CheckCollision( 
    Registry& inRegistry, Entity inE1, Entity inE2, AppliedCorrections& inCorrections,
    asge::game::systems::PhysicsState& ioState, std::set<EntityPair>& ioPairs,
    std::vector<TriggerContact>& ioContacts ) noexcept
{
    auto const& c1 = inRegistry.GetComponent<Collider>( inE1 ).Value().get();
    auto const& c2 = inRegistry.GetComponent<Collider>( inE2 ).Value().get();
    auto&       t1 = inRegistry.GetComponent<Transform>( inE1 ).Value().get();
    auto&       t2 = inRegistry.GetComponent<Transform>( inE2 ).Value().get();
    auto const  b1 = BodyOf( inRegistry, inE1 );
    auto const  b2 = BodyOf( inRegistry, inE2 );

    if ( 
            !LayersCanCollide( c1, c2 ) || (b1 == b2) 
        ||  HasUnknownResolution( c1 )  || HasUnknownResolution( c2 ) )
    {
        return true;
    }

    auto mtv = PenetrationVector( t1, c1, t2, c2 );
    if ( !mtv )
    {
        return true;
    }

    bool const c1Trigger = c1.m_Resolution == ResolutionType::Trigger;
    bool const c2Trigger = c2.m_Resolution == ResolutionType::Trigger;

    if ( c1Trigger || c2Trigger )
    {
        auto key = MakeCanonicalPair( inE1, inE2 );
        ioPairs.insert( key );

        // A Trigger is always reported as the first entity, whatever order the
        // broad phase produced the pair in.
        bool const swap = c2Trigger && !c1Trigger;
        ioContacts.push_back( 
            { 
                swap ? inE2 : inE1, swap ? inE1 : inE2,
                !ioState.m_PreviousTriggerPairs.contains( key ) 
            } );
    }
    else
    {
        // A shape is pushed through its body (see BodyOf), not on its own.
        auto bt1 = inRegistry.GetComponent<Transform>( b1 );
        auto bt2 = inRegistry.GetComponent<Transform>( b2 );
        if ( bt1 && bt2 )
        {
            ResolveSolidCollision( 
                inRegistry, b1, bt1.Value().get(), b2, bt2.Value().get(), *mtv, inCorrections 
            );
        }
    }

    return true;
}
}

asge::game::components::ColliderShape asge::game::systems::WorldBounds(
    components::Transform const& inTransform, components::Collider const& inCollider ) noexcept
{
    return std::visit([&inTransform]( auto const& inShape ) -> components::ColliderShape
    {
        using ShapeT = std::decay_t<decltype(inShape)>;

        if constexpr ( std::is_same_v<ShapeT, math::Rect> )
        {
            return math::Rect{
                inTransform.m_WorldCoordinates.x() + inShape.m_X, inTransform.m_WorldCoordinates.y() + inShape.m_Y,
                inShape.m_Width, inShape.m_Height
            };
        } else {
            return math::Circle{
                math::Float2{
                    inTransform.m_WorldCoordinates.x() + inShape.m_Center.x(),
                    inTransform.m_WorldCoordinates.y() + inShape.m_Center.y()
                },
                inShape.m_Radius
            };
        }
    }, inCollider.m_LocalBounds);
}

void asge::game::systems::MovementSystem(ecs::Registry &inRegistry, float inDeltaTime) noexcept
{
    for ( auto [ entity, transform, velocity ]
            : inRegistry.View<components::Transform, components::Velocity>() )
    {
        auto& t = transform.get();
        t.m_LocalCoordinates.x() += velocity.get().m_DX * inDeltaTime;
        t.m_LocalCoordinates.y() += velocity.get().m_DY * inDeltaTime;
        t.m_Dirty = true;
    }
}

void asge::game::systems::CollisionSystem(ecs::Registry &inRegistry, PhysicsState& ioState) noexcept
{
    static asge::spatial::QuadTree<asge::ecs::Entity> quadTree( GetArenaSize(), kMaxEntities );

    AppliedCorrections corrections;
    std::set<ecs::EntityPair> triggerPairs;
    std::vector<TriggerContact> contacts;

    // Step 1: Build the collision QuadTree with entities that have both
    // Transform and collider component, using Worldbounds coordinates.
    // With no colliders there is nothing to build; the Exit pass below still runs.
    auto const extension = ComputeColliderExtension( inRegistry );
    if ( !extension ) return;
    
    auto const reset = quadTree.Reset( *extension );
    if ( !reset.IsOk() )
    {
        LOG_ERROR( "CollisionSystem: broad phase skipped, QuadTree reset failed" );
        reset.LogError();
        return;
    }
    
    std::size_t rejected = 0;
    for ( auto [ e, t, c ] : inRegistry.View<components::Transform, components::Collider>() )
    {
        if ( !quadTree.Insert( e, ToBroadPhaseRect( WorldBounds( t.get(), c.get() ) ) ).IsOk() )
            ++rejected;
    }

    if ( rejected > 0 || quadTree.ArenaExhausted() )
    {
        LOG_WARNING( "CollisionSystem: QuadTree degraded, ", rejected, 
            " collider(s) not inserted, arena exhausted: ", quadTree.ArenaExhausted() );
    }

    // Step 2: Runs the collision test by checking each pair
    quadTree.ForEachPair( [&]( auto e1, auto e2 ) {
        return CheckCollision( inRegistry, e1, e2, corrections, ioState, triggerPairs, contacts );
    } );

    // Step 3: Emit trigger events only now, so handlers can't touch the
    // registry while the tree is being traversed.
    for ( auto const& contact : contacts )
    {
        if ( contact.m_Enter )
            events::OnCollisionTriggerEnter().Emit( contact.m_First, contact.m_Second );
        else
            events::OnCollisionTriggerStay().Emit( contact.m_First, contact.m_Second );
    }

    // Anything overlapping last frame but absent from this frame's contact
    // list — either it stopped overlapping, or an entity was destroyed.
    for ( auto const& pair : ioState.m_PreviousTriggerPairs )
    {
        if ( !triggerPairs.contains( pair ) )
            events::OnCollisionTriggerExit().Emit( pair.m_First, pair.m_Second );
    }

    ioState.m_PreviousTriggerPairs = std::move( triggerPairs );
}

void asge::game::systems::GravitySystem(ecs::Registry &inRegistry, float inDeltaTime) noexcept
{
    for ( auto [e, v, r] : 
            inRegistry.View<components::Velocity, components::Rigidbody>() )
    {
        if ( !r.get().m_AffectedByGravity ) continue;
        v.get().m_DY += kGravity * inDeltaTime;
    }
}

void asge::game::systems::PhysicsUpdate(
    ecs::Registry &inRegistry, PhysicsState &inState, float inDeltaTime) noexcept
{
    GravitySystem( inRegistry, inDeltaTime );
    MovementSystem( inRegistry, inDeltaTime );
    PathFollowingSystem( inRegistry, inDeltaTime );
    TransformPropagationSystem( inRegistry );
    CollisionSystem( inRegistry, inState );
}

void asge::game::systems::PathFollowingSystem(ecs::Registry &inRegistry, float inDeltaTime) noexcept
{
    for ( auto [ e, t, p ] 
        : inRegistry.View<components::Transform, components::PathFollow>() )
    {
        auto& path = p.get();
        auto& transform = t.get();

        if ( path.m_Finished || !path.m_Path.HasSegments() ) continue;
        path.m_Traveled += path.m_Speed * inDeltaTime;
        const float length = path.m_Path.Length();
        if ( path.m_Traveled >= length )
        {
            // length > 0.0f guards fmod: a degenerate path (all waypoints
            // coincide) has nothing to wrap around, so treat it as finished
            // even when m_Loop is set, rather than fmod-by-zero producing NaN.
            if ( path.m_Loop && length > 0.0f ) path.m_Traveled = std::fmod( path.m_Traveled, length );
            else
            {
                path.m_Traveled = length;
                path.m_Finished = true;
            }
        }

        math::Float2 pos = path.m_Path.PointAtDistance( path.m_Traveled );
        transform.m_LocalCoordinates = pos;
        transform.m_Dirty = true;

        float time = path.m_Path.TimeAtDistance( path.m_Traveled );
        math::Float2 tangent = path.m_Path.TangentAt( time );

        // A degenerate segment (coincident control points) normalizes to a
        // NaN tangent -- leave m_LocalRotation as it was rather than corrupting it.
        if ( std::isfinite( tangent.x() ) && std::isfinite( tangent.y() ) )
        {
            transform.m_LocalRotation = std::atan2( tangent.y(), tangent.x() );
        }
    }
}
