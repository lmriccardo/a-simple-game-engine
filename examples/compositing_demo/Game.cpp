#include "Game.hpp"

#include <algorithm>
#include <cmath>

namespace
{
using asge::game::components::Sprite;
using asge::game::components::Transform;
using asge::game::components::Velocity;
using asge::game::resources::ScreenFade;
using asge::graphics::RGBA_Color;

constexpr char const* kScenePath = "assets/scene.toml";
constexpr float WIDTH = 800.0f;
constexpr float HEIGHT = 600.0f;
constexpr float PLAYER_SIZE = 28.0f;
constexpr float PLAYER_SPEED = 260.0f;
constexpr float HAZARD_SIZE = 34.0f;
constexpr float ORB_HALF = 44.8f;       // half the orb sprite (64px * 1.4 scale / 2)
constexpr float ORB_REACH = 16.0f;      // how close the player must get to an orb's center
constexpr float FLASH_TIME = 0.35f;
constexpr float INVULNERABLE_TIME = 1.2f;
constexpr float GAME_OVER_DIM = 0.8f;   // how dark the screen gets on game over

constexpr RGBA_Color PLAYER_COLOR{ 90, 200, 255, 255 };
constexpr RGBA_Color HIT_COLOR{ 255, 60, 60, 255 };

bool Overlaps(float inAx, float inAy, float inAsize, float inBx, float inBy, float inBsize)
{
    return inAx < inBx + inBsize && inBx < inAx + inAsize
        && inAy < inBy + inBsize && inBy < inAy + inAsize;
}

void MoveTo(Transform& ioTransform, float inX, float inY)
{
    ioTransform.m_LocalCoordinates = { inX, inY };
    ioTransform.m_Dirty = true;
}
}

CompositingDemoState::CompositingDemoState(asge::game::scene::SceneManager& inSceneManager)
: m_SceneManager(inSceneManager)
{
    FindEntities();
    StartFadeIn();
}

void CompositingDemoState::FindEntities()
{
    // Roles are positional, see assets/scene.toml
    auto const entities = m_SceneManager.ActiveEntities();
    if (entities.size() != kFloors + kOrbs + kHazards + kLives + 1)
    {
        LOG_ERROR("compositing_demo: ", kScenePath, " should hold ", kFloors + kOrbs + kHazards + kLives + 1, " entities");
        return;
    }

    std::size_t next = kFloors; // the floor strips are never touched
    for (auto& orb : m_Orbs)       orb = entities[next++];
    for (auto& hazard : m_Hazards) hazard = entities[next++];
    for (auto& pip : m_Pips)       pip = entities[next++];
    m_Player = entities[next];
}

void CompositingDemoState::StartFadeIn()
{
    auto& registry = m_SceneManager.GetRegistry();
    ScreenFade fade;
    fade.Set(1.0f);
    fade.FadeTo(0.0f, 0.8f, asge::math::Easing::OutQuad);
    registry.SetResource(fade);
}

void CompositingDemoState::Restart()
{
    // Reloading the scene puts every entity back where scene.toml placed it
    m_SceneManager.RequestLoad(kScenePath);
    if (auto result = m_SceneManager.ApplyPendingTransition(); !result)
    {
        result.LogError();
        return;
    }

    FindEntities();
    StartFadeIn();
    m_Score = 0;
    m_Lives = kLives;
    m_HitFlash = 0.0f;
    m_Invulnerable = 0.0f;
}

std::optional<asge::game::state::Transition<int>>
CompositingDemoState::Update(float inDeltaTime, asge::input::InputState const& inInput)
{
    using asge::input::Keycode;

    if (m_Player == asge::ecs::Entity::Null()) return std::nullopt;

    auto& registry = m_SceneManager.GetRegistry();
    m_Time += inDeltaTime;
    m_HitFlash = std::max(0.0f, m_HitFlash - inDeltaTime);
    m_Invulnerable = std::max(0.0f, m_Invulnerable - inDeltaTime);

    // Each orb pulses by breathing its tint alpha, which is the strength of the additive glow
    for (std::size_t i = 0; i < m_Orbs.size(); ++i)
    {
        if (auto sprite = registry.GetComponent<Sprite>(m_Orbs[i]))
        {
            float const pulse = 0.5f + 0.5f * std::sin(m_Time * 3.0f + static_cast<float>(i) * 1.3f);
            sprite.Value().get().m_Tint.a = static_cast<std::uint8_t>(120.0f + 135.0f * pulse);
        }
    }

    if (m_Lives <= 0)
    {
        if (inInput.IsKeyPressed(Keycode::R)) Restart();
        return std::nullopt;
    }

    // Player: WASD sets the Velocity (integrated by the Game pipeline); clamp to the window
    auto playerVelocity = registry.GetComponent<Velocity>(m_Player);
    auto playerTransform = registry.GetComponent<Transform>(m_Player);
    auto playerSprite = registry.GetComponent<Sprite>(m_Player);
    if (!playerVelocity || !playerTransform || !playerSprite) return std::nullopt;

    auto& velocity = playerVelocity.Value().get();
    auto& transform = playerTransform.Value().get();
    velocity.m_DX = (inInput.IsKeyDown(Keycode::D) ? PLAYER_SPEED : 0.0f) - (inInput.IsKeyDown(Keycode::A) ? PLAYER_SPEED : 0.0f);
    velocity.m_DY = (inInput.IsKeyDown(Keycode::S) ? PLAYER_SPEED : 0.0f) - (inInput.IsKeyDown(Keycode::W) ? PLAYER_SPEED : 0.0f);

    float const px = std::clamp(transform.m_LocalCoordinates.x(), 0.0f, WIDTH - PLAYER_SIZE);
    float const py = std::clamp(transform.m_LocalCoordinates.y(), 0.0f, HEIGHT - PLAYER_SIZE);
    if (px != transform.m_LocalCoordinates.x() || py != transform.m_LocalCoordinates.y()) MoveTo(transform, px, py);

    // Tint flash on a hit, then see-through while invulnerable
    RGBA_Color tint = m_HitFlash > 0.0f ? HIT_COLOR : PLAYER_COLOR;
    if (m_Invulnerable > 0.0f) tint.a = 110;
    playerSprite.Value().get().m_Tint = tint;

    // Orbs: collect and respawn somewhere else
    for (auto const orb : m_Orbs)
    {
        auto orbTransform = registry.GetComponent<Transform>(orb);
        if (!orbTransform) continue;

        auto& t = orbTransform.Value().get();
        float const cx = t.m_LocalCoordinates.x() + ORB_HALF;
        float const cy = t.m_LocalCoordinates.y() + ORB_HALF;
        if (std::hypot(px + PLAYER_SIZE / 2.0f - cx, py + PLAYER_SIZE / 2.0f - cy) < ORB_REACH + PLAYER_SIZE / 2.0f)
        {
            ++m_Score;
            LOG_INFO("Orbs collected: ", m_Score);
            std::uniform_real_distribution<float> x(40.0f, WIDTH - 40.0f);
            std::uniform_real_distribution<float> y(40.0f, HEIGHT - 40.0f);
            MoveTo(t, x(m_Rng) - ORB_HALF, y(m_Rng) - ORB_HALF);
        }
    }

    // Hazards: bounce off the window edges, and hurt on contact
    for (auto const hazard : m_Hazards)
    {
        auto hazardTransform = registry.GetComponent<Transform>(hazard);
        auto hazardVelocity = registry.GetComponent<Velocity>(hazard);
        if (!hazardTransform || !hazardVelocity) continue;

        auto& t = hazardTransform.Value().get();
        auto& v = hazardVelocity.Value().get();
        float const hx = t.m_LocalCoordinates.x();
        float const hy = t.m_LocalCoordinates.y();
        if (hx < 0.0f || hx > WIDTH - HAZARD_SIZE) v.m_DX = -v.m_DX;
        if (hy < 0.0f || hy > HEIGHT - HAZARD_SIZE) v.m_DY = -v.m_DY;
        MoveTo(t, std::clamp(hx, 0.0f, WIDTH - HAZARD_SIZE), std::clamp(hy, 0.0f, HEIGHT - HAZARD_SIZE));

        if (m_Invulnerable <= 0.0f && Overlaps(px, py, PLAYER_SIZE, hx, hy, HAZARD_SIZE))
        {
            --m_Lives;
            m_HitFlash = FLASH_TIME;
            m_Invulnerable = INVULNERABLE_TIME;

            // Dim the pip that was just lost
            if (auto pip = registry.GetComponent<Sprite>(m_Pips[static_cast<std::size_t>(m_Lives)]))
            {
                pip.Value().get().m_Tint.a = 40;
            }

            if (m_Lives <= 0)
            {
                LOG_INFO("Game over with ", m_Score, " orbs. Press R to restart.");
                if (auto fade = registry.GetResource<ScreenFade>())
                {
                    fade.Value().get().FadeTo(GAME_OVER_DIM, 0.8f, asge::math::Easing::OutQuad);
                }
            }
        }
    }

    return std::nullopt;
}

void CompositingDemoState::Render([[maybe_unused]] asge::video::IRenderer& inRenderer)
{
    // Nothing to draw: Game<> already rendered the scene sprites and the ScreenFade overlay.
}

void CompositingDemoState::OnSystemEvent([[maybe_unused]] asge::event::SystemEvent const& inSysEvent)
{
}

CompositingDemoGame::CompositingDemoGame(asge::video::IRenderer& inRenderer, asge::audio::AudioDevice& inAudioDev)
: Game(inRenderer, inAudioDev)
{
    // ASGE_COMPOSITING_DEMO_ASSET_DIR is injected by CMakeLists.txt
    if (auto mount = m_Vfs.Mount("assets", ASGE_COMPOSITING_DEMO_ASSET_DIR); !mount) mount.LogError();
    if (auto load = m_SceneManager.LoadScene(kScenePath); !load) load.LogError();

    SetInitialState(0);
}

std::unique_ptr<CompositingDemoGame::StateType> CompositingDemoGame::CreateState([[maybe_unused]] int inId)
{
    return std::make_unique<CompositingDemoState>(m_SceneManager);
}
