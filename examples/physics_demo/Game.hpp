#pragma once

/**
 * @brief Showcase for Rigidbody, GravitySystem, mass-weighted
 *        CollisionResolution, and Trigger colliders.
 *
 * A floor and two side walls are static Solid Colliders (Transform +
 * Collider, deliberately no Velocity or Rigidbody, so PhysicsSystem treats
 * them as immovable). A yellow-outlined zone near the bottom-right is a
 * static Trigger Collider instead -- boxes fall/settle against the
 * Solid geometry as normal, but pass straight through the trigger zone
 * without being pushed; overlapping it despawns the box, via
 * events::OnTriggerOverlap rather than a Collider push-out.
 *
 * Left-click drops a new box -- Transform + Velocity + Collider + Rigidbody
 * with a randomized mass -- at the cursor; every frame runs GravitySystem,
 * then MovementSystem, then CollisionResolution, so boxes fall, land on the
 * floor/each other/the trigger zone, and settle (or get despawned). R
 * resets the scene.
 */

#include <ASGE/ASGE.hpp>
#include <random>
#include <vector>

class PhysicsDemoGame : public asge::game::IGame
{
    asge::ecs::Registry            m_Registry;
    std::vector<asge::ecs::Entity> m_Boxes; // dynamic entities only -- static geometry is never touched by Reset()
    std::mt19937                   m_Rng{ std::random_device{}() };

    asge::ecs::Entity m_TriggerZone{ asge::ecs::Entity::Null() };
    std::vector<asge::ecs::Entity> m_ConsumedByTrigger; // boxes to destroy once CollisionResolution returns -- see HandleTriggerOverlap
    asge::signals::Connection<asge::ecs::Entity, asge::ecs::Entity> m_TriggerConnection;

    void SpawnStaticGeometry();
    void SpawnTriggerZone();
    void SpawnBox( asge::math::Float2 inCenter );
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
