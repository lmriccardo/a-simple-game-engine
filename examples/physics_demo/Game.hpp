#pragma once

/**
 * @brief Showcase for Rigidbody, gravity, mass-weighted collision response,
 *        and Trigger colliders, all driven through systems::PhysicsUpdate.
 *
 * A floor and two side walls are static Solid Colliders (Transform +
 * Collider, deliberately no Velocity or Rigidbody, so collision response
 * treats them as immovable). A yellow-outlined zone near the bottom-right
 * is a static Trigger Collider instead -- boxes fall/settle against the
 * Solid geometry as normal, but pass straight through the trigger zone
 * without being pushed; overlapping it despawns the box, via
 * events::OnCollisionTriggerEnter rather than a Collider push-out.
 *
 * Left-click drops a new box -- Transform + Velocity + Collider + Rigidbody
 * with a randomized mass -- at the cursor; every frame runs
 * systems::PhysicsUpdate (gravity, movement, collision detection/response,
 * trigger events, in that order), so boxes fall, land on the floor/each
 * other/the trigger zone, and settle (or get despawned). R resets the scene.
 *
 * Two extra "ghost" boxes near the left wall showcase Collider::m_Layer/
 * m_Mask: dropped at the same X, on two layers whose masks exclude each
 * other. Both still land on the floor/walls normally (m_Mask still includes
 * the default layer everything else uses), but the second one falls
 * straight through the first instead of stacking on it -- see SpawnBox's
 * inLayer/inMask parameters and components::details::LayersCanCollide.
 */

#include <ASGE/ASGE.hpp>
#include <random>
#include <vector>

class PhysicsDemoGame : public asge::game::IGame
{
    asge::ecs::Registry            m_Registry;
    std::vector<asge::ecs::Entity> m_Boxes; // dynamic entities only -- static geometry is never touched by Reset()
    std::mt19937                   m_Rng{ std::random_device{}() };
    asge::game::systems::PhysicsState m_PhysicsState; // enter/exit bookkeeping for trigger pairs -- see PhysicsUpdate

    asge::ecs::Entity m_TriggerZone{ asge::ecs::Entity::Null() };
    std::vector<asge::ecs::Entity> m_ConsumedByTrigger; // boxes to destroy once PhysicsUpdate returns -- see HandleTriggerOverlap
    asge::signals::Connection<asge::ecs::Entity, asge::ecs::Entity> m_TriggerConnection;

    void SpawnStaticGeometry();
    void SpawnTriggerZone();
    void SpawnBox( asge::math::Float2 inCenter,
        asge::game::components::CollisionLayer inLayer = 1u,
        asge::game::components::CollisionLayer inMask = ~asge::game::components::CollisionLayer{0},
        float inSize = 0.0f );
    void SpawnInitialStack();
    void Reset();
    void DestroyBox( asge::ecs::Entity inEntity );
    void DespawnFallenBoxes();
    void HandleTriggerOverlap( asge::ecs::Entity inA, asge::ecs::Entity inB );
    void ProcessTriggerDespawns();
    void HandleInput( asge::input::InputState const& inInput );

public:
    PhysicsDemoGame();
    ~PhysicsDemoGame() override = default;

    void Update(float inDeltaTime, asge::input::InputState const& inInput) override;
    void Render(asge::video::IRenderer& inRenderer) override;
    void OnSystemEvent(asge::event::SystemEvent const& inSysEvent) override;
};
