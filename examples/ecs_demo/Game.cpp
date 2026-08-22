#include "Game.hpp"

#include <algorithm>
#include <filesystem>

namespace
{
using asge::game::components::Sprite;
using asge::game::components::Transform;
using asge::game::components::Velocity;

constexpr float kWindowWidth  = 800.0f;
constexpr float kWindowHeight = 600.0f;
constexpr float kPlayerSpeed  = 220.0f;

// A handful of hardcoded (position, velocity) pairs — enough to show
// several entities moving independently through the same MovementSystem,
// without pulling in <random> for a demo.
struct Drifter { float x, y, dx, dy; };
constexpr Drifter kDrifters[] = {
    { 100.0f, 100.0f,  60.0f,  40.0f },
    { 300.0f, 150.0f, -50.0f,  70.0f },
    { 500.0f,  80.0f,  30.0f, -60.0f },
    { 650.0f, 400.0f, -70.0f, -30.0f },
    { 200.0f, 450.0f,  90.0f,  20.0f },
    { 700.0f, 200.0f, -40.0f,  50.0f },
};
}

EcsDemoGame::EcsDemoGame()
{
    SpawnEntities();
}

void EcsDemoGame::SpawnEntities()
{
    for ( auto const& d : kDrifters )
    {
        auto entity = m_Registry.CreateEntity();
        if ( !entity ) { entity.LogError(); continue; }

        m_Registry.AddComponent<Transform>( entity.Value(), Transform{ d.x, d.y, 0.0f, 0.5f, 0.5f } );
        m_Registry.AddComponent<Velocity>( entity.Value(), Velocity{ d.dx, d.dy } );
        m_SpriteEntities.push_back( entity.Value() );
    }

    // The player: same components as a drifter, just controlled by
    // keyboard input (see OnSystemEvent/UpdatePlayerVelocity) instead of
    // a fixed velocity.
    auto player = m_Registry.CreateEntity();
    if ( !player ) { player.LogError(); return; }

    m_Player = player.Value();
    m_Registry.AddComponent<Transform>( m_Player, Transform{ 400.0f, 300.0f, 0.0f, 0.75f, 0.75f } );
    m_Registry.AddComponent<Velocity>( m_Player, Velocity{} );
    m_SpriteEntities.push_back( m_Player );
}

void EcsDemoGame::EnsureSpritesAttached(asge::video::IRenderer &inRenderer)
{
    if ( m_SpritesAttached ) return;

    // ASGE_ECS_DEMO_ASSET_DIR is injected by CMakeLists.txt.
    auto const path = std::filesystem::path(ASGE_ECS_DEMO_ASSET_DIR) / "checker.bmp";

    auto imageResult = asge::graphics::Image::Load(path);
    if ( !imageResult )
    {
        imageResult.LogError();
        return;
    }

    m_Texture = inRenderer.CreateTexture(imageResult.Value());
    if ( !m_Texture ) return;

    // Components can only be attached once a texture exists, so this
    // step is deferred to the first Render() call rather than done in
    // the constructor (same reasoning as texture_demo's own lazy load).
    for ( auto entity : m_SpriteEntities )
    {
        auto result = m_Registry.AddComponent<Sprite>( entity, Sprite{ m_Texture.get() } );
        if ( !result ) result.LogError();
    }

    m_SpritesAttached = true;
}

void EcsDemoGame::WrapAroundScreen()
{
    // Demo-specific dressing (not part of the shared Game/Systems library):
    // keeps drifting entities on screen by teleporting them across once
    // they fully exit one edge.
    for ( auto [ entity, transform ] : m_Registry.View<Transform>() )
    {
        (void)entity;
        auto& t = transform.get();
        float const margin = 64.0f * std::max(t.m_ScaleX, t.m_ScaleY); // rough sprite half-size

        if ( t.m_X < -margin )                    t.m_X = kWindowWidth + margin;
        else if ( t.m_X > kWindowWidth + margin )  t.m_X = -margin;

        if ( t.m_Y < -margin )                     t.m_Y = kWindowHeight + margin;
        else if ( t.m_Y > kWindowHeight + margin )  t.m_Y = -margin;
    }
}

void EcsDemoGame::UpdatePlayerVelocity()
{
    auto result = m_Registry.GetComponent<Velocity>( m_Player );
    if ( !result ) return;

    Velocity& velocity = result.Value().get();
    velocity.m_DX = (m_Right ? kPlayerSpeed : 0.0f) - (m_Left ? kPlayerSpeed : 0.0f);
    velocity.m_DY = (m_Down  ? kPlayerSpeed : 0.0f) - (m_Up   ? kPlayerSpeed : 0.0f);
}

void EcsDemoGame::Update(float inDeltaTime)
{
    UpdatePlayerVelocity();
    asge::game::systems::MovementSystem( m_Registry, inDeltaTime );
    WrapAroundScreen();
}

void EcsDemoGame::Render(asge::video::IRenderer &inRenderer)
{
    inRenderer.Clear({ 15, 15, 20, 255 });
    EnsureSpritesAttached(inRenderer);
    asge::game::systems::RenderSystem( m_Registry, inRenderer );
}

void EcsDemoGame::OnSystemEvent(asge::event::SystemEvent const &inSysEvent)
{
    auto const* keyEvent = inSysEvent.TryGet<asge::event::KeyboardEvent>();
    if ( !keyEvent ) return;

    bool const pressed = keyEvent->s_Type == asge::event::EventType::KEYBOARD_KEY_PRESSED;
    switch ( keyEvent->s_Keycode )
    {
    case asge::input::Keycode::W: m_Up = pressed; break;
    case asge::input::Keycode::S: m_Down = pressed; break;
    case asge::input::Keycode::A: m_Left = pressed; break;
    case asge::input::Keycode::D: m_Right = pressed; break;
    default: break;
    }
}
