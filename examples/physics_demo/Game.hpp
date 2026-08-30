#pragma once

/**
 * @brief Showcase for Rigidbody, GravitySystem, and mass-weighted CollisionResolution.
 *
 * A floor and two side walls are static Colliders (Transform + Collider,
 * deliberately no Velocity or Rigidbody, so PhysicsSystem treats them as
 * immovable). Left-click drops a new box -- Transform + Velocity + Collider
 * + Rigidbody with a randomized mass -- at the cursor; every frame runs
 * GravitySystem, then MovementSystem, then CollisionResolution, so boxes
 * fall, land on the floor/each other, and settle. R resets the scene.
 */

#include <ASGE/ASGE.hpp>
#include <random>
#include <vector>

class PhysicsDemoGame : public asge::game::IGame
{
    asge::ecs::Registry            m_Registry;
    std::vector<asge::ecs::Entity> m_Boxes; // dynamic entities only -- static geometry is never touched by Reset()
    std::mt19937                   m_Rng{ std::random_device{}() };

    void SpawnStaticGeometry();
    void SpawnBox( asge::math::Float2 inCenter );
    void SpawnInitialStack();
    void Reset();
    void DespawnFallenBoxes();
    void HandleInput( asge::input::InputState const& inInput );

public:
    PhysicsDemoGame();
    ~PhysicsDemoGame() override = default;

    void Update(float inDeltaTime, asge::input::InputState const& inInput) override;
    void Render(asge::video::IRenderer& inRenderer) override;
    void OnSystemEvent(asge::event::SystemEvent const& inSysEvent) override;
};
