#include <ASGE/Game/Systems/RenderSystem.hpp>
#include <ASGE/Game/Components/Animation.hpp>
#include <ASGE/Game/Components/Camera.hpp>
#include <ASGE/Game/Resources/ActiveCamera.hpp>
#include <ASGE/Core/ECS/Registry.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <vector>

namespace
{

using asge::Result;
using asge::ecs::Registry;
using asge::game::components::Animation;
using asge::game::components::Camera;
using asge::game::components::Sprite;
using asge::game::components::Transform;
using asge::game::resources::ActiveCamera;

// Minimal ITexture stub that just reports a fixed size -- no SDL/GPU
// resource, so RenderSystem can be exercised without a real renderer.
class FakeTexture final : public asge::video::ITexture
{
public:
    explicit FakeTexture(asge::math::Int2 inSize) : m_Size(inSize) {}

    [[nodiscard]] asge::math::Int2 Size() const noexcept override { return m_Size; }
    [[nodiscard]] void* NativeHandle() const noexcept override { return nullptr; }
    [[nodiscard]] bool IsValid() const noexcept override { return true; }

    void SetColorMod(asge::media::RGBA_Color) noexcept override {}

    [[nodiscard]] Result<asge::media::RGBA_Color> GetColorMod() const noexcept override
    {
        return Result<asge::media::RGBA_Color>::Ok(asge::media::RGBA_Color{});
    }

private:
    asge::math::Int2 m_Size;
};

// IRenderer stub recording every DrawTexture(src, dest) / DrawTexture(dest)
// call it receives, so tests can assert on the destRect RenderSystem
// computed without needing a real window/GPU.
class RecordingRenderer final : public asge::video::IRenderer
{
public:
    struct DrawCall
    {
        asge::math::Rect m_DestRect;         // set for a rect-based DrawTexture call, untouched otherwise
        bool m_HadSourceRect;                // rect-based DrawTexture only: whether it was the srcRect+destRect overload
        bool m_WasAffine{false};             // true for either DrawTextureAffine overload
        bool m_AffineHadSourceRect{false};   // affine only: whether it was the srcRect-taking overload
        asge::math::Float2 m_Origin{};       // affine only
        asge::math::Float2 m_Right{};        // affine only
        asge::math::Float2 m_Down{};         // affine only
    };

    mutable std::vector<DrawCall> m_Calls;

    void Clear(asge::media::RGBA_Color const&) const override {}
    void DrawRect(asge::math::Rect const&, asge::media::RGBA_Color const&, bool) const override {}
    void DrawLine(asge::math::Float2 const&, asge::math::Float2 const&,
        asge::media::RGBA_Color const&) const override {}
    void DrawCircle(asge::math::Int2 const&, int, asge::media::RGBA_Color const&, bool) const override {}

    void DrawTexture(asge::video::ITexture const&, asge::math::Rect const& inDestRect) const noexcept override
    {
        m_Calls.push_back({ inDestRect, false });
    }

    void DrawTexture(asge::video::ITexture const&, asge::math::Float2 const&) const noexcept override {}

    void DrawTexture(asge::video::ITexture const&, asge::math::Rect const&,
        asge::math::Rect const& inDestRect) const noexcept override
    {
        m_Calls.push_back({ inDestRect, true });
    }

    void DrawTexture9Grid(asge::video::ITexture const&, float, float, float, float,
        asge::math::Rect const&) const noexcept override {}
    void DrawTextureTiled(asge::video::ITexture const&, float, asge::math::Rect const&) const noexcept override {}
    void DrawTextureAffine(asge::video::ITexture const&, asge::math::Float2 const& inOrigin,
        asge::math::Float2 const& inRight, asge::math::Float2 const& inDown) const noexcept override
    {
        m_Calls.push_back({ {}, false, true, false, inOrigin, inRight, inDown });
    }

    void DrawTextureAffine(asge::video::ITexture const&, asge::math::Rect const&, asge::math::Float2 const& inOrigin,
        asge::math::Float2 const& inRight, asge::math::Float2 const& inDown) const noexcept override
    {
        m_Calls.push_back({ {}, false, true, true, inOrigin, inRight, inDown });
    }
    void DrawString(asge::str::StringView, asge::media::Font const&, asge::video::ITexture&,
        asge::math::Float2 const&, asge::media::RGBA_Color const&) const noexcept override {}

    void Present() const override {}

    [[nodiscard]] std::unique_ptr<asge::video::ITexture> CreateTexture(
        asge::media::Image const&) const noexcept override { return nullptr; }

    [[nodiscard]] bool IsValid() const override { return true; }

    void SetCamera(asge::video::Camera const& inCamera) override { m_Camera = inCamera; }
    [[nodiscard]] asge::video::Camera const& GetCamera() const override { return m_Camera; }
    void SetViewport(asge::video::Viewport const& inViewport) override { m_Viewport = inViewport; }
    [[nodiscard]] asge::video::Viewport const& GetViewport() const override { return m_Viewport; }

private:
    asge::video::Camera   m_Camera{};
    // Large enough that every existing test's small, arbitrary coordinates
    // stay inside RenderSystem's visible-rect culling (see VisibleWorldRect)
    // without each test needing its own SetViewport call -- mirrors
    // SDLRenderer now defaulting its viewport to the real window size
    // instead of zero (see SDLRendererCameraTests.cpp's
    // DefaultViewport_MatchesTheWindowSize).
    asge::video::Viewport m_Viewport{ 0.0f, 0.0f, 800.0f, 600.0f };
};

// ─── RenderSystem — whole-texture sprites (no source rect) ─────────────────────

TEST(RenderSystemTest, NoSourceRect_DestRectSizedFromFullTextureScaled)
{
    Registry registry;
    FakeTexture texture(asge::math::Int2{ 64, 32 });
    RecordingRenderer renderer;

    auto entity = registry.CreateEntity();
    ASSERT_TRUE(entity.IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(),
        Transform{ .m_X = 10.0f, .m_Y = 20.0f, .m_ScaleX = 2.0f, .m_ScaleY = 3.0f }).IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), Sprite{ .m_Texture = &texture }).IsOk());

    asge::game::systems::RenderSystem(registry, renderer);

    ASSERT_EQ(renderer.m_Calls.size(), 1u);
    EXPECT_FALSE(renderer.m_Calls[0].m_HadSourceRect);
    auto const& destRect = renderer.m_Calls[0].m_DestRect;
    EXPECT_FLOAT_EQ(destRect.m_X, 10.0f);
    EXPECT_FLOAT_EQ(destRect.m_Y, 20.0f);
    EXPECT_FLOAT_EQ(destRect.m_Width, 128.0f); // 64 * 2
    EXPECT_FLOAT_EQ(destRect.m_Height, 96.0f);  // 32 * 3
}

// ─── RenderSystem — cropped sprites (source rect set) ───────────────────────────

TEST(RenderSystemTest, SourceRectSet_DestRectSizedFromSourceRectNotFullTexture)
{
    Registry registry;
    // A 256x256 spritesheet, cropped down to one 32x32 cell -- exactly the
    // regression from issue #35.
    FakeTexture texture(asge::math::Int2{ 256, 256 });
    RecordingRenderer renderer;

    auto entity = registry.CreateEntity();
    ASSERT_TRUE(entity.IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(),
        Transform{ .m_X = 5.0f, .m_Y = 5.0f, .m_ScaleX = 2.0f, .m_ScaleY = 2.0f }).IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), Sprite{
        .m_Texture = &texture,
        .m_SourceRect = asge::math::Rect{ 64.0f, 0.0f, 32.0f, 32.0f }
    }).IsOk());

    asge::game::systems::RenderSystem(registry, renderer);

    ASSERT_EQ(renderer.m_Calls.size(), 1u);
    EXPECT_TRUE(renderer.m_Calls[0].m_HadSourceRect);
    auto const& destRect = renderer.m_Calls[0].m_DestRect;
    EXPECT_FLOAT_EQ(destRect.m_X, 5.0f);
    EXPECT_FLOAT_EQ(destRect.m_Y, 5.0f);
    // Must come from the 32x32 source cell * scale, not the 256x256 sheet.
    EXPECT_FLOAT_EQ(destRect.m_Width, 64.0f); // 32 * 2
    EXPECT_FLOAT_EQ(destRect.m_Height, 64.0f); // 32 * 2
}

TEST(RenderSystemTest, SourceRectAndFullTextureEntities_EachDestRectComputedIndependently)
{
    Registry registry;
    FakeTexture sheet(asge::math::Int2{ 256, 256 });
    FakeTexture standalone(asge::math::Int2{ 16, 16 });
    RecordingRenderer renderer;

    auto cropped = registry.CreateEntity();
    ASSERT_TRUE(cropped.IsOk());
    ASSERT_TRUE(registry.AddComponent(cropped.Value(),
        Transform{ .m_X = 0.0f, .m_Y = 0.0f, .m_ScaleX = 1.0f, .m_ScaleY = 1.0f }).IsOk());
    ASSERT_TRUE(registry.AddComponent(cropped.Value(), Sprite{
        .m_Texture = &sheet,
        .m_SourceRect = asge::math::Rect{ 0.0f, 0.0f, 32.0f, 32.0f }
    }).IsOk());

    auto whole = registry.CreateEntity();
    ASSERT_TRUE(whole.IsOk());
    ASSERT_TRUE(registry.AddComponent(whole.Value(),
        Transform{ .m_X = 0.0f, .m_Y = 0.0f, .m_ScaleX = 1.0f, .m_ScaleY = 1.0f }).IsOk());
    ASSERT_TRUE(registry.AddComponent(whole.Value(), Sprite{ .m_Texture = &standalone }).IsOk());

    asge::game::systems::RenderSystem(registry, renderer);

    ASSERT_EQ(renderer.m_Calls.size(), 2u);
    for (auto const& call : renderer.m_Calls)
    {
        // Cropped entity's 32x32 source cell must not leak its size onto
        // the uncropped entity's destRect (or vice versa).
        float const expected = call.m_HadSourceRect ? 32.0f : 16.0f;
        EXPECT_FLOAT_EQ(call.m_DestRect.m_Width, expected);
        EXPECT_FLOAT_EQ(call.m_DestRect.m_Height, expected);
    }
}

// ─── RenderSystem — layer ordering ──────────────────────────────────────────────

TEST(RenderSystemTest, Layer_LowerLayerDrawnBeforeHigherLayer)
{
    Registry registry;
    FakeTexture texture(asge::math::Int2{ 10, 10 });
    RecordingRenderer renderer;

    auto high = registry.CreateEntity();
    ASSERT_TRUE(high.IsOk());
    ASSERT_TRUE(registry.AddComponent(high.Value(),
        Transform{ .m_X = 100.0f, .m_Y = 0.0f }).IsOk());
    ASSERT_TRUE(registry.AddComponent(high.Value(),
        Sprite{ .m_Texture = &texture, .m_Layer = 5 }).IsOk());

    auto low = registry.CreateEntity();
    ASSERT_TRUE(low.IsOk());
    ASSERT_TRUE(registry.AddComponent(low.Value(),
        Transform{ .m_X = 200.0f, .m_Y = 0.0f }).IsOk());
    ASSERT_TRUE(registry.AddComponent(low.Value(),
        Sprite{ .m_Texture = &texture, .m_Layer = 1 }).IsOk());

    asge::game::systems::RenderSystem(registry, renderer);

    ASSERT_EQ(renderer.m_Calls.size(), 2u);
    // Layer 1 (low) must draw before layer 5 (high) regardless of creation order.
    EXPECT_FLOAT_EQ(renderer.m_Calls[0].m_DestRect.m_X, 200.0f);
    EXPECT_FLOAT_EQ(renderer.m_Calls[1].m_DestRect.m_X, 100.0f);
}

// ─── RenderSystem — y-sort within a layer ───────────────────────────────────────

TEST(RenderSystemTest, YSort_SortsByBottomEdgeWithinSameLayer)
{
    Registry registry;
    FakeTexture texture(asge::math::Int2{ 10, 10 }); // Fixed 10-tall, so bottom edge = y + 10
    RecordingRenderer renderer;

    auto front = registry.CreateEntity(); // Higher on screen -> lower bottom edge -> drawn first
    ASSERT_TRUE(front.IsOk());
    ASSERT_TRUE(registry.AddComponent(front.Value(),
        Transform{ .m_X = 2.0f, .m_Y = 10.0f }).IsOk());
    ASSERT_TRUE(registry.AddComponent(front.Value(),
        Sprite{ .m_Texture = &texture, .m_YSort = true }).IsOk());

    auto back = registry.CreateEntity(); // Created first, but lower on screen -> drawn last
    ASSERT_TRUE(back.IsOk());
    ASSERT_TRUE(registry.AddComponent(back.Value(),
        Transform{ .m_X = 1.0f, .m_Y = 100.0f }).IsOk());
    ASSERT_TRUE(registry.AddComponent(back.Value(),
        Sprite{ .m_Texture = &texture, .m_YSort = true }).IsOk());

    asge::game::systems::RenderSystem(registry, renderer);

    ASSERT_EQ(renderer.m_Calls.size(), 2u);
    EXPECT_FLOAT_EQ(renderer.m_Calls[0].m_DestRect.m_X, 2.0f);
    EXPECT_FLOAT_EQ(renderer.m_Calls[1].m_DestRect.m_X, 1.0f);
}

TEST(RenderSystemTest, YSort_TiedBottomEdge_FallsBackToEntityIndex)
{
    Registry registry;
    FakeTexture texture(asge::math::Int2{ 10, 10 });
    RecordingRenderer renderer;

    auto first = registry.CreateEntity();
    ASSERT_TRUE(first.IsOk());
    ASSERT_TRUE(registry.AddComponent(first.Value(),
        Transform{ .m_X = 1.0f, .m_Y = 10.0f }).IsOk());
    ASSERT_TRUE(registry.AddComponent(first.Value(),
        Sprite{ .m_Texture = &texture, .m_YSort = true }).IsOk());

    auto second = registry.CreateEntity();
    ASSERT_TRUE(second.IsOk());
    ASSERT_TRUE(registry.AddComponent(second.Value(),
        Transform{ .m_X = 2.0f, .m_Y = 10.0f }).IsOk()); // Same bottom edge as `first`
    ASSERT_TRUE(registry.AddComponent(second.Value(),
        Sprite{ .m_Texture = &texture, .m_YSort = true }).IsOk());

    asge::game::systems::RenderSystem(registry, renderer);

    ASSERT_EQ(renderer.m_Calls.size(), 2u);
    // Bottom edges tie, so creation order (entity index) decides.
    EXPECT_FLOAT_EQ(renderer.m_Calls[0].m_DestRect.m_X, 1.0f);
    EXPECT_FLOAT_EQ(renderer.m_Calls[1].m_DestRect.m_X, 2.0f);
}

TEST(RenderSystemTest, YSort_MixedWithNonYSortSprite_EitherOptingInSortsBothByY)
{
    Registry registry;
    FakeTexture texture(asge::math::Int2{ 10, 10 });
    RecordingRenderer renderer;

    // Created first, no y-sort, but sits lower on screen (larger bottom edge).
    auto plain = registry.CreateEntity();
    ASSERT_TRUE(plain.IsOk());
    ASSERT_TRUE(registry.AddComponent(plain.Value(),
        Transform{ .m_X = 1.0f, .m_Y = 100.0f }).IsOk());
    ASSERT_TRUE(registry.AddComponent(plain.Value(),
        Sprite{ .m_Texture = &texture, .m_YSort = false }).IsOk());

    // Created second, opts into y-sort, and sits higher on screen.
    auto sorted = registry.CreateEntity();
    ASSERT_TRUE(sorted.IsOk());
    ASSERT_TRUE(registry.AddComponent(sorted.Value(),
        Transform{ .m_X = 2.0f, .m_Y = 10.0f }).IsOk());
    ASSERT_TRUE(registry.AddComponent(sorted.Value(),
        Sprite{ .m_Texture = &texture, .m_YSort = true }).IsOk());

    asge::game::systems::RenderSystem(registry, renderer);

    ASSERT_EQ(renderer.m_Calls.size(), 2u);
    // Either side opting into y-sort is enough to order the pair by bottom
    // edge -- creation order alone (which would put `plain` first) is not used.
    EXPECT_FLOAT_EQ(renderer.m_Calls[0].m_DestRect.m_X, 2.0f);
    EXPECT_FLOAT_EQ(renderer.m_Calls[1].m_DestRect.m_X, 1.0f);
}

TEST(RenderSystemTest, NoYSort_SameLayer_PreservesEntityCreationOrderRegardlessOfY)
{
    Registry registry;
    FakeTexture texture(asge::math::Int2{ 10, 10 });
    RecordingRenderer renderer;

    // Created first but sits lower on screen than `second` -- without
    // y-sort, creation order must win, not vertical position.
    auto first = registry.CreateEntity();
    ASSERT_TRUE(first.IsOk());
    ASSERT_TRUE(registry.AddComponent(first.Value(),
        Transform{ .m_X = 1.0f, .m_Y = 100.0f }).IsOk());
    ASSERT_TRUE(registry.AddComponent(first.Value(),
        Sprite{ .m_Texture = &texture }).IsOk());

    auto second = registry.CreateEntity();
    ASSERT_TRUE(second.IsOk());
    ASSERT_TRUE(registry.AddComponent(second.Value(),
        Transform{ .m_X = 2.0f, .m_Y = 10.0f }).IsOk());
    ASSERT_TRUE(registry.AddComponent(second.Value(),
        Sprite{ .m_Texture = &texture }).IsOk());

    asge::game::systems::RenderSystem(registry, renderer);

    ASSERT_EQ(renderer.m_Calls.size(), 2u);
    EXPECT_FLOAT_EQ(renderer.m_Calls[0].m_DestRect.m_X, 1.0f);
    EXPECT_FLOAT_EQ(renderer.m_Calls[1].m_DestRect.m_X, 2.0f);
}

// ─── RenderSystem — null texture ────────────────────────────────────────────────

TEST(RenderSystemTest, NullTexture_EntitySkipped)
{
    Registry registry;
    RecordingRenderer renderer;

    auto entity = registry.CreateEntity();
    ASSERT_TRUE(entity.IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), Transform{}).IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), Sprite{ .m_Texture = nullptr }).IsOk());

    asge::game::systems::RenderSystem(registry, renderer);

    EXPECT_TRUE(renderer.m_Calls.empty());
}

// ─── RenderSystem — camera-driven culling ───────────────────────────────────────

TEST(RenderSystemTest, Culling_SpriteWithinTheDefaultViewport_IsDrawn)
{
    Registry registry;
    FakeTexture texture(asge::math::Int2{ 32, 32 });
    RecordingRenderer renderer; // default camera {0,0,zoom=1}, viewport {0,0,800,600}

    auto entity = registry.CreateEntity();
    ASSERT_TRUE(entity.IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), Transform{ .m_X = 100.0f, .m_Y = 100.0f }).IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), Sprite{ .m_Texture = &texture }).IsOk());

    asge::game::systems::RenderSystem(registry, renderer);

    EXPECT_EQ(renderer.m_Calls.size(), 1u);
}

TEST(RenderSystemTest, Culling_SpriteFarOutsideTheViewport_IsSkipped)
{
    Registry registry;
    FakeTexture texture(asge::math::Int2{ 32, 32 });
    RecordingRenderer renderer; // visible world rect is {0,0,800,600}

    auto entity = registry.CreateEntity();
    ASSERT_TRUE(entity.IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), Transform{ .m_X = 5000.0f, .m_Y = 5000.0f }).IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), Sprite{ .m_Texture = &texture }).IsOk());

    asge::game::systems::RenderSystem(registry, renderer);

    EXPECT_TRUE(renderer.m_Calls.empty());
}

TEST(RenderSystemTest, Culling_SpriteStraddlingTheViewportEdge_IsDrawn)
{
    // Overlap, not full containment, is the bar -- a sprite whose destination
    // rect only partly crosses into view must still be drawn, not clipped
    // away entirely by the cull check itself.
    Registry registry;
    FakeTexture texture(asge::math::Int2{ 64, 64 });
    RecordingRenderer renderer; // visible world rect ends at x=800

    auto entity = registry.CreateEntity();
    ASSERT_TRUE(entity.IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), Transform{ .m_X = 790.0f, .m_Y = 100.0f }).IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), Sprite{ .m_Texture = &texture }).IsOk());

    asge::game::systems::RenderSystem(registry, renderer);

    EXPECT_EQ(renderer.m_Calls.size(), 1u);
}

TEST(RenderSystemTest, Culling_FollowsTheRendererSCurrentCameraNotJustItsDefault)
{
    // Moving the camera away from the origin must shift what counts as
    // "visible" along with it -- culling reads IRenderer::GetCamera() fresh
    // each call rather than assuming an identity camera.
    Registry registry;
    FakeTexture texture(asge::math::Int2{ 32, 32 });
    RecordingRenderer renderer;
    renderer.SetCamera( asge::video::Camera{ .m_X = 2000.0f, .m_Y = 2000.0f, .m_Zoom = 1.0f } );

    auto nowOffscreen = registry.CreateEntity();
    ASSERT_TRUE(nowOffscreen.IsOk());
    ASSERT_TRUE(registry.AddComponent(nowOffscreen.Value(), Transform{ .m_X = 0.0f, .m_Y = 0.0f }).IsOk());
    ASSERT_TRUE(registry.AddComponent(nowOffscreen.Value(), Sprite{ .m_Texture = &texture }).IsOk());

    auto nowOnscreen = registry.CreateEntity();
    ASSERT_TRUE(nowOnscreen.IsOk());
    ASSERT_TRUE(registry.AddComponent(nowOnscreen.Value(), Transform{ .m_X = 2050.0f, .m_Y = 2050.0f }).IsOk());
    ASSERT_TRUE(registry.AddComponent(nowOnscreen.Value(), Sprite{ .m_Texture = &texture }).IsOk());

    asge::game::systems::RenderSystem(registry, renderer);

    ASSERT_EQ(renderer.m_Calls.size(), 1u);
    EXPECT_FLOAT_EQ(renderer.m_Calls[0].m_DestRect.m_X, 2050.0f);
}

// ─── RenderSystem — rotation ─────────────────────────────────────────────────────

TEST(RenderSystemTest, Rotation_ZeroRotation_UsesThePlainDrawTexturePath)
{
    Registry registry;
    FakeTexture texture(asge::math::Int2{ 32, 32 });
    RecordingRenderer renderer;

    auto entity = registry.CreateEntity();
    ASSERT_TRUE(entity.IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), Transform{ .m_X = 100.0f, .m_Y = 100.0f, .m_Rotation = 0.0f }).IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), Sprite{ .m_Texture = &texture }).IsOk());

    asge::game::systems::RenderSystem(registry, renderer);

    ASSERT_EQ(renderer.m_Calls.size(), 1u);
    EXPECT_FALSE(renderer.m_Calls[0].m_WasAffine);
}

TEST(RenderSystemTest, Rotation_NonZeroRotationNoSourceRect_RoutesThroughWholeTextureAffineOverload)
{
    Registry registry;
    FakeTexture texture(asge::math::Int2{ 32, 32 });
    RecordingRenderer renderer;

    auto entity = registry.CreateEntity();
    ASSERT_TRUE(entity.IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(),
        Transform{ .m_X = 100.0f, .m_Y = 100.0f, .m_Rotation = 3.14159265358979323846f * 0.5f }).IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), Sprite{ .m_Texture = &texture }).IsOk());

    asge::game::systems::RenderSystem(registry, renderer);

    ASSERT_EQ(renderer.m_Calls.size(), 1u);
    auto const& call = renderer.m_Calls[0];
    EXPECT_TRUE(call.m_WasAffine);
    EXPECT_FALSE(call.m_AffineHadSourceRect);

    // Dest rect is {100,100,32,32}, center (116,116); a 90-degree turn
    // (clockwise on screen) puts the texture's top-left at what was the
    // dest rect's top-right corner.
    EXPECT_NEAR(call.m_Origin.x(), 132.0f, 1e-2f);
    EXPECT_NEAR(call.m_Origin.y(), 100.0f, 1e-2f);
    EXPECT_NEAR(call.m_Right.x(), 132.0f, 1e-2f);
    EXPECT_NEAR(call.m_Right.y(), 132.0f, 1e-2f);
    EXPECT_NEAR(call.m_Down.x(), 100.0f, 1e-2f);
    EXPECT_NEAR(call.m_Down.y(), 100.0f, 1e-2f);
}

TEST(RenderSystemTest, Rotation_NonZeroRotationWithSourceRect_RoutesThroughSourceRectAffineOverload)
{
    Registry registry;
    FakeTexture texture(asge::math::Int2{ 256, 256 }); // a spritesheet
    RecordingRenderer renderer;

    auto entity = registry.CreateEntity();
    ASSERT_TRUE(entity.IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), Transform{ .m_X = 0.0f, .m_Y = 0.0f, .m_Rotation = 0.7f }).IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(),
        Sprite{ .m_Texture = &texture, .m_SourceRect = asge::math::Rect{ 0.0f, 0.0f, 16.0f, 16.0f } }).IsOk());

    asge::game::systems::RenderSystem(registry, renderer);

    ASSERT_EQ(renderer.m_Calls.size(), 1u);
    EXPECT_TRUE(renderer.m_Calls[0].m_WasAffine);
    EXPECT_TRUE(renderer.m_Calls[0].m_AffineHadSourceRect); // cropped *and* rotated -- needs the srcRect affine overload
}

// ─── CameraSystem ────────────────────────────────────────────────────────────────

TEST(CameraSystemTest, NoActiveCameraResourceSet_LeavesTheRendererSCameraUntouched)
{
    Registry registry;
    RecordingRenderer renderer;

    asge::game::systems::CameraSystem(registry, renderer, 1.0f / 60.0f);

    EXPECT_FLOAT_EQ(renderer.GetCamera().m_X, 0.0f);
    EXPECT_FLOAT_EQ(renderer.GetCamera().m_Y, 0.0f);
    EXPECT_FLOAT_EQ(renderer.GetCamera().m_Zoom, 1.0f);
}

TEST(CameraSystemTest, ActiveCameraPointsAtEntityNull_LeavesTheRendererSCameraUntouched)
{
    Registry registry;
    RecordingRenderer renderer;
    registry.SetResource( ActiveCamera{} ); // default-constructed m_Entity is Entity::Null()

    asge::game::systems::CameraSystem(registry, renderer, 1.0f / 60.0f);

    EXPECT_FLOAT_EQ(renderer.GetCamera().m_X, 0.0f);
    EXPECT_FLOAT_EQ(renderer.GetCamera().m_Y, 0.0f);
}

TEST(CameraSystemTest, ActiveCameraEntityMissingCameraComponent_LeavesTheRendererSCameraUntouched)
{
    Registry registry;
    RecordingRenderer renderer;

    auto entity = registry.CreateEntity();
    ASSERT_TRUE(entity.IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), Transform{ .m_X = 500.0f, .m_Y = 500.0f }).IsOk());
    registry.SetResource( ActiveCamera{ entity.Value() } );

    asge::game::systems::CameraSystem(registry, renderer, 1.0f / 60.0f);

    EXPECT_FLOAT_EQ(renderer.GetCamera().m_X, 0.0f);
    EXPECT_FLOAT_EQ(renderer.GetCamera().m_Y, 0.0f);
}

TEST(CameraSystemTest, ActiveCameraEntityMissingTransform_LeavesTheRendererSCameraUntouched)
{
    Registry registry;
    RecordingRenderer renderer;

    auto entity = registry.CreateEntity();
    ASSERT_TRUE(entity.IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), Camera{ .m_Zoom = 2.0f }).IsOk());
    registry.SetResource( ActiveCamera{ entity.Value() } );

    asge::game::systems::CameraSystem(registry, renderer, 1.0f / 60.0f);

    EXPECT_FLOAT_EQ(renderer.GetCamera().m_Zoom, 1.0f); // still the default -- never touched
}

TEST(CameraSystemTest, ZeroSmoothing_SnapsStraightToTheTargetEntityCenteredInTheViewport)
{
    Registry registry;
    RecordingRenderer renderer; // default viewport {0,0,800,600}

    auto entity = registry.CreateEntity();
    ASSERT_TRUE(entity.IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), Transform{ .m_X = 500.0f, .m_Y = 300.0f }).IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), Camera{ .m_Zoom = 1.0f, .m_Smoothing = 0.0f }).IsOk());
    registry.SetResource( ActiveCamera{ entity.Value() } );

    asge::game::systems::CameraSystem(registry, renderer, 1.0f / 60.0f);

    // Centered: entity position minus half the viewport, at zoom 1.
    EXPECT_FLOAT_EQ(renderer.GetCamera().m_X, 100.0f);  // 500 - 800/2
    EXPECT_FLOAT_EQ(renderer.GetCamera().m_Y, 0.0f);    // 300 - 600/2
    EXPECT_FLOAT_EQ(renderer.GetCamera().m_Zoom, 1.0f);
}

TEST(CameraSystemTest, ZeroSmoothing_ZoomNarrowsHowMuchViewportIsSubtracted)
{
    Registry registry;
    RecordingRenderer renderer; // default viewport {0,0,800,600}

    auto entity = registry.CreateEntity();
    ASSERT_TRUE(entity.IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), Transform{ .m_X = 500.0f, .m_Y = 300.0f }).IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), Camera{ .m_Zoom = 2.0f, .m_Smoothing = 0.0f }).IsOk());
    registry.SetResource( ActiveCamera{ entity.Value() } );

    asge::game::systems::CameraSystem(registry, renderer, 1.0f / 60.0f);

    EXPECT_FLOAT_EQ(renderer.GetCamera().m_X, 300.0f); // 500 - 800/(2*2)
    EXPECT_FLOAT_EQ(renderer.GetCamera().m_Y, 150.0f); // 300 - 600/(2*2)
    EXPECT_FLOAT_EQ(renderer.GetCamera().m_Zoom, 2.0f);
}

TEST(CameraSystemTest, PositiveSmoothing_EasesPartwayTowardTheTargetInsteadOfSnapping)
{
    Registry registry;
    RecordingRenderer renderer; // default viewport {0,0,800,600}
    renderer.SetCamera( asge::video::Camera{ .m_X = 0.0f, .m_Y = 0.0f, .m_Zoom = 1.0f } );

    auto entity = registry.CreateEntity();
    ASSERT_TRUE(entity.IsOk());
    // Target center: 500 - 800/2 = 100 on X, 300 - 600/2 = 0 on Y.
    ASSERT_TRUE(registry.AddComponent(entity.Value(), Transform{ .m_X = 500.0f, .m_Y = 300.0f }).IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), Camera{ .m_Zoom = 1.0f, .m_Smoothing = 4.0f }).IsOk());
    registry.SetResource( ActiveCamera{ entity.Value() } );

    float const dt = 1.0f / 60.0f;
    asge::game::systems::CameraSystem(registry, renderer, dt);

    float const k = 1.0f - std::exp( -4.0f * dt );
    EXPECT_FLOAT_EQ(renderer.GetCamera().m_X, 0.0f + (100.0f - 0.0f) * k);
    EXPECT_GT(renderer.GetCamera().m_X, 0.0f);   // moved toward the target...
    EXPECT_LT(renderer.GetCamera().m_X, 100.0f); // ...but hasn't snapped all the way there
}

TEST(CameraSystemTest, PositiveSmoothing_RepeatedTicksConvergeOnTheTarget)
{
    Registry registry;
    RecordingRenderer renderer;

    auto entity = registry.CreateEntity();
    ASSERT_TRUE(entity.IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), Transform{ .m_X = 500.0f, .m_Y = 300.0f }).IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), Camera{ .m_Zoom = 1.0f, .m_Smoothing = 10.0f }).IsOk());
    registry.SetResource( ActiveCamera{ entity.Value() } );

    for ( int i = 0; i < 300; ++i )
    {
        asge::game::systems::CameraSystem(registry, renderer, 1.0f / 60.0f);
    }

    EXPECT_NEAR(renderer.GetCamera().m_X, 100.0f, 0.01f); // 500 - 800/2
    EXPECT_NEAR(renderer.GetCamera().m_Y, 0.0f, 0.01f);   // 300 - 600/2
}

// ─── AnimationSystem ─────────────────────────────────────────────────────────────

namespace
{
// Wraps a plain frame list into an already-resolved Animation::m_Clip, the
// same shape asset::AssetManager::ResolveAssets would hand back -- these
// tests exercise AnimationSystem in isolation, so they build the resolved
// asset directly rather than going through a real VFS/AssetManager.
Animation::frame_table MakeClip(std::vector<asge::math::Rect> inFrames)
{
    using asge::game::asset::Asset;
    using asge::game::asset::FrameTable;
    return Asset<FrameTable>::Create( "test/clip.toml", FrameTable{ std::move(inFrames) } );
}
}

TEST(AnimationSystemTest, AdvancesToNextFrameOnceFrameDurationElapses)
{
    Registry registry;
    FakeTexture texture(asge::math::Int2{ 32, 32 });

    auto entity = registry.CreateEntity();
    ASSERT_TRUE(entity.IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), Sprite{ .m_Texture = &texture }).IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), Animation{
        .m_ClipPath = "test/clip.toml",
        .m_Clip = MakeClip({ asge::math::Rect{ 0.0f, 0.0f, 8.0f, 8.0f }, asge::math::Rect{ 8.0f, 0.0f, 8.0f, 8.0f } }),
        .m_FrameDuration = 0.1f
    }).IsOk());

    asge::game::systems::AnimationSystem(registry, 0.1f);

    auto animResult = registry.GetComponent<Animation>(entity.Value());
    auto const& anim = animResult.Value().get();
    EXPECT_EQ(anim.m_CurrentFrame, 1u);
    auto spriteResult = registry.GetComponent<Sprite>(entity.Value());
    auto const& sprite = spriteResult.Value().get();
    ASSERT_TRUE(sprite.m_SourceRect.has_value());
    EXPECT_FLOAT_EQ(sprite.m_SourceRect->m_X, 8.0f);
}

TEST(AnimationSystemTest, LoopingAnimationWrapsToFrameZeroPastTheLastFrame)
{
    Registry registry;
    FakeTexture texture(asge::math::Int2{ 32, 32 });

    auto entity = registry.CreateEntity();
    ASSERT_TRUE(entity.IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), Sprite{ .m_Texture = &texture }).IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), Animation{
        .m_ClipPath = "test/clip.toml",
        .m_Clip = MakeClip({ asge::math::Rect{ 0.0f, 0.0f, 8.0f, 8.0f }, asge::math::Rect{ 8.0f, 0.0f, 8.0f, 8.0f } }),
        .m_FrameDuration = 0.1f,
        .m_Loop = true
    }).IsOk());

    // Two full frame-durations from frame 0 lands back on frame 0.
    asge::game::systems::AnimationSystem(registry, 0.2f);

    auto animResult = registry.GetComponent<Animation>(entity.Value());
    auto const& anim = animResult.Value().get();
    EXPECT_EQ(anim.m_CurrentFrame, 0u);
    EXPECT_TRUE(anim.m_Playing);
}

TEST(AnimationSystemTest, NonLoopingAnimationClampsOnLastFrameAndStopsPlaying)
{
    Registry registry;
    FakeTexture texture(asge::math::Int2{ 32, 32 });

    auto entity = registry.CreateEntity();
    ASSERT_TRUE(entity.IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), Sprite{ .m_Texture = &texture }).IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), Animation{
        .m_ClipPath = "test/clip.toml",
        .m_Clip = MakeClip({ asge::math::Rect{ 0.0f, 0.0f, 8.0f, 8.0f }, asge::math::Rect{ 8.0f, 0.0f, 8.0f, 8.0f } }),
        .m_FrameDuration = 0.1f,
        .m_Loop = false
    }).IsOk());

    asge::game::systems::AnimationSystem(registry, 0.5f); // Well past the end.

    auto animResult = registry.GetComponent<Animation>(entity.Value());
    auto const& anim = animResult.Value().get();
    EXPECT_EQ(anim.m_CurrentFrame, 1u); // Clamped to the last frame, not wrapped.
    EXPECT_FALSE(anim.m_Playing);
}

TEST(AnimationSystemTest, NotPlayingAnimationIsNotAdvanced)
{
    Registry registry;
    FakeTexture texture(asge::math::Int2{ 32, 32 });

    auto entity = registry.CreateEntity();
    ASSERT_TRUE(entity.IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), Sprite{ .m_Texture = &texture }).IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), Animation{
        .m_ClipPath = "test/clip.toml",
        .m_Clip = MakeClip({ asge::math::Rect{ 0.0f, 0.0f, 8.0f, 8.0f }, asge::math::Rect{ 8.0f, 0.0f, 8.0f, 8.0f } }),
        .m_FrameDuration = 0.1f,
        .m_Playing = false
    }).IsOk());

    asge::game::systems::AnimationSystem(registry, 10.0f);

    auto animResult = registry.GetComponent<Animation>(entity.Value());
    auto const& anim = animResult.Value().get();
    EXPECT_EQ(anim.m_CurrentFrame, 0u);
    EXPECT_FLOAT_EQ(anim.m_ElapsedTime, 0.0f);
}

TEST(AnimationSystemTest, EmptyFramesListIsSkippedRatherThanCrashing)
{
    Registry registry;
    FakeTexture texture(asge::math::Int2{ 32, 32 });

    auto entity = registry.CreateEntity();
    ASSERT_TRUE(entity.IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), Sprite{ .m_Texture = &texture }).IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), Animation{
        .m_ClipPath = "test/clip.toml", .m_Clip = MakeClip({})
    }).IsOk());

    asge::game::systems::AnimationSystem(registry, 1.0f);

    auto spriteResult = registry.GetComponent<Sprite>(entity.Value());
    auto const& sprite = spriteResult.Value().get();
    EXPECT_FALSE(sprite.m_SourceRect.has_value());
}

TEST(AnimationSystemTest, UnresolvedClipIsSkippedRatherThanCrashing)
{
    // Regression guard: an entity whose Animation::m_Clip hasn't been
    // resolved yet (asset::AssetManager::ResolveAssets never ran, or ran
    // before this entity existed) must be left alone, not dereferenced.
    Registry registry;
    FakeTexture texture(asge::math::Int2{ 32, 32 });

    auto entity = registry.CreateEntity();
    ASSERT_TRUE(entity.IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), Sprite{ .m_Texture = &texture }).IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), Animation{
        .m_ClipPath = "test/clip.toml", .m_Clip = nullptr, .m_FrameDuration = 0.1f
    }).IsOk());

    asge::game::systems::AnimationSystem(registry, 1.0f);

    auto spriteResult = registry.GetComponent<Sprite>(entity.Value());
    auto const& sprite = spriteResult.Value().get();
    EXPECT_FALSE(sprite.m_SourceRect.has_value());
}

TEST(AnimationSystemTest, NullTextureIsSkipped)
{
    Registry registry;

    auto entity = registry.CreateEntity();
    ASSERT_TRUE(entity.IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), Sprite{ .m_Texture = nullptr }).IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), Animation{
        .m_ClipPath = "test/clip.toml",
        .m_Clip = MakeClip({ asge::math::Rect{ 0.0f, 0.0f, 8.0f, 8.0f } }),
        .m_FrameDuration = 0.1f
    }).IsOk());

    asge::game::systems::AnimationSystem(registry, 1.0f);

    auto animResult = registry.GetComponent<Animation>(entity.Value());
    auto const& anim = animResult.Value().get();
    EXPECT_EQ(anim.m_CurrentFrame, 0u);
}

TEST(AnimationSystemTest, NonPositiveFrameDurationIsSkippedRatherThanLoopingForever)
{
    // Regression guard: the advance loop is `while (elapsed >= duration)`,
    // so a zero/negative duration must be treated as "not animating" --
    // otherwise this call never returns.
    Registry registry;
    FakeTexture texture(asge::math::Int2{ 32, 32 });

    auto entity = registry.CreateEntity();
    ASSERT_TRUE(entity.IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), Sprite{ .m_Texture = &texture }).IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), Animation{
        .m_ClipPath = "test/clip.toml",
        .m_Clip = MakeClip({ asge::math::Rect{ 0.0f, 0.0f, 8.0f, 8.0f }, asge::math::Rect{ 8.0f, 0.0f, 8.0f, 8.0f } }),
        .m_FrameDuration = 0.0f
    }).IsOk());

    asge::game::systems::AnimationSystem(registry, 1.0f);

    auto animResult = registry.GetComponent<Animation>(entity.Value());
    auto const& anim = animResult.Value().get();
    EXPECT_EQ(anim.m_CurrentFrame, 0u);
}

TEST(AnimationSystemTest, LargeDeltaTimeStepsThroughMultipleFramesInOneCall)
{
    Registry registry;
    FakeTexture texture(asge::math::Int2{ 32, 32 });

    auto entity = registry.CreateEntity();
    ASSERT_TRUE(entity.IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), Sprite{ .m_Texture = &texture }).IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), Animation{
        .m_ClipPath = "test/clip.toml",
        .m_Clip = MakeClip({
            asge::math::Rect{ 0.0f, 0.0f, 8.0f, 8.0f },
            asge::math::Rect{ 8.0f, 0.0f, 8.0f, 8.0f },
            asge::math::Rect{ 16.0f, 0.0f, 8.0f, 8.0f }
        }),
        .m_FrameDuration = 0.1f,
        .m_Loop = true
    }).IsOk());

    // 0.35s / 0.1s per frame = 3 whole steps -> frame (0 + 3) % 3 == 0, 0.05s left over.
    asge::game::systems::AnimationSystem(registry, 0.35f);

    auto animResult = registry.GetComponent<Animation>(entity.Value());
    auto const& anim = animResult.Value().get();
    EXPECT_EQ(anim.m_CurrentFrame, 0u);
    EXPECT_NEAR(anim.m_ElapsedTime, 0.05f, 1e-5f);
}

// ─── RenderPipeline ──────────────────────────────────────────────────────────────

TEST(RenderPipelineTest, AdvancesAnimationThenDrawsTheUpdatedFrame)
{
    Registry registry;
    FakeTexture texture(asge::math::Int2{ 32, 32 });
    RecordingRenderer renderer;

    auto entity = registry.CreateEntity();
    ASSERT_TRUE(entity.IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), Transform{}).IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), Sprite{ .m_Texture = &texture }).IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), Animation{
        .m_ClipPath = "test/clip.toml",
        .m_Clip = MakeClip({ asge::math::Rect{ 0.0f, 0.0f, 8.0f, 8.0f }, asge::math::Rect{ 8.0f, 0.0f, 8.0f, 8.0f } }),
        .m_FrameDuration = 0.1f
    }).IsOk());

    asge::game::systems::RenderPipeline(registry, renderer, 0.1f);

    ASSERT_EQ(renderer.m_Calls.size(), 1u);
    EXPECT_TRUE(renderer.m_Calls[0].m_HadSourceRect);
    EXPECT_FLOAT_EQ(renderer.m_Calls[0].m_DestRect.m_Width, 8.0f); // The 2nd (advanced-to) frame's width.
}

}
