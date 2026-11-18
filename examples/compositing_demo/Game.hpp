#pragma once

#include <ASGE/ASGE.hpp>

#include <array>
#include <random>

// Orb Rush: WASD to collect the glowing orbs, avoid the red blocks, R restarts.
// The world is assets/scene.toml: the orbs are additive sprites (the glow), and
// the screen fade is the ScreenFade resource. This state only runs the game
// rules; Game<> draws the scene and the fade through RenderPipeline.
class CompositingDemoState final : public asge::game::state::IGameState<int>
{
    static constexpr std::size_t kFloors = 2;  // Floor strips, first in the scene so they draw behind
    static constexpr std::size_t kOrbs = 6;    // Orb entities, after the floors
    static constexpr std::size_t kHazards = 3; // Hazard entities, after the orbs
    static constexpr std::size_t kLives = 3;   // Life pip entities, after the hazards

    asge::game::scene::SceneManager& m_SceneManager;

    std::array<asge::ecs::Entity, kOrbs>    m_Orbs{};    // Glowing pickups
    std::array<asge::ecs::Entity, kHazards> m_Hazards{}; // Bouncing red blocks
    std::array<asge::ecs::Entity, kLives>   m_Pips{};    // Life indicators
    asge::ecs::Entity m_Player{ asge::ecs::Entity::Null() }; // The scene's last entity

    std::mt19937 m_Rng{ 1234 }; // Fixed seed so runs are repeatable
    float m_Time{0.0f};         // Seconds since start, drives the pulse
    float m_HitFlash{0.0f};     // Seconds left of the red hit flash
    float m_Invulnerable{0.0f}; // Seconds left of damage immunity
    int   m_Score{0};           // Orbs collected
    int   m_Lives{kLives};      // Hits left

    void FindEntities();
    void Restart();
    void StartFadeIn();

public:
    explicit CompositingDemoState(asge::game::scene::SceneManager& inSceneManager);

    [[nodiscard]] std::optional<asge::game::state::Transition<int>>
    Update(float inDeltaTime, asge::input::InputState const& inInput) override;
    void Render(asge::video::IRenderer& inRenderer) override;
    [[nodiscard]] asge::graphics::RGBA_Color ClearColor() const noexcept override { return { 12, 12, 24, 255 }; }
    void OnSystemEvent(asge::event::SystemEvent const& inSysEvent) override;
};

class CompositingDemoGame final : public asge::game::Game<int>
{
public:
    explicit CompositingDemoGame(asge::video::IRenderer& inRenderer, asge::audio::AudioDevice& inAudioDev);

protected:
    [[nodiscard]] std::unique_ptr<StateType> CreateState(int inId) override;
};
