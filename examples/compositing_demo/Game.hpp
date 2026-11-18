#pragma once

#include <ASGE/ASGE.hpp>

#include <array>
#include <random>

// Orb Rush: WASD to collect the glowing orbs, avoid the red blocks, R restarts.
// Shows the compositing helpers in a tiny game: a pulsing additive glow on a
// render-target layer, a tint flash when hit, and fade in / out of the screen.
class CompositingDemoState final : public asge::game::state::IGameState<int>
{
    struct Orb
    {
        asge::math::Float2 m_Pos{};  // Center in pixels
        float              m_Phase{}; // Offsets the pulse so orbs do not blink together
    };

    struct Hazard
    {
        asge::math::Float2 m_Pos{};   // Top-left in pixels
        asge::math::Float2 m_Speed{}; // Pixels per second
    };

    std::unique_ptr<asge::video::ITexture> m_OrbTexture;    // Soft white radial gradient
    std::unique_ptr<asge::video::ITexture> m_BlockTexture;  // Solid white square, tinted when drawn
    std::unique_ptr<asge::video::ITexture> m_GlowLayer;     // Offscreen layer all orb glows are drawn into

    std::mt19937 m_Rng{ 1234 };            // Fixed seed so runs are repeatable
    std::array<Orb, 5> m_Orbs{};           // Orbs on screen
    std::array<Hazard, 3> m_Hazards{};     // Bouncing red blocks
    asge::math::Float2 m_Player{};         // Top-left of the player
    float m_Time{0.0f};                    // Seconds since start, drives the pulse
    float m_HitFlash{0.0f};                // Seconds left of the red hit flash
    float m_Invulnerable{0.0f};            // Seconds left of damage immunity
    float m_Fade{1.0f};                    // Current black overlay amount
    float m_FadeTarget{0.0f};              // Amount the overlay moves toward
    int   m_Score{0};                      // Orbs collected
    int   m_Lives{3};                      // Hits left

    void EnsureResources(asge::video::IRenderer& inRenderer);
    void Restart();
    void RespawnOrb(Orb& inOrb);

public:
    CompositingDemoState();

    [[nodiscard]] std::optional<asge::game::state::Transition<int>>
    Update(float inDeltaTime, asge::input::InputState const& inInput) override;
    void Render(asge::video::IRenderer& inRenderer) override;
    void OnSystemEvent(asge::event::SystemEvent const& inSysEvent) override;
};

class CompositingDemoGame final : public asge::game::Game<int>
{
public:
    explicit CompositingDemoGame(asge::video::IRenderer& inRenderer, asge::audio::AudioDevice& inAudioDev);

protected:
    [[nodiscard]] std::unique_ptr<StateType> CreateState(int inId) override;
};
