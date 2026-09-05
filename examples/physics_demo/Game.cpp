#include "Game.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <type_traits>

namespace
{
using asge::game::components::Collider;
using asge::game::components::CollisionLayer;
using asge::game::components::ResolutionType;
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
constexpr std::size_t kMaxBoxes = 80; // keeps DetectCollisions's all-pairs check cheap

// A Trigger-resolution despawn zone near the bottom-right, sitting on top
// of the floor -- any box's Collider overlapping it is removed instead of
// being pushed out, since a Trigger never participates in solid push-out.
constexpr float kTriggerZoneSize = 150.0f;
constexpr float kTriggerZoneX    = kWindowWidth - kTriggerZoneSize - 30.0f;
constexpr float kTriggerZoneY    = kWindowHeight - kFloorHeight - kTriggerZoneSize;

constexpr asge::media::RGBA_Color kStaticColor{ 70, 70, 80, 255 };
constexpr asge::media::RGBA_Color kTriggerColor{ 240, 210, 60, 255 };
constexpr asge::media::RGBA_Color kBoxPalette[] = {
    { 220, 90, 90, 255 },
    { 90, 180, 220, 255 },
    { 230, 200, 90, 255 },
    { 120, 200, 120, 255 },
    { 190, 120, 220, 255 },
};

constexpr CollisionLayer kLayerDefault   = 1u;
constexpr CollisionLayer kLayerGhostRed  = 1u << 1;
constexpr CollisionLayer kLayerGhostBlue = 1u << 2;
constexpr CollisionLayer kMaskGhostRed   = kLayerDefault | kLayerGhostRed;
constexpr CollisionLayer kMaskGhostBlue  = kLayerDefault | kLayerGhostBlue;

constexpr float kGhostDemoRedX  = 110.0f;
constexpr float kGhostDemoBlueX = 140.0f;
constexpr asge::media::RGBA_Color kGhostRedColor{ 235, 60, 60, 255 };
constexpr asge::media::RGBA_Color kGhostBlueColor{ 60, 130, 235, 255 };
}

PhysicsDemoState::PhysicsDemoState(asge::ecs::Registry& inRegistry)
: m_Registry(inRegistry)
{
    SpawnStaticGeometry();
    SpawnTriggerZone();
    SpawnInitialStack();

    // Enter, not Stay/Exit -- this demo only cares about the first frame a
    // box touches the trigger zone (it gets despawned that same frame, so
    // there's never a "still overlapping" frame after to report Stay for).
    m_TriggerConnection = asge::game::events::OnCollisionTriggerEnter().Connect(
        [this]( asge::ecs::Entity inA, asge::ecs::Entity inB ) { HandleTriggerOverlap( inA, inB ); }
    );
}

void PhysicsDemoState::SpawnStaticGeometry()
{
    // Floor and side walls: Transform + Collider only -- no Velocity or
    // Rigidbody, so ResolveCollisions treats them as immovable.
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

void PhysicsDemoState::SpawnTriggerZone()
{
    auto entity = m_Registry.CreateEntity();
    if ( !entity ) { entity.LogError(); return; }

    m_Registry.AddComponent<Transform>( entity.Value(),
        Transform{ kTriggerZoneX, kTriggerZoneY, 0.0f, 1.0f, 1.0f } );
    m_Registry.AddComponent<Collider>( entity.Value(), Collider{
        asge::math::Rect{ 0.0f, 0.0f, kTriggerZoneSize, kTriggerZoneSize },
        ResolutionType::Trigger
    } );

    m_TriggerZone = entity.Value();
}

void PhysicsDemoState::SpawnBox(asge::math::Float2 inCenter, CollisionLayer inLayer, CollisionLayer inMask, float inSize)
{
    if ( m_Boxes.size() >= kMaxBoxes ) return;

    auto entity = m_Registry.CreateEntity();
    if ( !entity ) { entity.LogError(); return; }

    std::uniform_real_distribution<float> sizeDist( kMinBoxSize, kMaxBoxSize );
    std::uniform_real_distribution<float> massDist( kMinMass, kMaxMass );
    float const size = inSize > 0.0f ? inSize : sizeDist( m_Rng );

    m_Registry.AddComponent<Transform>( entity.Value(),
        Transform{ inCenter.x() - size * 0.5f, inCenter.y() - size * 0.5f, 0.0f, 1.0f, 1.0f } );
    m_Registry.AddComponent<Velocity>( entity.Value(), Velocity{} );
    m_Registry.AddComponent<Collider>( entity.Value(), Collider{
        asge::math::Rect{ 0.0f, 0.0f, size, size }, ResolutionType::Solid, inLayer, inMask
    } );
    m_Registry.AddComponent<Rigidbody>( entity.Value(), Rigidbody{ massDist( m_Rng ), true } );

    m_Boxes.push_back( entity.Value() );
}

void PhysicsDemoState::SpawnInitialStack()
{
    // A staggered column dropped from above the floor, so gravity and
    // mass-weighted collision resolution have something to settle right
    // away, without waiting for a click.
    for ( int i = 0; i < 6; ++i )
    {
        float const xOffset = (i % 2 == 0) ? -20.0f : 20.0f;
        SpawnBox({ kWindowWidth * 0.5f + xOffset, 80.0f * float(i) });
    }

    // One extra box dropped straight above the trigger zone, so the
    // despawn-on-overlap behavior is visible immediately on startup/reset,
    // without needing a click.
    SpawnBox({ kTriggerZoneX + kTriggerZoneSize * 0.5f, 50.0f });

    SpawnBox( { kGhostDemoRedX, 20.0f },   kLayerGhostRed,  kMaskGhostRed,  48.0f );
    SpawnBox( { kGhostDemoBlueX, -80.0f }, kLayerGhostBlue, kMaskGhostBlue, 32.0f );
}

void PhysicsDemoState::Reset()
{
    for ( auto entity : m_Boxes )
    {
        if ( auto result = m_Registry.DestroyEntity( entity ); !result ) result.LogError();
    }
    m_Boxes.clear();
    m_ConsumedByTrigger.clear(); // nothing pending should survive a reset
    SpawnInitialStack();
}

void PhysicsDemoState::DestroyBox(asge::ecs::Entity inEntity)
{
    if ( auto result = m_Registry.DestroyEntity( inEntity ); !result ) result.LogError();
    m_Boxes.erase( std::remove( m_Boxes.begin(), m_Boxes.end(), inEntity ), m_Boxes.end() );
}

void PhysicsDemoState::DespawnFallenBoxes()
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

void PhysicsDemoState::HandleTriggerOverlap(asge::ecs::Entity inA, asge::ecs::Entity inB)
{
    // events::OnCollisionTriggerEnter() fires synchronously, from inside
    // systems::DispatchTriggerEvents -- itself called partway through
    // PhysicsUpdate, still iterating this frame's contact list. Destroying
    // an entity here rather than deferring it could disturb that in-progress
    // iteration (or any later contact in the same list still referencing this
    // entity), so just record which box to remove; the actual destruction
    // happens in ProcessTriggerDespawns(), after PhysicsUpdate has returned
    // for this frame.
    asge::ecs::Entity other;
    if ( inA == m_TriggerZone )      other = inB;
    else if ( inB == m_TriggerZone ) other = inA;
    else return; // this overlap doesn't involve our despawn zone

    m_ConsumedByTrigger.push_back( other );
}

void PhysicsDemoState::ProcessTriggerDespawns()
{
    for ( auto entity : m_ConsumedByTrigger )
    {
        LOG_INFO( "[Trigger] box (entity index ", entity.m_Index, ") despawned by the trigger zone" );
        DestroyBox( entity );
    }
    m_ConsumedByTrigger.clear();
}

void PhysicsDemoState::HandleInput(asge::input::InputState const &inInput)
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

std::optional<asge::game::state::Transition<int>>
PhysicsDemoState::Update(float inDeltaTime, asge::input::InputState const &inInput)
{
    HandleInput( inInput );

    // May queue trigger-zone despawns via HandleTriggerOverlap (connected
    // to events::OnCollisionTriggerEnter in the constructor).
    asge::game::systems::PhysicsUpdate( m_Registry, m_PhysicsState, inDeltaTime );

    ProcessTriggerDespawns();
    DespawnFallenBoxes();
    return std::nullopt;
}

void PhysicsDemoState::Render(asge::video::IRenderer &inRenderer)
{
    inRenderer.Clear({ 18, 18, 24, 255 });

    // No Sprite/RenderSystem here -- Colliders don't carry a texture, so
    // this demo draws each entity's world-space Collider bounds directly.
    // Every Collider this demo spawns is a Rect today, but drawing through
    // std::visit rather than assuming .x/.y/.w/.h keeps this correct if a
    // Circle collider ever gets spawned here too.
    for ( auto [ entity, transform, collider ] : m_Registry.View<Transform, Collider>() )
    {
        auto const& t = transform.get();
        auto const& c = collider.get();

        // Trigger geometry is drawn outlined-only in a distinct color, so
        // it visually reads as "a zone to watch for", not solid geometry.
        bool const isTrigger = c.m_Resolution == ResolutionType::Trigger;
        bool const isBox = m_Registry.HasComponent<Rigidbody>( entity );
        auto const color = isTrigger ? kTriggerColor
            : c.m_Layer == kLayerGhostRed  ? kGhostRedColor
            : c.m_Layer == kLayerGhostBlue ? kGhostBlueColor
            : isBox ? kBoxPalette[ entity.m_Index % std::size(kBoxPalette) ]
            : kStaticColor;
        bool const fill = !isTrigger;

        std::visit( [&]( auto const& inShape )
        {
            using ShapeT = std::decay_t<decltype(inShape)>;
            if constexpr ( std::is_same_v<ShapeT, asge::math::Rect> )
            {
                asge::math::Rect const bounds{
                    t.m_X + inShape.x, t.m_Y + inShape.y, inShape.w, inShape.h
                };
                inRenderer.DrawRect( bounds, color, fill );
            }
            else
            {
                asge::math::Int2 const center{
                    static_cast<int>( t.m_X + inShape.m_Center.x() ),
                    static_cast<int>( t.m_Y + inShape.m_Center.y() )
                };
                inRenderer.DrawCircle( center, static_cast<int>( inShape.m_Radius ), color, fill );
            }
        }, c.m_LocalBounds );
    }
}

void PhysicsDemoState::OnSystemEvent([[maybe_unused]] asge::event::SystemEvent const &inSysEvent)
{
    // Everything here is driven by polling InputState in Update() instead.
}

PhysicsDemoGame::PhysicsDemoGame(asge::video::IRenderer& inRenderer)
: Game(inRenderer)
{
    SetInitialState(0);
}

std::unique_ptr<PhysicsDemoGame::StateType> PhysicsDemoGame::CreateState([[maybe_unused]] int inId)
{
    return std::make_unique<PhysicsDemoState>( m_SceneManager.GetRegistry() );
}
