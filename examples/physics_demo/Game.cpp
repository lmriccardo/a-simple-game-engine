#include "Game.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <type_traits>

namespace
{
using asge::game::components::Collider;
using asge::game::components::Rigidbody;
using asge::game::components::Transform;
using asge::game::components::Velocity;

constexpr float kWindowWidth   = 800.0f;
constexpr float kWindowHeight  = 600.0f;
constexpr float kWallThickness = 20.0f;
constexpr float kFloorHeight   = 30.0f;
constexpr float kMinBoxSize    = 24.0f;
constexpr float kMaxBoxSize    = 48.0f;
constexpr float kMinMass       = 0.5f;
constexpr float kMaxMass       = 3.0f;
constexpr float kFallLimit     = kWindowHeight + 200.0f; // safety net for a box tunneling past the floor
constexpr std::size_t kMaxBoxes = 80; // keeps the all-pairs CollisionResolution cheap

constexpr asge::graphics::RGBA_Color kStaticColor{ 70, 70, 80, 255 };
constexpr asge::graphics::RGBA_Color kBoxPalette[] = {
    { 220, 90, 90, 255 },
    { 90, 180, 220, 255 },
    { 230, 200, 90, 255 },
    { 120, 200, 120, 255 },
    { 190, 120, 220, 255 },
};
}

PhysicsDemoGame::PhysicsDemoGame()
{
    SpawnStaticGeometry();
    SpawnInitialStack();
}

void PhysicsDemoGame::SpawnStaticGeometry()
{
    // Floor and side walls: Transform + Collider only -- no Velocity or
    // Rigidbody, so CollisionResolution treats them as immovable.
    auto makeStatic = [this]( asge::math::Rect inBounds )
    {
        auto entity = m_Registry.CreateEntity();
        if ( !entity ) { entity.LogError(); return; }

        m_Registry.AddComponent<Transform>( entity.Value(), Transform{ inBounds.x, inBounds.y, 0.0f, 1.0f, 1.0f } );
        m_Registry.AddComponent<Collider>( entity.Value(),
            Collider{ asge::math::Rect{ 0.0f, 0.0f, inBounds.w, inBounds.h } } );
    };

    makeStatic({ 0.0f, kWindowHeight - kFloorHeight, kWindowWidth, kFloorHeight });      // floor
    makeStatic({ -kWallThickness, 0.0f, kWallThickness, kWindowHeight });                // left wall
    makeStatic({ kWindowWidth, 0.0f, kWallThickness, kWindowHeight });                   // right wall
}

void PhysicsDemoGame::SpawnBox(asge::math::Float2 inCenter)
{
    if ( m_Boxes.size() >= kMaxBoxes ) return;

    auto entity = m_Registry.CreateEntity();
    if ( !entity ) { entity.LogError(); return; }

    std::uniform_real_distribution<float> sizeDist( kMinBoxSize, kMaxBoxSize );
    std::uniform_real_distribution<float> massDist( kMinMass, kMaxMass );
    float const size = sizeDist( m_Rng );

    m_Registry.AddComponent<Transform>( entity.Value(),
        Transform{ inCenter.x() - size * 0.5f, inCenter.y() - size * 0.5f, 0.0f, 1.0f, 1.0f } );
    m_Registry.AddComponent<Velocity>( entity.Value(), Velocity{} );
    m_Registry.AddComponent<Collider>( entity.Value(), Collider{ asge::math::Rect{ 0.0f, 0.0f, size, size } } );
    m_Registry.AddComponent<Rigidbody>( entity.Value(), Rigidbody{ massDist( m_Rng ), true } );

    m_Boxes.push_back( entity.Value() );
}

void PhysicsDemoGame::SpawnInitialStack()
{
    // A staggered column dropped from above the floor, so gravity and
    // mass-weighted collision resolution have something to settle right
    // away, without waiting for a click.
    for ( int i = 0; i < 6; ++i )
    {
        float const xOffset = (i % 2 == 0) ? -20.0f : 20.0f;
        SpawnBox({ kWindowWidth * 0.5f + xOffset, 80.0f * float(i) });
    }
}

void PhysicsDemoGame::Reset()
{
    for ( auto entity : m_Boxes )
    {
        if ( auto result = m_Registry.DestroyEntity( entity ); !result ) result.LogError();
    }
    m_Boxes.clear();
    SpawnInitialStack();
}

void PhysicsDemoGame::DespawnFallenBoxes()
{
    m_Boxes.erase(
        std::remove_if( m_Boxes.begin(), m_Boxes.end(), [this]( asge::ecs::Entity inEntity )
        {
            auto transform = m_Registry.GetComponent<Transform>( inEntity );
            bool const fellThrough = transform.IsOk() && transform.Value().get().m_Y > kFallLimit;
            if ( fellThrough )
            {
                if ( auto result = m_Registry.DestroyEntity( inEntity ); !result ) result.LogError();
            }
            return fellThrough;
        }),
        m_Boxes.end()
    );
}

void PhysicsDemoGame::HandleInput(asge::input::InputState const &inInput)
{
    using asge::input::Keycode;
    using asge::input::MouseButton;

    if ( inInput.IsMouseButtonPressed( MouseButton::LEFT ) )
    {
        SpawnBox( inInput.GetMousePosition() );
    }

    if ( inInput.IsKeyPressed( Keycode::R ) )
    {
        Reset();
    }
}

void PhysicsDemoGame::Update(float inDeltaTime, asge::input::InputState const &inInput)
{
    HandleInput( inInput );

    asge::game::systems::GravitySystem( m_Registry, inDeltaTime );
    asge::game::systems::MovementSystem( m_Registry, inDeltaTime );
    asge::game::systems::CollisionResolution( m_Registry );

    DespawnFallenBoxes();
}

void PhysicsDemoGame::Render(asge::video::IRenderer &inRenderer)
{
    inRenderer.Clear({ 18, 18, 24, 255 });

    // No Sprite/RenderSystem here -- Colliders don't carry a texture, so
    // this demo draws each entity's world-space Collider bounds directly.
    for ( auto [ entity, transform, collider ] : m_Registry.View<Transform, Collider>() )
    {
        auto const& t = transform.get();
        auto const& c = collider.get();

        bool const isBox = m_Registry.HasComponent<Rigidbody>( entity );
        auto const color = isBox ? kBoxPalette[ entity.m_Index % std::size(kBoxPalette) ] : kStaticColor;

        // Every Collider this demo spawns is a Rect today, but drawing
        // through std::visit rather than assuming .x/.y/.w/.h keeps this
        // correct if a Circle collider ever gets spawned here too.
        std::visit( [&]( auto const& inShape )
        {
            using ShapeT = std::decay_t<decltype(inShape)>;
            if constexpr ( std::is_same_v<ShapeT, asge::math::Rect> )
            {
                asge::math::Rect const bounds{
                    t.m_X + inShape.x, t.m_Y + inShape.y, inShape.w, inShape.h
                };
                inRenderer.DrawRect( bounds, color, true );
            }
            else
            {
                asge::math::Int2 const center{
                    static_cast<int>( t.m_X + inShape.m_Center.x() ),
                    static_cast<int>( t.m_Y + inShape.m_Center.y() )
                };
                inRenderer.DrawCircle( center, static_cast<int>( inShape.m_Radius ), color, true );
            }
        }, c.m_LocalBounds );
    }
}

void PhysicsDemoGame::OnSystemEvent([[maybe_unused]] asge::event::SystemEvent const &inSysEvent)
{
    // Everything here is driven by polling InputState in Update() instead.
}
