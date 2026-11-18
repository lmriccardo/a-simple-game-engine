#include "Game.hpp"

#include <algorithm>
#include <cmath>

using asge::input::Keycode;
using asge::graphics::RGBA_Color;
using asge::math::Float2;
using asge::math::Rect;

namespace
{
constexpr float WIDTH = 800.0f;
constexpr float HEIGHT = 600.0f;
constexpr float PLAYER_SIZE = 28.0f;
constexpr float PLAYER_SPEED = 260.0f;
constexpr float HAZARD_SIZE = 34.0f;
constexpr float ORB_RADIUS = 14.0f;
constexpr float FADE_SPEED = 1.5f;       // overlay amount per second
constexpr float GAME_OVER_DIM = 0.8f;    // how dark the screen gets on game over
constexpr float FLASH_TIME = 0.35f;
constexpr float INVULNERABLE_TIME = 1.2f;
constexpr int   GLOW_SIZE = 64;          // orb texture side, also the glow layer scale

constexpr RGBA_Color PLAYER_COLOR{ 90, 200, 255, 255 };
constexpr RGBA_Color HIT_COLOR{ 255, 60, 60, 255 };
constexpr RGBA_Color HAZARD_COLOR{ 200, 50, 70, 255 };
constexpr RGBA_Color ORB_COLOR{ 255, 190, 60, 255 };

// White disc whose alpha falls off toward the edge, so tinting it gives a soft light.
asge::media::Image MakeOrbImage()
{
    std::vector<std::uint8_t> pixels(GLOW_SIZE * GLOW_SIZE * 4);
    float const half = GLOW_SIZE / 2.0f;
    for (int y = 0; y < GLOW_SIZE; ++y)
    {
        for (int x = 0; x < GLOW_SIZE; ++x)
        {
            float const d = std::hypot(x + 0.5f - half, y + 0.5f - half) / half;
            float const a = std::clamp(1.0f - d, 0.0f, 1.0f);
            auto* p = &pixels[(y * GLOW_SIZE + x) * 4];
            p[0] = p[1] = p[2] = 255;
            p[3] = static_cast<std::uint8_t>(255.0f * a * a);
        }
    }
    return asge::media::Image(GLOW_SIZE, GLOW_SIZE, asge::graphics::PixelFormat::RGBA8, pixels);
}

asge::media::Image MakeBlockImage()
{
    return asge::media::Image(4, 4, asge::graphics::PixelFormat::RGBA8,
        std::vector<std::uint8_t>(4 * 4 * 4, 255));
}

bool Overlaps(Rect const& inA, Rect const& inB)
{
    return inA.m_X < inB.m_X + inB.m_Width && inB.m_X < inA.m_X + inA.m_Width
        && inA.m_Y < inB.m_Y + inB.m_Height && inB.m_Y < inA.m_Y + inA.m_Height;
}

float MoveToward(float inValue, float inTarget, float inStep)
{
    if (std::abs(inTarget - inValue) <= inStep) return inTarget;
    return inValue + (inTarget > inValue ? inStep : -inStep);
}
}

CompositingDemoState::CompositingDemoState()
{
    Restart();
}

void CompositingDemoState::Restart()
{
    m_Player = { WIDTH / 2.0f - PLAYER_SIZE / 2.0f, HEIGHT / 2.0f - PLAYER_SIZE / 2.0f };
    m_Score = 0;
    m_Lives = 3;
    m_HitFlash = 0.0f;
    m_Invulnerable = 0.0f;

    std::uniform_real_distribution<float> phase(0.0f, 6.28f);
    for (auto& orb : m_Orbs)
    {
        RespawnOrb(orb);
        orb.m_Phase = phase(m_Rng);
    }

    constexpr std::array<Float2, 3> starts{ Float2{ 80.0f, 80.0f }, Float2{ 650.0f, 120.0f }, Float2{ 400.0f, 500.0f } };
    constexpr std::array<Float2, 3> speeds{ Float2{ 150.0f, 110.0f }, Float2{ -130.0f, 150.0f }, Float2{ 170.0f, -120.0f } };
    for (std::size_t i = 0; i < m_Hazards.size(); ++i)
    {
        m_Hazards[i] = { starts[i], speeds[i] };
    }

    m_Fade = 1.0f;       // start black and fade in
    m_FadeTarget = 0.0f;
}

void CompositingDemoState::RespawnOrb(Orb& inOrb)
{
    std::uniform_real_distribution<float> x(40.0f, WIDTH - 40.0f);
    std::uniform_real_distribution<float> y(40.0f, HEIGHT - 40.0f);
    inOrb.m_Pos = { x(m_Rng), y(m_Rng) };
}

void CompositingDemoState::EnsureResources(asge::video::IRenderer& inRenderer)
{
    if (!m_OrbTexture)   m_OrbTexture   = inRenderer.CreateTexture(MakeOrbImage());
    if (!m_BlockTexture) m_BlockTexture = inRenderer.CreateTexture(MakeBlockImage());
    if (!m_GlowLayer)    m_GlowLayer    = inRenderer.CreateRenderTarget({ static_cast<int>(WIDTH), static_cast<int>(HEIGHT) });
}

std::optional<asge::game::state::Transition<int>>
CompositingDemoState::Update(float inDeltaTime, asge::input::InputState const& inInput)
{
    m_Time += inDeltaTime;
    m_Fade = MoveToward(m_Fade, m_FadeTarget, FADE_SPEED * inDeltaTime);
    m_HitFlash = std::max(0.0f, m_HitFlash - inDeltaTime);
    m_Invulnerable = std::max(0.0f, m_Invulnerable - inDeltaTime);

    if (m_Lives <= 0)
    {
        if (inInput.IsKeyPressed(Keycode::R)) Restart();
        return std::nullopt;
    }

    float const dx = (inInput.IsKeyDown(Keycode::D) ? 1.0f : 0.0f) - (inInput.IsKeyDown(Keycode::A) ? 1.0f : 0.0f);
    float const dy = (inInput.IsKeyDown(Keycode::S) ? 1.0f : 0.0f) - (inInput.IsKeyDown(Keycode::W) ? 1.0f : 0.0f);
    m_Player = {
        std::clamp(m_Player.x() + dx * PLAYER_SPEED * inDeltaTime, 0.0f, WIDTH - PLAYER_SIZE),
        std::clamp(m_Player.y() + dy * PLAYER_SPEED * inDeltaTime, 0.0f, HEIGHT - PLAYER_SIZE)
    };
    Rect const playerRect{ m_Player.x(), m_Player.y(), PLAYER_SIZE, PLAYER_SIZE };

    for (auto& orb : m_Orbs)
    {
        Rect const orbRect{ orb.m_Pos.x() - ORB_RADIUS, orb.m_Pos.y() - ORB_RADIUS, ORB_RADIUS * 2.0f, ORB_RADIUS * 2.0f };
        if (Overlaps(playerRect, orbRect))
        {
            ++m_Score;
            RespawnOrb(orb);
        }
    }

    for (auto& hazard : m_Hazards)
    {
        Float2 pos = hazard.m_Pos + hazard.m_Speed * inDeltaTime;
        if (pos.x() < 0.0f || pos.x() > WIDTH - HAZARD_SIZE)  hazard.m_Speed = { -hazard.m_Speed.x(), hazard.m_Speed.y() };
        if (pos.y() < 0.0f || pos.y() > HEIGHT - HAZARD_SIZE) hazard.m_Speed = { hazard.m_Speed.x(), -hazard.m_Speed.y() };
        hazard.m_Pos = { std::clamp(pos.x(), 0.0f, WIDTH - HAZARD_SIZE), std::clamp(pos.y(), 0.0f, HEIGHT - HAZARD_SIZE) };

        if (m_Invulnerable <= 0.0f && Overlaps(playerRect, { hazard.m_Pos.x(), hazard.m_Pos.y(), HAZARD_SIZE, HAZARD_SIZE }))
        {
            --m_Lives;
            m_HitFlash = FLASH_TIME;
            m_Invulnerable = INVULNERABLE_TIME;
            if (m_Lives <= 0) m_FadeTarget = GAME_OVER_DIM; // fade out toward game over
        }
    }

    return std::nullopt;
}

void CompositingDemoState::Render(asge::video::IRenderer& inRenderer)
{
    using namespace asge::video;

    EnsureResources(inRenderer);
    if (!m_OrbTexture || !m_BlockTexture || !m_GlowLayer) return;

    inRenderer.Clear({ 12, 12, 24, 255 });

    // 1. Glow layer: every orb's halo is drawn into one offscreen texture ...
    {
        RenderTargetScope const scope(inRenderer, *m_GlowLayer);
        inRenderer.Clear({ 0, 0, 0, 0 });
        for (auto const& orb : m_Orbs)
        {
            float const pulse = 0.5f + 0.5f * std::sin(m_Time * 3.0f + orb.m_Phase);
            float const size = 70.0f + 40.0f * pulse;
            inRenderer.DrawTextureTinted(*m_OrbTexture,
                { orb.m_Pos.x() - size / 2.0f, orb.m_Pos.y() - size / 2.0f, size, size }, ORB_COLOR);
        }
    }
    // ... then composited additively in a single call, so it brightens the scene.
    inRenderer.DrawTextureGlow(*m_GlowLayer, { 0.0f, 0.0f, WIDTH, HEIGHT }, 0.9f);

    for (auto const& orb : m_Orbs)
    {
        inRenderer.DrawCircle({ static_cast<int>(orb.m_Pos.x()), static_cast<int>(orb.m_Pos.y()) }, 6, { 255, 240, 200, 255 }, true);
    }

    // 2. Hazards: a white texture tinted red.
    for (auto const& hazard : m_Hazards)
    {
        inRenderer.DrawTextureTinted(*m_BlockTexture, { hazard.m_Pos.x(), hazard.m_Pos.y(), HAZARD_SIZE, HAZARD_SIZE }, HAZARD_COLOR);
    }

    // 3. Player: flashes red when hit, then stays see-through while invulnerable.
    if (m_Lives > 0)
    {
        RGBA_Color tint = m_HitFlash > 0.0f ? HIT_COLOR : PLAYER_COLOR;
        if (m_Invulnerable > 0.0f) tint.a = 110;
        inRenderer.DrawTextureTinted(*m_BlockTexture, { m_Player.x(), m_Player.y(), PLAYER_SIZE, PLAYER_SIZE }, tint);
    }

    // 4. HUD: gold pips for the score, red pips for the lives.
    for (int i = 0; i < std::min(m_Score, 40); ++i)
    {
        inRenderer.DrawRect({ 12.0f + static_cast<float>(i) * 10.0f, 12.0f, 8.0f, 8.0f }, ORB_COLOR, true);
    }
    for (int i = 0; i < m_Lives; ++i)
    {
        inRenderer.DrawRect({ WIDTH - 24.0f - static_cast<float>(i) * 22.0f, 12.0f, 16.0f, 16.0f }, HIT_COLOR, true);
    }

    // 5. Screen fade: black at start (fades in), dims on game over (R to restart).
    inRenderer.DrawFade({ 0, 0, 0, 255 }, m_Fade);
}

void CompositingDemoState::OnSystemEvent([[maybe_unused]] asge::event::SystemEvent const& inSysEvent)
{
}

CompositingDemoGame::CompositingDemoGame(asge::video::IRenderer& inRenderer, asge::audio::AudioDevice& inAudioDev)
: Game(inRenderer, inAudioDev)
{
    SetInitialState(0);
}

std::unique_ptr<CompositingDemoGame::StateType> CompositingDemoGame::CreateState([[maybe_unused]] int inId)
{
    return std::make_unique<CompositingDemoState>();
}
