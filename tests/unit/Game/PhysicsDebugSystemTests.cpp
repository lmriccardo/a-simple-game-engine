#include <ASGE/Game/Systems/PhysicsDebugSystem.hpp>
#include <ASGE/Core/ECS/Registry.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

namespace
{

using asge::ecs::Entity;
using asge::ecs::Registry;
using asge::game::components::Collider;
using asge::game::components::ResolutionType;
using asge::game::components::Transform;

// IRenderer stub recording every DrawRect/DrawCircle call it receives, so
// tests can assert on DebugDrawColliders' shape/color choices without a
// real window/GPU.
class RecordingRenderer final : public asge::video::IRenderer
{
public:
    struct RectCall   { asge::math::Rect m_Rect; asge::media::RGBA_Color m_Color; bool m_Fill; };
    struct CircleCall { asge::math::Int2 m_Center; int m_Radius; asge::media::RGBA_Color m_Color; bool m_Fill; };

    mutable std::vector<RectCall>   m_RectCalls;
    mutable std::vector<CircleCall> m_CircleCalls;

    void Clear(asge::media::RGBA_Color const&) const override {}

    void DrawRect(asge::math::Rect const& inRect,
        asge::media::RGBA_Color const& inColor, bool inFill) const override
    {
        m_RectCalls.push_back({ inRect, inColor, inFill });
    }

    void DrawLine(asge::math::Float2 const&, asge::math::Float2 const&,
        asge::media::RGBA_Color const&) const override {}

    void DrawCircle(asge::math::Int2 const& inCenter, int inRadius,
        asge::media::RGBA_Color const& inColor, bool inFill) const override
    {
        m_CircleCalls.push_back({ inCenter, inRadius, inColor, inFill });
    }

    void DrawTexture(asge::video::ITexture const&, asge::math::Rect const&) const noexcept override {}
    void DrawTexture(asge::video::ITexture const&, asge::math::Float2 const&) const noexcept override {}
    void DrawTexture(asge::video::ITexture const&, asge::math::Rect const&,
        asge::math::Rect const&) const noexcept override {}
    void DrawTexture9Grid(asge::video::ITexture const&, float, float, float, float,
        asge::math::Rect const&) const noexcept override {}
    void DrawTextureTiled(asge::video::ITexture const&, float, asge::math::Rect const&) const noexcept override {}
    void DrawTextureAffine(asge::video::ITexture const&, asge::math::Float2 const&,
        asge::math::Float2 const&, asge::math::Float2 const&) const noexcept override {}
    void DrawString(asge::str::StringView, asge::media::Font const&, asge::video::ITexture&,
        asge::math::Float2 const&, asge::media::RGBA_Color const&) const noexcept override {}

    void Present() const override {}
    [[nodiscard]] std::unique_ptr<asge::video::ITexture> CreateTexture(
        asge::media::Image const&) const noexcept override { return nullptr; }
    [[nodiscard]] bool IsValid() const override { return true; }
};

Entity MakeRectCollider(Registry& inRegistry, float inX, float inY,
    ResolutionType inResolution = ResolutionType::Solid)
{
    auto entity = inRegistry.CreateEntity();
    EXPECT_TRUE(entity.IsOk());
    EXPECT_TRUE(inRegistry.AddComponent(entity.Value(), Transform{ .m_X = inX, .m_Y = inY }).IsOk());
    EXPECT_TRUE(inRegistry.AddComponent(entity.Value(), Collider{
        .m_LocalBounds = asge::math::Rect{ 5.0f, 6.0f, 30.0f, 40.0f },
        .m_Resolution = inResolution
    }).IsOk());
    return entity.Value();
}

// ─── DebugDrawColliders — shape dispatch ────────────────────────────────────

TEST(PhysicsDebugSystemTest, RectCollider_DrawsUnfilledRectAtWorldOffsetBounds)
{
    Registry registry;
    RecordingRenderer renderer;
    MakeRectCollider(registry, 10.0f, 20.0f);

    asge::game::systems::DebugDrawColliders(registry, renderer);

    ASSERT_EQ(renderer.m_RectCalls.size(), 1u);
    EXPECT_TRUE(renderer.m_CircleCalls.empty());
    auto const& call = renderer.m_RectCalls[0];
    EXPECT_FALSE(call.m_Fill);
    EXPECT_FLOAT_EQ(call.m_Rect.x, 15.0f); // 10 + 5
    EXPECT_FLOAT_EQ(call.m_Rect.y, 26.0f); // 20 + 6
    EXPECT_FLOAT_EQ(call.m_Rect.w, 30.0f);
    EXPECT_FLOAT_EQ(call.m_Rect.h, 40.0f);
}

TEST(PhysicsDebugSystemTest, CircleCollider_DrawsUnfilledCircleAtWorldOffsetCenter)
{
    Registry registry;
    RecordingRenderer renderer;

    auto entity = registry.CreateEntity();
    ASSERT_TRUE(entity.IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), Transform{ .m_X = 10.0f, .m_Y = 20.0f }).IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), Collider{
        .m_LocalBounds = asge::math::Circle{ asge::math::Float2{ 5.0f, 6.0f }, 8.0f }
    }).IsOk());

    asge::game::systems::DebugDrawColliders(registry, renderer);

    ASSERT_EQ(renderer.m_CircleCalls.size(), 1u);
    EXPECT_TRUE(renderer.m_RectCalls.empty());
    auto const& call = renderer.m_CircleCalls[0];
    EXPECT_FALSE(call.m_Fill);
    EXPECT_EQ(call.m_Center.x(), 15); // 10 + 5
    EXPECT_EQ(call.m_Center.y(), 26); // 20 + 6
    EXPECT_EQ(call.m_Radius, 8);
}

TEST(PhysicsDebugSystemTest, EmptyRegistry_DrawsNothing)
{
    Registry registry;
    RecordingRenderer renderer;

    asge::game::systems::DebugDrawColliders(registry, renderer);

    EXPECT_TRUE(renderer.m_RectCalls.empty());
    EXPECT_TRUE(renderer.m_CircleCalls.empty());
}

// ─── DebugDrawColliders — color by ResolutionType ───────────────────────────

void ExpectColor(asge::media::RGBA_Color const& inColor,
    std::uint8_t inR, std::uint8_t inG, std::uint8_t inB, std::uint8_t inA)
{
    EXPECT_EQ(inColor.r, inR);
    EXPECT_EQ(inColor.g, inG);
    EXPECT_EQ(inColor.b, inB);
    EXPECT_EQ(inColor.a, inA);
}

TEST(PhysicsDebugSystemTest, SolidResolution_DrawsGreen)
{
    Registry registry;
    RecordingRenderer renderer;
    MakeRectCollider(registry, 0.0f, 0.0f, ResolutionType::Solid);

    asge::game::systems::DebugDrawColliders(registry, renderer);

    ASSERT_EQ(renderer.m_RectCalls.size(), 1u);
    ExpectColor(renderer.m_RectCalls[0].m_Color, 0, 255, 0, 255);
}

TEST(PhysicsDebugSystemTest, TriggerResolution_DrawsYellow)
{
    Registry registry;
    RecordingRenderer renderer;
    MakeRectCollider(registry, 0.0f, 0.0f, ResolutionType::Trigger);

    asge::game::systems::DebugDrawColliders(registry, renderer);

    ASSERT_EQ(renderer.m_RectCalls.size(), 1u);
    ExpectColor(renderer.m_RectCalls[0].m_Color, 255, 255, 0, 255);
}

TEST(PhysicsDebugSystemTest, UnknownResolution_DrawsGrey)
{
    Registry registry;
    RecordingRenderer renderer;
    MakeRectCollider(registry, 0.0f, 0.0f, ResolutionType::Unknown);

    asge::game::systems::DebugDrawColliders(registry, renderer);

    ASSERT_EQ(renderer.m_RectCalls.size(), 1u);
    ExpectColor(renderer.m_RectCalls[0].m_Color, 128, 128, 128, 255);
}

}
