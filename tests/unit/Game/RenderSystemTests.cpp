#include <ASGE/Game/Systems/RenderSystem.hpp>
#include <ASGE/Game/Components/Animation.hpp>
#include <ASGE/Game/Components/Camera.hpp>
#include <ASGE/Game/Components/Hierarchy.hpp>
#include <ASGE/Game/Components/RenderInfo.hpp>
#include <ASGE/Game/Components/UI/UIButton.hpp>
#include <ASGE/Game/Components/UI/UILabel.hpp>
#include <ASGE/Game/Components/UI/UICheckbox.hpp>
#include <ASGE/Game/Components/UI/UISlider.hpp>
#include <ASGE/Game/Components/UI/UIPanel.hpp>
#include <ASGE/Game/Components/UI/Common.hpp>
#include <ASGE/Game/Resources/ActiveCamera.hpp>
#include <ASGE/Game/Resources/HitEntry.hpp>
#include <ASGE/Core/ECS/Registry.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <filesystem>
#include <vector>

namespace
{

using asge::Result;
using asge::ecs::Entity;
using asge::ecs::Registry;
using asge::game::components::Animation;
using asge::game::components::AttachChild;
using asge::game::components::Camera;
using asge::game::components::RenderInfo;
using asge::game::components::Sprite;
using asge::game::components::Transform;
using asge::game::components::UIButton;
using asge::game::components::UICheckbox;
using asge::game::components::UISlider;
using asge::game::components::UIPanel;
using asge::game::components::UILabel;
using asge::game::components::VerticalAlign;
using asge::game::components::UIRect;
using asge::game::components::Interactable;
using asge::game::resources::ActiveCamera;
using asge::game::resources::HitEntry;
using asge::game::resources::UIHitList;

// Minimal ITexture stub that just reports a fixed size -- no SDL/GPU
// resource, so RenderSystem can be exercised without a real renderer.
class FakeTexture final : public asge::video::ITexture
{
public:
    explicit FakeTexture(asge::math::Int2 inSize) : m_Size(inSize) {}

    [[nodiscard]] asge::math::Int2 Size() const noexcept override { return m_Size; }
    [[nodiscard]] void* NativeHandle() const noexcept override { return nullptr; }
    [[nodiscard]] bool IsValid() const noexcept override { return true; }

    void SetColorMod(asge::graphics::RGBA_Color) noexcept override {}

    [[nodiscard]] Result<asge::graphics::RGBA_Color> GetColorMod() const noexcept override
    {
        return Result<asge::graphics::RGBA_Color>::Ok(asge::graphics::RGBA_Color{});
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
        asge::video::Camera m_CameraAtDraw{}; // renderer's camera at the moment this call was made
    };

    // Recorded separately from DrawCall -- UIButton draws via DrawRect,
    // never DrawTexture*, so it needs its own color/fill assertions.
    struct RectCall { asge::math::Rect m_Rect; asge::graphics::RGBA_Color m_Color; bool m_Fill; };

    // UILabel draws via DrawString, never DrawTexture*/DrawRect either.
    struct StringCall { std::string m_Text; asge::math::Float2 m_Position; };

    // UISlider's thumb draws via DrawCircle.
    struct CircleCall { asge::math::Int2 m_Center; int m_Radius; asge::graphics::RGBA_Color m_Color; bool m_Fill; };

    mutable std::vector<CircleCall> m_CircleCalls;
    mutable std::vector<DrawCall> m_Calls;
    mutable std::vector<RectCall> m_RectCalls;
    mutable std::vector<StringCall> m_StringCalls;

    void Clear(asge::graphics::RGBA_Color const&) const override {}

    void DrawRect(asge::math::Rect const& inRect,
        asge::graphics::RGBA_Color const& inColor, bool inFill) const override
    {
        m_RectCalls.push_back({ inRect, inColor, inFill });
    }

    void DrawLine(asge::math::Float2 const&, asge::math::Float2 const&,
        asge::graphics::RGBA_Color const&) const override {}
    void DrawCircle(asge::math::Int2 const& inCenter, int inRadius,
        asge::graphics::RGBA_Color const& inColor, bool inFill) const override
    {
        m_CircleCalls.push_back({ inCenter, inRadius, inColor, inFill });
    }

    void DrawTexture(asge::video::ITexture const&, asge::math::Rect const& inDestRect) const noexcept override
    {
        m_Calls.push_back({ inDestRect, false, false, false, {}, {}, {}, m_Camera });
    }

    void DrawTexture(asge::video::ITexture const&, asge::math::Float2 const&) const noexcept override {}

    void DrawTexture(asge::video::ITexture const&, asge::math::Rect const&,
        asge::math::Rect const& inDestRect) const noexcept override
    {
        m_Calls.push_back({ inDestRect, true, false, false, {}, {}, {}, m_Camera });
    }

    void DrawTexture9Grid(asge::video::ITexture const&, float, float, float, float,
        asge::math::Rect const&) const noexcept override {}
    void DrawTextureTiled(asge::video::ITexture const&, float, asge::math::Rect const&) const noexcept override {}
    void DrawTextureAffine(asge::video::ITexture const&, asge::math::Float2 const& inOrigin,
        asge::math::Float2 const& inRight, asge::math::Float2 const& inDown) const noexcept override
    {
        m_Calls.push_back({ {}, false, true, false, inOrigin, inRight, inDown, m_Camera });
    }

    void DrawTextureAffine(asge::video::ITexture const&, asge::math::Rect const&, asge::math::Float2 const& inOrigin,
        asge::math::Float2 const& inRight, asge::math::Float2 const& inDown) const noexcept override
    {
        m_Calls.push_back({ {}, false, true, true, inOrigin, inRight, inDown, m_Camera });
    }
    void DrawString(asge::str::StringView inText, asge::media::Font const&, asge::video::ITexture&,
        asge::math::Float2 const& inPosition, asge::graphics::RGBA_Color const&) const noexcept override
    {
        m_StringCalls.push_back({ std::string(inText), inPosition });
    }

    void Present() const override {}

    [[nodiscard]] std::unique_ptr<asge::video::ITexture> CreateTexture(
        asge::media::Image const&) const noexcept override { return nullptr; }

    [[nodiscard]] bool IsValid() const override { return true; }
    [[nodiscard]] void* NativeHandle() const noexcept override { return nullptr; }

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
        Transform{ .m_WorldCoordinates = {10.0f, 20.0f}, .m_WorldScale = {2.0f, 3.0f} }).IsOk());
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
        Transform{ .m_WorldCoordinates = {5.0f, 5.0f}, .m_WorldScale = {2.0f, 2.0f} }).IsOk());
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
        Transform{ .m_WorldCoordinates = {0.0f, 0.0f}, .m_WorldScale = {1.0f, 1.0f} }).IsOk());
    ASSERT_TRUE(registry.AddComponent(cropped.Value(), Sprite{
        .m_Texture = &sheet,
        .m_SourceRect = asge::math::Rect{ 0.0f, 0.0f, 32.0f, 32.0f }
    }).IsOk());

    auto whole = registry.CreateEntity();
    ASSERT_TRUE(whole.IsOk());
    ASSERT_TRUE(registry.AddComponent(whole.Value(),
        Transform{ .m_WorldCoordinates = {0.0f, 0.0f}, .m_WorldScale = {1.0f, 1.0f} }).IsOk());
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
    // Draw order is driven by the RenderInfo component, not Sprite.
    Registry registry;
    FakeTexture texture(asge::math::Int2{ 10, 10 });
    RecordingRenderer renderer;

    auto high = registry.CreateEntity();
    ASSERT_TRUE(high.IsOk());
    ASSERT_TRUE(registry.AddComponent(high.Value(),
        Transform{ .m_WorldCoordinates = {100.0f, 0.0f} }).IsOk());
    ASSERT_TRUE(registry.AddComponent(high.Value(), Sprite{ .m_Texture = &texture }).IsOk());
    ASSERT_TRUE(registry.AddComponent(high.Value(), RenderInfo{ .m_Layer = 5 }).IsOk());

    auto low = registry.CreateEntity();
    ASSERT_TRUE(low.IsOk());
    ASSERT_TRUE(registry.AddComponent(low.Value(),
        Transform{ .m_WorldCoordinates = {200.0f, 0.0f} }).IsOk());
    ASSERT_TRUE(registry.AddComponent(low.Value(), Sprite{ .m_Texture = &texture }).IsOk());
    ASSERT_TRUE(registry.AddComponent(low.Value(), RenderInfo{ .m_Layer = 1 }).IsOk());

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
        Transform{ .m_WorldCoordinates = {2.0f, 10.0f} }).IsOk());
    ASSERT_TRUE(registry.AddComponent(front.Value(), Sprite{ .m_Texture = &texture }).IsOk());
    ASSERT_TRUE(registry.AddComponent(front.Value(), RenderInfo{ .m_YSort = true }).IsOk());

    auto back = registry.CreateEntity(); // Created first, but lower on screen -> drawn last
    ASSERT_TRUE(back.IsOk());
    ASSERT_TRUE(registry.AddComponent(back.Value(),
        Transform{ .m_WorldCoordinates = {1.0f, 100.0f} }).IsOk());
    ASSERT_TRUE(registry.AddComponent(back.Value(), Sprite{ .m_Texture = &texture }).IsOk());
    ASSERT_TRUE(registry.AddComponent(back.Value(), RenderInfo{ .m_YSort = true }).IsOk());

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
        Transform{ .m_WorldCoordinates = {1.0f, 10.0f} }).IsOk());
    ASSERT_TRUE(registry.AddComponent(first.Value(), Sprite{ .m_Texture = &texture }).IsOk());
    ASSERT_TRUE(registry.AddComponent(first.Value(), RenderInfo{ .m_YSort = true }).IsOk());

    auto second = registry.CreateEntity();
    ASSERT_TRUE(second.IsOk());
    ASSERT_TRUE(registry.AddComponent(second.Value(),
        Transform{ .m_WorldCoordinates = {2.0f, 10.0f} }).IsOk()); // Same bottom edge as `first`
    ASSERT_TRUE(registry.AddComponent(second.Value(), Sprite{ .m_Texture = &texture }).IsOk());
    ASSERT_TRUE(registry.AddComponent(second.Value(), RenderInfo{ .m_YSort = true }).IsOk());

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
        Transform{ .m_WorldCoordinates = {1.0f, 100.0f} }).IsOk());
    ASSERT_TRUE(registry.AddComponent(plain.Value(), Sprite{ .m_Texture = &texture }).IsOk());

    // Created second, opts into y-sort, and sits higher on screen.
    auto sorted = registry.CreateEntity();
    ASSERT_TRUE(sorted.IsOk());
    ASSERT_TRUE(registry.AddComponent(sorted.Value(),
        Transform{ .m_WorldCoordinates = {2.0f, 10.0f} }).IsOk());
    ASSERT_TRUE(registry.AddComponent(sorted.Value(), Sprite{ .m_Texture = &texture }).IsOk());
    ASSERT_TRUE(registry.AddComponent(sorted.Value(), RenderInfo{ .m_YSort = true }).IsOk());

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
        Transform{ .m_WorldCoordinates = {1.0f, 100.0f} }).IsOk());
    ASSERT_TRUE(registry.AddComponent(first.Value(),
        Sprite{ .m_Texture = &texture }).IsOk());

    auto second = registry.CreateEntity();
    ASSERT_TRUE(second.IsOk());
    ASSERT_TRUE(registry.AddComponent(second.Value(),
        Transform{ .m_WorldCoordinates = {2.0f, 10.0f} }).IsOk());
    ASSERT_TRUE(registry.AddComponent(second.Value(),
        Sprite{ .m_Texture = &texture }).IsOk());

    asge::game::systems::RenderSystem(registry, renderer);

    ASSERT_EQ(renderer.m_Calls.size(), 2u);
    EXPECT_FLOAT_EQ(renderer.m_Calls[0].m_DestRect.m_X, 1.0f);
    EXPECT_FLOAT_EQ(renderer.m_Calls[1].m_DestRect.m_X, 2.0f);
}

// ─── RenderSystem — screen space ────────────────────────────────────────────────

TEST(RenderSystemTest, ScreenSpace_DrawsAfterWorldSpaceRegardlessOfLayer)
{
    Registry registry;
    FakeTexture texture(asge::math::Int2{ 10, 10 });
    RecordingRenderer renderer;

    // Low world-space layer, but screen space always sorts last.
    auto world = registry.CreateEntity();
    ASSERT_TRUE(world.IsOk());
    ASSERT_TRUE(registry.AddComponent(world.Value(),
        Transform{ .m_WorldCoordinates = {1.0f, 0.0f} }).IsOk());
    ASSERT_TRUE(registry.AddComponent(world.Value(), Sprite{ .m_Texture = &texture }).IsOk());
    ASSERT_TRUE(registry.AddComponent(world.Value(), RenderInfo{ .m_Layer = -100 }).IsOk());

    auto screen = registry.CreateEntity();
    ASSERT_TRUE(screen.IsOk());
    ASSERT_TRUE(registry.AddComponent(screen.Value(),
        Transform{ .m_WorldCoordinates = {2.0f, 0.0f} }).IsOk());
    ASSERT_TRUE(registry.AddComponent(screen.Value(), Sprite{ .m_Texture = &texture }).IsOk());
    ASSERT_TRUE(registry.AddComponent(screen.Value(), RenderInfo{ .m_Layer = 100, .m_ScreenSpace = true }).IsOk());

    asge::game::systems::RenderSystem(registry, renderer);

    ASSERT_EQ(renderer.m_Calls.size(), 2u);
    EXPECT_FLOAT_EQ(renderer.m_Calls[0].m_DestRect.m_X, 1.0f); // world-space first...
    EXPECT_FLOAT_EQ(renderer.m_Calls[1].m_DestRect.m_X, 2.0f); // ...screen-space on top
}

TEST(RenderSystemTest, ScreenSpace_BypassesCulling_DrawnEvenFarOutsideTheViewport)
{
    Registry registry;
    FakeTexture texture(asge::math::Int2{ 32, 32 });
    RecordingRenderer renderer; // visible world rect is {0,0,800,600}

    auto entity = registry.CreateEntity();
    ASSERT_TRUE(entity.IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), Transform{ .m_WorldCoordinates = {5000.0f, 5000.0f} }).IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), Sprite{ .m_Texture = &texture }).IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), RenderInfo{ .m_ScreenSpace = true }).IsOk());

    asge::game::systems::RenderSystem(registry, renderer);

    EXPECT_EQ(renderer.m_Calls.size(), 1u);
}

TEST(RenderSystemTest, ScreenSpace_DrawsWithAnOriginZoomOneCameraThenRestoresTheWorldCamera)
{
    Registry registry;
    FakeTexture texture(asge::math::Int2{ 10, 10 });
    RecordingRenderer renderer;
    asge::video::Camera const worldCamera{ .m_X = 300.0f, .m_Y = 150.0f, .m_Zoom = 2.0f };
    renderer.SetCamera( worldCamera );

    auto world = registry.CreateEntity();
    ASSERT_TRUE(world.IsOk());
    ASSERT_TRUE(registry.AddComponent(world.Value(),
        Transform{ .m_WorldCoordinates = {300.0f, 150.0f} }).IsOk()); // inside view once the world camera is applied
    ASSERT_TRUE(registry.AddComponent(world.Value(), Sprite{ .m_Texture = &texture }).IsOk());

    auto screen = registry.CreateEntity();
    ASSERT_TRUE(screen.IsOk());
    ASSERT_TRUE(registry.AddComponent(screen.Value(),
        Transform{ .m_WorldCoordinates = {2.0f, 0.0f} }).IsOk());
    ASSERT_TRUE(registry.AddComponent(screen.Value(), Sprite{ .m_Texture = &texture }).IsOk());
    ASSERT_TRUE(registry.AddComponent(screen.Value(), RenderInfo{ .m_ScreenSpace = true }).IsOk());

    asge::game::systems::RenderSystem(registry, renderer);

    ASSERT_EQ(renderer.m_Calls.size(), 2u);
    // World-space item drawn under the world camera...
    EXPECT_FLOAT_EQ(renderer.m_Calls[0].m_CameraAtDraw.m_X, worldCamera.m_X);
    EXPECT_FLOAT_EQ(renderer.m_Calls[0].m_CameraAtDraw.m_Zoom, worldCamera.m_Zoom);
    // ...screen-space item drawn under an origin, zoom-1 camera instead.
    EXPECT_FLOAT_EQ(renderer.m_Calls[1].m_CameraAtDraw.m_X, 0.0f);
    EXPECT_FLOAT_EQ(renderer.m_Calls[1].m_CameraAtDraw.m_Y, 0.0f);
    EXPECT_FLOAT_EQ(renderer.m_Calls[1].m_CameraAtDraw.m_Zoom, 1.0f);
    // CameraSystem smooths from GetCamera() next frame -- must not be left on the screen camera.
    EXPECT_FLOAT_EQ(renderer.GetCamera().m_X, worldCamera.m_X);
    EXPECT_FLOAT_EQ(renderer.GetCamera().m_Y, worldCamera.m_Y);
    EXPECT_FLOAT_EQ(renderer.GetCamera().m_Zoom, worldCamera.m_Zoom);
}

// ─── RenderSystem — RenderInfo hierarchy sort inheritance ──────────────────────

TEST(RenderSystemTest, InheritSortFromParent_ChildAdoptsParentsLayer)
{
    Registry registry;
    FakeTexture texture(asge::math::Int2{ 10, 10 });
    RecordingRenderer renderer;

    // Parent carries the layer but isn't itself drawn (no Sprite).
    auto parent = registry.CreateEntity();
    ASSERT_TRUE(parent.IsOk());
    ASSERT_TRUE(registry.AddComponent(parent.Value(), RenderInfo{ .m_Layer = 5 }).IsOk());

    auto child = registry.CreateEntity();
    ASSERT_TRUE(child.IsOk());
    ASSERT_TRUE(registry.AddComponent(child.Value(), Transform{ .m_WorldCoordinates = {1.0f, 0.0f} }).IsOk());
    ASSERT_TRUE(registry.AddComponent(child.Value(), Sprite{ .m_Texture = &texture }).IsOk());
    ASSERT_TRUE(registry.AddComponent(child.Value(), RenderInfo{ .m_InheritSortFromParent = true }).IsOk());
    AttachChild( registry, parent.Value(), child.Value() );

    auto sibling = registry.CreateEntity(); // own low layer, drawn before the inherited-layer-5 child
    ASSERT_TRUE(sibling.IsOk());
    ASSERT_TRUE(registry.AddComponent(sibling.Value(), Transform{ .m_WorldCoordinates = {2.0f, 0.0f} }).IsOk());
    ASSERT_TRUE(registry.AddComponent(sibling.Value(), Sprite{ .m_Texture = &texture }).IsOk());
    ASSERT_TRUE(registry.AddComponent(sibling.Value(), RenderInfo{ .m_Layer = 1 }).IsOk());

    asge::game::systems::RenderSystem(registry, renderer);

    ASSERT_EQ(renderer.m_Calls.size(), 2u);
    EXPECT_FLOAT_EQ(renderer.m_Calls[0].m_DestRect.m_X, 2.0f); // sibling (layer 1) first...
    EXPECT_FLOAT_EQ(renderer.m_Calls[1].m_DestRect.m_X, 1.0f); // ...child (inherited layer 5) on top
}

TEST(RenderSystemTest, InheritSortFromParent_YSortUsesTheParentsPositionNotTheChildS)
{
    Registry registry;
    FakeTexture texture(asge::math::Int2{ 10, 10 }); // fixed 10-tall, so bottom edge = y + 10
    RecordingRenderer renderer;

    // Parent has a Transform (for its own sort position) but no Sprite --
    // ComputeOwnerSortY falls back to its bare Transform Y (10), not a bottom edge.
    auto parent = registry.CreateEntity();
    ASSERT_TRUE(parent.IsOk());
    ASSERT_TRUE(registry.AddComponent(parent.Value(), Transform{ .m_WorldCoordinates = {0.0f, 10.0f} }).IsOk());
    ASSERT_TRUE(registry.AddComponent(parent.Value(), RenderInfo{ .m_YSort = true }).IsOk());

    // Child sits far below its parent on screen (though still inside the
    // default viewport, so culling doesn't remove it) -- if inheritance
    // mistakenly used the child's own Transform, it would sort after
    // `sibling` instead of before it.
    auto child = registry.CreateEntity();
    ASSERT_TRUE(child.IsOk());
    ASSERT_TRUE(registry.AddComponent(child.Value(), Transform{ .m_WorldCoordinates = {1.0f, 500.0f} }).IsOk());
    ASSERT_TRUE(registry.AddComponent(child.Value(), Sprite{ .m_Texture = &texture }).IsOk());
    ASSERT_TRUE(registry.AddComponent(child.Value(), RenderInfo{ .m_InheritSortFromParent = true }).IsOk());
    AttachChild( registry, parent.Value(), child.Value() );

    auto sibling = registry.CreateEntity();
    ASSERT_TRUE(sibling.IsOk());
    ASSERT_TRUE(registry.AddComponent(sibling.Value(), Transform{ .m_WorldCoordinates = {2.0f, 50.0f} }).IsOk());
    ASSERT_TRUE(registry.AddComponent(sibling.Value(), Sprite{ .m_Texture = &texture }).IsOk());
    ASSERT_TRUE(registry.AddComponent(sibling.Value(), RenderInfo{ .m_YSort = true }).IsOk());

    asge::game::systems::RenderSystem(registry, renderer);

    ASSERT_EQ(renderer.m_Calls.size(), 2u);
    EXPECT_FLOAT_EQ(renderer.m_Calls[0].m_DestRect.m_X, 1.0f); // child, sorted by its parent's y=10...
    EXPECT_FLOAT_EQ(renderer.m_Calls[1].m_DestRect.m_X, 2.0f); // ...before the sibling's bottom edge=60
}

TEST(RenderSystemTest, InheritSortFromParent_SiblingsTieBreakByLocalOrder)
{
    Registry registry;
    FakeTexture texture(asge::math::Int2{ 10, 10 });
    RecordingRenderer renderer;

    auto parent = registry.CreateEntity();
    ASSERT_TRUE(parent.IsOk());
    ASSERT_TRUE(registry.AddComponent(parent.Value(), RenderInfo{}).IsOk());

    auto behind = registry.CreateEntity(); // created after `front`, but m_LocalOrder puts it behind
    ASSERT_TRUE(behind.IsOk());
    ASSERT_TRUE(registry.AddComponent(behind.Value(), Transform{ .m_WorldCoordinates = {1.0f, 0.0f} }).IsOk());
    ASSERT_TRUE(registry.AddComponent(behind.Value(), Sprite{ .m_Texture = &texture }).IsOk());
    ASSERT_TRUE(registry.AddComponent(behind.Value(),
        RenderInfo{ .m_InheritSortFromParent = true, .m_LocalOrder = -1 }).IsOk());

    auto front = registry.CreateEntity();
    ASSERT_TRUE(front.IsOk());
    ASSERT_TRUE(registry.AddComponent(front.Value(), Transform{ .m_WorldCoordinates = {2.0f, 0.0f} }).IsOk());
    ASSERT_TRUE(registry.AddComponent(front.Value(), Sprite{ .m_Texture = &texture }).IsOk());
    ASSERT_TRUE(registry.AddComponent(front.Value(),
        RenderInfo{ .m_InheritSortFromParent = true, .m_LocalOrder = 0 }).IsOk());

    AttachChild( registry, parent.Value(), behind.Value() );
    AttachChild( registry, parent.Value(), front.Value() );

    asge::game::systems::RenderSystem(registry, renderer);

    ASSERT_EQ(renderer.m_Calls.size(), 2u);
    EXPECT_FLOAT_EQ(renderer.m_Calls[0].m_DestRect.m_X, 1.0f); // m_LocalOrder -1 drawn first (behind)...
    EXPECT_FLOAT_EQ(renderer.m_Calls[1].m_DestRect.m_X, 2.0f); // ...m_LocalOrder 0 drawn on top
}

TEST(RenderSystemTest, ScreenSpace_PropagatesToChildrenRegardlessOfInheritSortFromParent)
{
    Registry registry;
    FakeTexture texture(asge::math::Int2{ 32, 32 });
    RecordingRenderer renderer; // visible world rect is {0,0,800,600}

    auto parent = registry.CreateEntity();
    ASSERT_TRUE(parent.IsOk());
    ASSERT_TRUE(registry.AddComponent(parent.Value(), RenderInfo{ .m_ScreenSpace = true }).IsOk());

    // Doesn't inherit layer/y-sort, but m_ScreenSpace must still propagate.
    auto child = registry.CreateEntity();
    ASSERT_TRUE(child.IsOk());
    ASSERT_TRUE(registry.AddComponent(child.Value(), Transform{ .m_WorldCoordinates = {5000.0f, 5000.0f} }).IsOk());
    ASSERT_TRUE(registry.AddComponent(child.Value(), Sprite{ .m_Texture = &texture }).IsOk());
    ASSERT_TRUE(registry.AddComponent(child.Value(), RenderInfo{}).IsOk());
    AttachChild( registry, parent.Value(), child.Value() );

    asge::game::systems::RenderSystem(registry, renderer);

    EXPECT_EQ(renderer.m_Calls.size(), 1u); // not culled, despite sitting far outside the viewport
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
    ASSERT_TRUE(registry.AddComponent(entity.Value(), Transform{ .m_WorldCoordinates = {100.0f, 100.0f} }).IsOk());
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
    ASSERT_TRUE(registry.AddComponent(entity.Value(), Transform{ .m_WorldCoordinates = {5000.0f, 5000.0f} }).IsOk());
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
    ASSERT_TRUE(registry.AddComponent(entity.Value(), Transform{ .m_WorldCoordinates = {790.0f, 100.0f} }).IsOk());
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
    ASSERT_TRUE(registry.AddComponent(nowOffscreen.Value(), Transform{ .m_WorldCoordinates = {0.0f, 0.0f} }).IsOk());
    ASSERT_TRUE(registry.AddComponent(nowOffscreen.Value(), Sprite{ .m_Texture = &texture }).IsOk());

    auto nowOnscreen = registry.CreateEntity();
    ASSERT_TRUE(nowOnscreen.IsOk());
    ASSERT_TRUE(registry.AddComponent(nowOnscreen.Value(), Transform{ .m_WorldCoordinates = {2050.0f, 2050.0f} }).IsOk());
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
    ASSERT_TRUE(registry.AddComponent(entity.Value(), Transform{ .m_WorldCoordinates = {100.0f, 100.0f}, .m_WorldRotation = 0.0f }).IsOk());
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
        Transform{ .m_WorldCoordinates = {100.0f, 100.0f}, .m_WorldRotation = 3.14159265358979323846f * 0.5f }).IsOk());
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
    ASSERT_TRUE(registry.AddComponent(entity.Value(), Transform{ .m_WorldCoordinates = {0.0f, 0.0f}, .m_WorldRotation = 0.7f }).IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(),
        Sprite{ .m_Texture = &texture, .m_SourceRect = asge::math::Rect{ 0.0f, 0.0f, 16.0f, 16.0f } }).IsOk());

    asge::game::systems::RenderSystem(registry, renderer);

    ASSERT_EQ(renderer.m_Calls.size(), 1u);
    EXPECT_TRUE(renderer.m_Calls[0].m_WasAffine);
    EXPECT_TRUE(renderer.m_Calls[0].m_AffineHadSourceRect); // cropped *and* rotated -- needs the srcRect affine overload
}

// ─── RenderSystem — UIRect / UIButton ────────────────────────────────────────────

TEST(RenderSystemTest, UIRect_DrawnAsAFilledRectSizedFromMSizeScaledByWorldScale)
{
    Registry registry;
    RecordingRenderer renderer;

    auto entity = registry.CreateEntity();
    ASSERT_TRUE(entity.IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(),
        Transform{ .m_WorldCoordinates = {10.0f, 20.0f}, .m_WorldScale = {2.0f, 3.0f} }).IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), UIRect{ .m_Size = {80.0f, 24.0f} }).IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), UIButton{}).IsOk()); // Draw(UIRect) needs a sibling UIButton for its fill color

    asge::game::systems::RenderSystem(registry, renderer);

    ASSERT_EQ(renderer.m_RectCalls.size(), 1u);
    auto const& call = renderer.m_RectCalls[0];
    EXPECT_TRUE(call.m_Fill);
    EXPECT_FLOAT_EQ(call.m_Rect.m_X, 10.0f);
    EXPECT_FLOAT_EQ(call.m_Rect.m_Y, 20.0f);
    EXPECT_FLOAT_EQ(call.m_Rect.m_Width, 160.0f);  // 80 * 2
    EXPECT_FLOAT_EQ(call.m_Rect.m_Height, 72.0f);  // 24 * 3
}

TEST(RenderSystemTest, UIRect_NoSiblingUIButton_DrawsNothing)
{
    // A UIRect (+ Interactable) with no UIButton is a valid composition --
    // an invisible but still hit-testable zone -- so it must not crash or
    // draw some fallback color.
    Registry registry;
    RecordingRenderer renderer;

    auto entity = registry.CreateEntity();
    ASSERT_TRUE(entity.IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), Transform{}).IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), UIRect{}).IsOk());

    asge::game::systems::RenderSystem(registry, renderer);

    EXPECT_TRUE(renderer.m_RectCalls.empty());
}

TEST(RenderSystemTest, UIRect_NeitherHoveredNorHeld_DrawnWithMColor)
{
    Registry registry;
    RecordingRenderer renderer;
    UIButton button;
    button.m_Colors.m_Color = { 10, 20, 30, 255 };

    auto entity = registry.CreateEntity();
    ASSERT_TRUE(entity.IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), Transform{}).IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), UIRect{}).IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), button).IsOk());

    asge::game::systems::RenderSystem(registry, renderer);

    ASSERT_EQ(renderer.m_RectCalls.size(), 1u);
    auto const& color = renderer.m_RectCalls[0].m_Color;
    EXPECT_EQ(color.r, 10);
    EXPECT_EQ(color.g, 20);
    EXPECT_EQ(color.b, 30);
}

TEST(RenderSystemTest, UIRect_Hovered_DrawnWithMHoverColor)
{
    Registry registry;
    RecordingRenderer renderer;
    UIButton button;
    button.m_Colors.m_HoverColor = { 40, 50, 60, 255 };

    auto entity = registry.CreateEntity();
    ASSERT_TRUE(entity.IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), Transform{}).IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), UIRect{}).IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), button).IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), Interactable{ .m_Hovered = true }).IsOk());

    asge::game::systems::RenderSystem(registry, renderer);

    ASSERT_EQ(renderer.m_RectCalls.size(), 1u);
    auto const& color = renderer.m_RectCalls[0].m_Color;
    EXPECT_EQ(color.r, 40);
    EXPECT_EQ(color.g, 50);
    EXPECT_EQ(color.b, 60);
}

TEST(RenderSystemTest, UIRect_HeldAndHovered_PressedColorTakesPriorityOverHoverColor)
{
    Registry registry;
    RecordingRenderer renderer;
    UIButton button;
    button.m_Colors.m_PressedColor = { 70, 80, 90, 255 };

    auto entity = registry.CreateEntity();
    ASSERT_TRUE(entity.IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), Transform{}).IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), UIRect{}).IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), button).IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), Interactable{ .m_Hovered = true, .m_Held = true }).IsOk());

    asge::game::systems::RenderSystem(registry, renderer);

    ASSERT_EQ(renderer.m_RectCalls.size(), 1u);
    auto const& color = renderer.m_RectCalls[0].m_Color;
    EXPECT_EQ(color.r, 70);
    EXPECT_EQ(color.g, 80);
    EXPECT_EQ(color.b, 90);
}

TEST(RenderSystemTest, UIRect_EntityAlsoHasASprite_OnlyTheSpriteIsDrawn)
{
    // ShouldExclude<UIRect> skips an entity that also has a Sprite -- a
    // scene author swapping a placeholder Sprite for a UI widget (or vice
    // versa) shouldn't end up with both drawn on top of each other.
    Registry registry;
    FakeTexture texture(asge::math::Int2{ 32, 32 });
    RecordingRenderer renderer;

    auto entity = registry.CreateEntity();
    ASSERT_TRUE(entity.IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), Transform{}).IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), Sprite{ .m_Texture = &texture }).IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), UIRect{}).IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), UIButton{}).IsOk());

    asge::game::systems::RenderSystem(registry, renderer);

    EXPECT_EQ(renderer.m_Calls.size(), 1u);     // the Sprite...
    EXPECT_TRUE(renderer.m_RectCalls.empty());  // ...not the UIRect
}

// ─── RenderSystem — UICheckbox ───────────────────────────────────────────────────

TEST(RenderSystemTest, UICheckbox_Unchecked_DrawsOnlyTheBoxRect)
{
    Registry registry;
    RecordingRenderer renderer;
    UICheckbox checkbox;
    checkbox.m_BoxColors.m_Color = { 10, 20, 30, 255 };
    checkbox.m_Checked = false;

    auto entity = registry.CreateEntity();
    ASSERT_TRUE(entity.IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), Transform{}).IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), UIRect{ .m_Size = {24.0f, 24.0f} }).IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), checkbox).IsOk());

    asge::game::systems::RenderSystem(registry, renderer);

    ASSERT_EQ(renderer.m_RectCalls.size(), 1u); // the box only -- no check mark
    auto const& box = renderer.m_RectCalls[0];
    EXPECT_TRUE(box.m_Fill);
    EXPECT_EQ(box.m_Color.r, 10);
    EXPECT_EQ(box.m_Color.g, 20);
    EXPECT_EQ(box.m_Color.b, 30);
}

TEST(RenderSystemTest, UICheckbox_Checked_DrawsBoxThenAnInsetCheckMarkRect)
{
    Registry registry;
    RecordingRenderer renderer;
    UICheckbox checkbox;
    checkbox.m_CheckColor = { 1, 2, 3, 255 };
    checkbox.m_Checked = true;

    auto entity = registry.CreateEntity();
    ASSERT_TRUE(entity.IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(),
        Transform{ .m_WorldCoordinates = {100.0f, 50.0f} }).IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), UIRect{ .m_Size = {24.0f, 24.0f} }).IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), checkbox).IsOk());

    asge::game::systems::RenderSystem(registry, renderer);

    ASSERT_EQ(renderer.m_RectCalls.size(), 2u); // box, then the check mark on top
    auto const& mark = renderer.m_RectCalls[1];
    EXPECT_TRUE(mark.m_Fill);
    EXPECT_EQ(mark.m_Color.r, 1);
    EXPECT_EQ(mark.m_Color.g, 2);
    EXPECT_EQ(mark.m_Color.b, 3);
    // Inset 15% of the 24x24 box on each side -- a 3.6px margin, 16.8x16.8 mark.
    EXPECT_FLOAT_EQ(mark.m_Rect.m_X, 103.6f);
    EXPECT_FLOAT_EQ(mark.m_Rect.m_Y, 53.6f);
    EXPECT_FLOAT_EQ(mark.m_Rect.m_Width, 16.8f);
    EXPECT_FLOAT_EQ(mark.m_Rect.m_Height, 16.8f);
}

TEST(RenderSystemTest, UICheckbox_HeldAndHovered_BoxUsesPressedColorLikeUIButton)
{
    // PickStateColor is shared with UIButton (see RenderSystem.cpp) --
    // regression coverage for UICheckbox actually being wired into the
    // UIWidgets dispatch tuple Draw(UIRect) walks, not just having its own
    // Draw() overload defined but never called.
    Registry registry;
    RecordingRenderer renderer;
    UICheckbox checkbox;
    checkbox.m_BoxColors.m_PressedColor = { 70, 80, 90, 255 };

    auto entity = registry.CreateEntity();
    ASSERT_TRUE(entity.IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), Transform{}).IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), UIRect{ .m_Size = {24.0f, 24.0f} }).IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), checkbox).IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), Interactable{ .m_Hovered = true, .m_Held = true }).IsOk());

    asge::game::systems::RenderSystem(registry, renderer);

    ASSERT_FALSE(renderer.m_RectCalls.empty());
    auto const& box = renderer.m_RectCalls[0];
    EXPECT_EQ(box.m_Color.r, 70);
    EXPECT_EQ(box.m_Color.g, 80);
    EXPECT_EQ(box.m_Color.b, 90);
}

// ─── RenderSystem — UISlider ─────────────────────────────────────────────────────

TEST(RenderSystemTest, UISlider_DrawsFilledTrackEmptyTrackAndThumbCircle)
{
    Registry registry;
    RecordingRenderer renderer;
    UISlider slider;
    slider.m_Value = 0.25f; // a quarter of 0..1
    slider.m_TrackColor = { 5, 6, 7, 255 };
    slider.m_ThumbColor.m_Color = { 40, 50, 60, 255 };

    auto entity = registry.CreateEntity();
    ASSERT_TRUE(entity.IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(),
        Transform{ .m_WorldCoordinates = {100.0f, 50.0f} }).IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), UIRect{ .m_Size = {200.0f, 20.0f} }).IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), slider).IsOk());

    asge::game::systems::RenderSystem(registry, renderer);

    // Bar inset 30% of the 20px height top and bottom -- 8px tall, at y = 56.
    ASSERT_EQ(renderer.m_RectCalls.size(), 2u);
    auto const& filled = renderer.m_RectCalls[0];
    EXPECT_TRUE(filled.m_Fill);
    EXPECT_EQ(filled.m_Color.r, 5);
    EXPECT_FLOAT_EQ(filled.m_Rect.m_X, 100.0f);
    EXPECT_FLOAT_EQ(filled.m_Rect.m_Y, 56.0f);
    EXPECT_FLOAT_EQ(filled.m_Rect.m_Width, 50.0f);
    EXPECT_FLOAT_EQ(filled.m_Rect.m_Height, 8.0f);

    auto const& empty = renderer.m_RectCalls[1];
    EXPECT_FALSE(empty.m_Fill);
    EXPECT_FLOAT_EQ(empty.m_Rect.m_X, 150.0f);
    EXPECT_FLOAT_EQ(empty.m_Rect.m_Width, 150.0f);

    ASSERT_EQ(renderer.m_CircleCalls.size(), 1u);
    auto const& thumb = renderer.m_CircleCalls[0];
    EXPECT_TRUE(thumb.m_Fill);
    EXPECT_EQ(thumb.m_Center.x(), 150);
    EXPECT_EQ(thumb.m_Center.y(), 60); // vertically centered in the rect
    EXPECT_EQ(thumb.m_Radius, 10);
    EXPECT_EQ(thumb.m_Color.r, 40);
}

TEST(RenderSystemTest, UISlider_HeldAndHovered_ThumbUsesPressedColor)
{
    Registry registry;
    RecordingRenderer renderer;
    UISlider slider;
    slider.m_ThumbColor.m_PressedColor = { 70, 80, 90, 255 };

    auto entity = registry.CreateEntity();
    ASSERT_TRUE(entity.IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), Transform{}).IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), UIRect{ .m_Size = {200.0f, 20.0f} }).IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), slider).IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), Interactable{ .m_Hovered = true, .m_Held = true }).IsOk());

    asge::game::systems::RenderSystem(registry, renderer);

    ASSERT_EQ(renderer.m_CircleCalls.size(), 1u);
    EXPECT_EQ(renderer.m_CircleCalls[0].m_Color.r, 70);
}

// ─── RenderSystem — UIPanel ──────────────────────────────────────────────────────

TEST(RenderSystemTest, UIPanel_WithBorder_DrawsFilledBackgroundThenOutlineOfFullRect)
{
    Registry registry;
    RecordingRenderer renderer;
    UIPanel panel;
    panel.m_Background = { 10, 20, 30, 255 };
    panel.m_BorderColor = { 200, 210, 220, 255 };
    panel.m_Margin = { 5.0f, 8.0f };
    panel.m_Border = true;

    auto entity = registry.CreateEntity();
    ASSERT_TRUE(entity.IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(),
        Transform{ .m_WorldCoordinates = {100.0f, 50.0f} }).IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), UIRect{ .m_Size = {200.0f, 100.0f} }).IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), panel).IsOk());

    asge::game::systems::RenderSystem(registry, renderer);

    ASSERT_EQ(renderer.m_RectCalls.size(), 2u);
    auto const& background = renderer.m_RectCalls[0];
    EXPECT_TRUE(background.m_Fill);
    EXPECT_EQ(background.m_Color.r, 10);
    EXPECT_FLOAT_EQ(background.m_Rect.m_X, 105.0f);
    EXPECT_FLOAT_EQ(background.m_Rect.m_Y, 58.0f);
    EXPECT_FLOAT_EQ(background.m_Rect.m_Width, 190.0f);
    EXPECT_FLOAT_EQ(background.m_Rect.m_Height, 84.0f);

    auto const& border = renderer.m_RectCalls[1];
    EXPECT_FALSE(border.m_Fill);
    EXPECT_EQ(border.m_Color.r, 200);
    EXPECT_FLOAT_EQ(border.m_Rect.m_X, 100.0f);
    EXPECT_FLOAT_EQ(border.m_Rect.m_Y, 50.0f);
    EXPECT_FLOAT_EQ(border.m_Rect.m_Width, 200.0f);
    EXPECT_FLOAT_EQ(border.m_Rect.m_Height, 100.0f);
}

TEST(RenderSystemTest, UIPanel_WithoutBorder_DrawsOnlyTheBackground)
{
    Registry registry;
    RecordingRenderer renderer;
    UIPanel panel;
    panel.m_Border = false;

    auto entity = registry.CreateEntity();
    ASSERT_TRUE(entity.IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), Transform{}).IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), UIRect{ .m_Size = {200.0f, 100.0f} }).IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), panel).IsOk());

    asge::game::systems::RenderSystem(registry, renderer);

    ASSERT_EQ(renderer.m_RectCalls.size(), 1u);
    EXPECT_TRUE(renderer.m_RectCalls[0].m_Fill);
}

// ─── RenderSystem — UIHitList ───────────────────────────────────────────────────

TEST(RenderSystemTest, UIHitList_ResourceNotSet_RenderSystemDoesNotCrashOrCreateIt)
{
    Registry registry;
    RecordingRenderer renderer;

    auto entity = registry.CreateEntity();
    ASSERT_TRUE(entity.IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), Transform{}).IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), UIRect{}).IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), Interactable{}).IsOk());

    asge::game::systems::RenderSystem(registry, renderer);

    EXPECT_FALSE(registry.GetResource<UIHitList>().IsOk());
}

TEST(RenderSystemTest, UIHitList_UIRectAndEnabledInteractable_GetsOneEntrySizedFromMSize)
{
    // No UIButton needed -- hit-testing only cares about UIRect (footprint)
    // and Interactable (opt-in), not what's actually drawn.
    Registry registry;
    RecordingRenderer renderer;
    registry.SetResource( UIHitList{} );

    auto entity = registry.CreateEntity();
    ASSERT_TRUE(entity.IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(),
        Transform{ .m_WorldCoordinates = {10.0f, 20.0f} }).IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), UIRect{ .m_Size = {80.0f, 24.0f} }).IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), Interactable{}).IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), RenderInfo{ .m_ScreenSpace = true }).IsOk());

    asge::game::systems::RenderSystem(registry, renderer);

    auto hitList = registry.GetResource<UIHitList>();
    ASSERT_TRUE(hitList.IsOk());
    ASSERT_EQ(hitList.Value().get().m_Entries.size(), 1u);
    auto const& hit = hitList.Value().get().m_Entries[0];
    EXPECT_EQ(hit.m_Entity, entity.Value());
    EXPECT_TRUE(hit.m_ScreenSpace);
    EXPECT_FLOAT_EQ(hit.m_Rect.m_X, 10.0f);
    EXPECT_FLOAT_EQ(hit.m_Rect.m_Y, 20.0f);
    EXPECT_FLOAT_EQ(hit.m_Rect.m_Width, 80.0f);
    EXPECT_FLOAT_EQ(hit.m_Rect.m_Height, 24.0f);
}

TEST(RenderSystemTest, UIHitList_ButtonAlsoCarryingASprite_StillGetsAHitEntry)
{
    // Even though ShouldExclude<UIRect> means only the Sprite is drawn (see
    // the UIRect_EntityAlsoHasASprite_OnlyTheSpriteIsDrawn test above), the
    // entity is still clickable -- CollectHitList keys off owning a UIRect
    // + enabled Interactable, not off which DrawItem got produced.
    Registry registry;
    FakeTexture texture(asge::math::Int2{ 32, 32 });
    RecordingRenderer renderer;
    registry.SetResource( UIHitList{} );

    auto entity = registry.CreateEntity();
    ASSERT_TRUE(entity.IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), Transform{}).IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), Sprite{ .m_Texture = &texture }).IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), UIRect{ .m_Size = {32.0f, 32.0f} }).IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), Interactable{}).IsOk());

    asge::game::systems::RenderSystem(registry, renderer);

    auto hitList = registry.GetResource<UIHitList>();
    ASSERT_TRUE(hitList.IsOk());
    ASSERT_EQ(hitList.Value().get().m_Entries.size(), 1u);
    EXPECT_EQ(hitList.Value().get().m_Entries[0].m_Entity, entity.Value());
}

TEST(RenderSystemTest, UIHitList_EntityWithNoInteractable_NeverAddsAHitEntry)
{
    Registry registry;
    RecordingRenderer renderer;
    registry.SetResource( UIHitList{} );

    auto entity = registry.CreateEntity();
    ASSERT_TRUE(entity.IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), Transform{}).IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), UIRect{}).IsOk());

    asge::game::systems::RenderSystem(registry, renderer);

    EXPECT_TRUE(registry.GetResource<UIHitList>().Value().get().m_Entries.empty());
}

TEST(RenderSystemTest, UIHitList_DisabledInteractable_NeverAddsAHitEntry)
{
    Registry registry;
    RecordingRenderer renderer;
    registry.SetResource( UIHitList{} );

    auto entity = registry.CreateEntity();
    ASSERT_TRUE(entity.IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), Transform{}).IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), UIRect{}).IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), Interactable{ .m_Enabled = false }).IsOk());

    asge::game::systems::RenderSystem(registry, renderer);

    EXPECT_TRUE(registry.GetResource<UIHitList>().Value().get().m_Entries.empty());
}

TEST(RenderSystemTest, UIHitList_RebuiltEveryCall_StalePreviousFrameEntriesDoNotLinger)
{
    Registry registry;
    RecordingRenderer renderer;
    registry.SetResource( UIHitList{} );

    auto first = registry.CreateEntity();
    ASSERT_TRUE(first.IsOk());
    ASSERT_TRUE(registry.AddComponent(first.Value(), Transform{}).IsOk());
    ASSERT_TRUE(registry.AddComponent(first.Value(), UIRect{}).IsOk());
    ASSERT_TRUE(registry.AddComponent(first.Value(), Interactable{}).IsOk());
    asge::game::systems::RenderSystem(registry, renderer);
    ASSERT_EQ(registry.GetResource<UIHitList>().Value().get().m_Entries.size(), 1u);

    // `first` no longer has a Transform, so it drops out of Collect<UIRect>
    // entirely -- its stale entry from the call above must not survive.
    ASSERT_TRUE(registry.RemoveComponent<Transform>( first.Value() ).IsOk());
    asge::game::systems::RenderSystem(registry, renderer);

    EXPECT_TRUE(registry.GetResource<UIHitList>().Value().get().m_Entries.empty());
}

// ─── RenderSystem — UILabel text alignment ───────────────────────────────────────

namespace
{
// Ahem.ttf (tests/support/fonts/, see NOTICE.md): a real TTF is needed for
// a real Font::Load bake (see FontTests.cpp's own doc comment). Font::Measure
// itself has its own coverage in FontTests.cpp -- these tests just need it
// to compute the expected pen position, the same way Draw(UILabel) does.
std::filesystem::path AhemPath()
{
    return std::filesystem::path(ASGE_TEST_FONTS_DIR) / "Ahem.ttf";
}
}

TEST(RenderSystemTest, UILabel_LeftAlign_DrawnAtRectsLeftEdge)
{
    Registry registry;
    RecordingRenderer renderer;

    auto fontResult = asge::media::Font::Load( AhemPath(), 20 );
    ASSERT_TRUE(fontResult.IsOk());
    asge::media::Font font = std::move(fontResult).Value();
    FakeTexture atlasTexture( asge::math::Int2{ 8, 8 } );

    auto entity = registry.CreateEntity();
    ASSERT_TRUE(entity.IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(),
        Transform{ .m_WorldCoordinates = {100.0f, 50.0f} }).IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), UIRect{ .m_Size = {200.0f, 40.0f} }).IsOk());

    UILabel label;
    label.m_Text = "Hi";
    label.m_Align = asge::str::TextAlign::Left;
    label.m_Font = &font;
    label.m_Texture = &atlasTexture;
    ASSERT_TRUE(registry.AddComponent(entity.Value(), label).IsOk());

    asge::game::systems::RenderSystem(registry, renderer);

    ASSERT_EQ(renderer.m_StringCalls.size(), 1u);
    EXPECT_EQ(renderer.m_StringCalls[0].m_Text, "Hi");
    EXPECT_FLOAT_EQ(renderer.m_StringCalls[0].m_Position.x(), 100.0f); // rect's left edge, untouched
}

TEST(RenderSystemTest, UILabel_CenterAlign_DrawnHalfwayIntoTheRectsSlack)
{
    // Regression test: RenderSystem.cpp previously ran inLabel.m_Text
    // through str::Justify(text, align, inItem.m_DstRect.m_Width) -- a
    // *character-count* padder, not a pixel one, so passing a pixel width
    // (e.g. 200.0f) as the target character count padded in far more space
    // glyphs than intended and pushed the text well past the button.
    Registry registry;
    RecordingRenderer renderer;

    auto fontResult = asge::media::Font::Load( AhemPath(), 20 );
    ASSERT_TRUE(fontResult.IsOk());
    asge::media::Font font = std::move(fontResult).Value();
    FakeTexture atlasTexture( asge::math::Int2{ 8, 8 } );

    auto entity = registry.CreateEntity();
    ASSERT_TRUE(entity.IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(),
        Transform{ .m_WorldCoordinates = {100.0f, 50.0f} }).IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), UIRect{ .m_Size = {200.0f, 40.0f} }).IsOk());

    UILabel label;
    label.m_Text = "Hi";
    label.m_Align = asge::str::TextAlign::Center;
    label.m_Font = &font;
    label.m_Texture = &atlasTexture;
    ASSERT_TRUE(registry.AddComponent(entity.Value(), label).IsOk());

    asge::game::systems::RenderSystem(registry, renderer);

    ASSERT_EQ(renderer.m_StringCalls.size(), 1u);
    EXPECT_EQ(renderer.m_StringCalls[0].m_Text, "Hi"); // unpadded -- not str::Justify's space-padded string
    float const textWidth = font.Measure( "Hi" ).x();
    float const expectedX = 100.0f + ( 200.0f - textWidth ) * 0.5f;
    EXPECT_NEAR(renderer.m_StringCalls[0].m_Position.x(), expectedX, 0.01f);
}

TEST(RenderSystemTest, UILabel_RightAlign_DrawnAtRectsRightEdgeMinusTextWidth)
{
    Registry registry;
    RecordingRenderer renderer;

    auto fontResult = asge::media::Font::Load( AhemPath(), 20 );
    ASSERT_TRUE(fontResult.IsOk());
    asge::media::Font font = std::move(fontResult).Value();
    FakeTexture atlasTexture( asge::math::Int2{ 8, 8 } );

    auto entity = registry.CreateEntity();
    ASSERT_TRUE(entity.IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(),
        Transform{ .m_WorldCoordinates = {100.0f, 50.0f} }).IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), UIRect{ .m_Size = {200.0f, 40.0f} }).IsOk());

    UILabel label;
    label.m_Text = "Hi";
    label.m_Align = asge::str::TextAlign::Right;
    label.m_Font = &font;
    label.m_Texture = &atlasTexture;
    ASSERT_TRUE(registry.AddComponent(entity.Value(), label).IsOk());

    asge::game::systems::RenderSystem(registry, renderer);

    ASSERT_EQ(renderer.m_StringCalls.size(), 1u);
    float const textWidth = font.Measure( "Hi" ).x();
    float const expectedX = 100.0f + ( 200.0f - textWidth );
    EXPECT_NEAR(renderer.m_StringCalls[0].m_Position.x(), expectedX, 0.01f);
}

TEST(RenderSystemTest, UILabel_TopVerticalAlign_DrawnAtRectsTopEdge)
{
    Registry registry;
    RecordingRenderer renderer;

    auto fontResult = asge::media::Font::Load( AhemPath(), 20 );
    ASSERT_TRUE(fontResult.IsOk());
    asge::media::Font font = std::move(fontResult).Value();
    FakeTexture atlasTexture( asge::math::Int2{ 8, 8 } );

    auto entity = registry.CreateEntity();
    ASSERT_TRUE(entity.IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(),
        Transform{ .m_WorldCoordinates = {100.0f, 50.0f} }).IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), UIRect{ .m_Size = {200.0f, 40.0f} }).IsOk());

    UILabel label;
    label.m_Text = "Hi";
    label.m_VerticalAlign = VerticalAlign::Top;
    label.m_Font = &font;
    label.m_Texture = &atlasTexture;
    ASSERT_TRUE(registry.AddComponent(entity.Value(), label).IsOk());

    asge::game::systems::RenderSystem(registry, renderer);

    ASSERT_EQ(renderer.m_StringCalls.size(), 1u);
    // Baseline sits GetAscent() below the rect's top edge -- DrawString's
    // position is the baseline, not a bounding-box corner (see Draw(UILabel)).
    EXPECT_FLOAT_EQ(renderer.m_StringCalls[0].m_Position.y(), 50.0f + static_cast<float>(font.GetAscent()));
}

TEST(RenderSystemTest, UILabel_CenterVerticalAlign_DrawnHalfwayIntoTheRectsVerticalSlack)
{
    Registry registry;
    RecordingRenderer renderer;

    auto fontResult = asge::media::Font::Load( AhemPath(), 20 );
    ASSERT_TRUE(fontResult.IsOk());
    asge::media::Font font = std::move(fontResult).Value();
    FakeTexture atlasTexture( asge::math::Int2{ 8, 8 } );

    auto entity = registry.CreateEntity();
    ASSERT_TRUE(entity.IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(),
        Transform{ .m_WorldCoordinates = {100.0f, 50.0f} }).IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), UIRect{ .m_Size = {200.0f, 40.0f} }).IsOk());

    UILabel label;
    label.m_Text = "Hi";
    label.m_VerticalAlign = VerticalAlign::Center; // also UILabel's own struct default
    label.m_Font = &font;
    label.m_Texture = &atlasTexture;
    ASSERT_TRUE(registry.AddComponent(entity.Value(), label).IsOk());

    asge::game::systems::RenderSystem(registry, renderer);

    ASSERT_EQ(renderer.m_StringCalls.size(), 1u);
    float const textHeight = font.Measure( "Hi" ).y();
    float const expectedY = 50.0f + ( 40.0f - textHeight ) * 0.5f + static_cast<float>(font.GetAscent());
    EXPECT_NEAR(renderer.m_StringCalls[0].m_Position.y(), expectedY, 0.01f);
}

TEST(RenderSystemTest, UILabel_BottomVerticalAlign_DrawnAtRectsBottomEdgeMinusTextHeight)
{
    Registry registry;
    RecordingRenderer renderer;

    auto fontResult = asge::media::Font::Load( AhemPath(), 20 );
    ASSERT_TRUE(fontResult.IsOk());
    asge::media::Font font = std::move(fontResult).Value();
    FakeTexture atlasTexture( asge::math::Int2{ 8, 8 } );

    auto entity = registry.CreateEntity();
    ASSERT_TRUE(entity.IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(),
        Transform{ .m_WorldCoordinates = {100.0f, 50.0f} }).IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), UIRect{ .m_Size = {200.0f, 40.0f} }).IsOk());

    UILabel label;
    label.m_Text = "Hi";
    label.m_VerticalAlign = VerticalAlign::Bottom;
    label.m_Font = &font;
    label.m_Texture = &atlasTexture;
    ASSERT_TRUE(registry.AddComponent(entity.Value(), label).IsOk());

    asge::game::systems::RenderSystem(registry, renderer);

    ASSERT_EQ(renderer.m_StringCalls.size(), 1u);
    float const textHeight = font.Measure( "Hi" ).y();
    float const expectedY = 50.0f + ( 40.0f - textHeight ) + static_cast<float>(font.GetAscent());
    EXPECT_NEAR(renderer.m_StringCalls[0].m_Position.y(), expectedY, 0.01f);
}

TEST(RenderSystemTest, UILabel_TextLongerThanItsRect_OverflowsRatherThanBeingClipped)
{
    // Current, documented limitation (see Draw(UILabel)'s doc comment):
    // there's no clipping yet, so a label wider than its UIRect just draws
    // past its edges instead of being cut off at them.
    Registry registry;
    RecordingRenderer renderer;

    auto fontResult = asge::media::Font::Load( AhemPath(), 20 );
    ASSERT_TRUE(fontResult.IsOk());
    asge::media::Font font = std::move(fontResult).Value();
    FakeTexture atlasTexture( asge::math::Int2{ 8, 8 } );

    auto entity = registry.CreateEntity();
    ASSERT_TRUE(entity.IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(),
        Transform{ .m_WorldCoordinates = {100.0f, 50.0f} }).IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), UIRect{ .m_Size = {10.0f, 40.0f} }).IsOk()); // far narrower than the text

    UILabel label;
    label.m_Text = "Way too long for this rect";
    label.m_Align = asge::str::TextAlign::Left;
    label.m_Font = &font;
    label.m_Texture = &atlasTexture;
    ASSERT_TRUE(registry.AddComponent(entity.Value(), label).IsOk());

    asge::game::systems::RenderSystem(registry, renderer);

    ASSERT_EQ(renderer.m_StringCalls.size(), 1u);
    EXPECT_EQ(renderer.m_StringCalls[0].m_Text, "Way too long for this rect"); // drawn whole, not truncated
    EXPECT_FLOAT_EQ(renderer.m_StringCalls[0].m_Position.x(), 100.0f); // still starts at the rect's left edge
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
    ASSERT_TRUE(registry.AddComponent(entity.Value(), Transform{ .m_WorldCoordinates = {500.0f, 500.0f} }).IsOk());
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
    ASSERT_TRUE(registry.AddComponent(entity.Value(), Transform{ .m_WorldCoordinates = {500.0f, 300.0f} }).IsOk());
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
    ASSERT_TRUE(registry.AddComponent(entity.Value(), Transform{ .m_WorldCoordinates = {500.0f, 300.0f} }).IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), Camera{ .m_Zoom = 2.0f, .m_Smoothing = 0.0f }).IsOk());
    registry.SetResource( ActiveCamera{ entity.Value() } );

    asge::game::systems::CameraSystem(registry, renderer, 1.0f / 60.0f);

    EXPECT_FLOAT_EQ(renderer.GetCamera().m_X, 300.0f); // 500 - 800/(2*2)
    EXPECT_FLOAT_EQ(renderer.GetCamera().m_Y, 150.0f); // 300 - 600/(2*2)
    EXPECT_FLOAT_EQ(renderer.GetCamera().m_Zoom, 2.0f);
}

TEST(CameraSystemTest, ZeroSmoothing_EntityWithResolvedSprite_CentersOnTheSpritesVisualMidpointNotTransformCorner)
{
    Registry registry;
    FakeTexture texture(asge::math::Int2{ 100, 60 });
    RecordingRenderer renderer; // default viewport {0,0,800,600}

    auto entity = registry.CreateEntity();
    ASSERT_TRUE(entity.IsOk());
    // Transform::m_WorldCoordinates is the sprite's dest-rect top-left
    // (SpriteGetDstRect), not its middle -- dest rect is {500,300,100,60},
    // so the visual center is (550,330), not the raw (500,300) Transform point.
    ASSERT_TRUE(registry.AddComponent(entity.Value(), Transform{ .m_WorldCoordinates = {500.0f, 300.0f} }).IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), Sprite{ .m_Texture = &texture }).IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), Camera{ .m_Zoom = 1.0f, .m_Smoothing = 0.0f }).IsOk());
    registry.SetResource( ActiveCamera{ entity.Value() } );

    asge::game::systems::CameraSystem(registry, renderer, 1.0f / 60.0f);

    EXPECT_FLOAT_EQ(renderer.GetCamera().m_X, 150.0f); // 550 - 800/2
    EXPECT_FLOAT_EQ(renderer.GetCamera().m_Y, 30.0f);  // 330 - 600/2
}

TEST(CameraSystemTest, ZeroSmoothing_EntityWithUnresolvedSprite_FallsBackToTheRawTransformPoint)
{
    Registry registry;
    RecordingRenderer renderer; // default viewport {0,0,800,600}

    auto entity = registry.CreateEntity();
    ASSERT_TRUE(entity.IsOk());
    // Sprite present but m_Texture is still null (not yet resolved) --
    // SpriteGetDstRect returns nullopt, so this must fall back to the raw
    // Transform point exactly like an entity with no Sprite at all.
    ASSERT_TRUE(registry.AddComponent(entity.Value(), Transform{ .m_WorldCoordinates = {500.0f, 300.0f} }).IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), Sprite{ .m_Texture = nullptr }).IsOk());
    ASSERT_TRUE(registry.AddComponent(entity.Value(), Camera{ .m_Zoom = 1.0f, .m_Smoothing = 0.0f }).IsOk());
    registry.SetResource( ActiveCamera{ entity.Value() } );

    asge::game::systems::CameraSystem(registry, renderer, 1.0f / 60.0f);

    EXPECT_FLOAT_EQ(renderer.GetCamera().m_X, 100.0f); // 500 - 800/2
    EXPECT_FLOAT_EQ(renderer.GetCamera().m_Y, 0.0f);   // 300 - 600/2
}

TEST(CameraSystemTest, PositiveSmoothing_EasesPartwayTowardTheTargetInsteadOfSnapping)
{
    Registry registry;
    RecordingRenderer renderer; // default viewport {0,0,800,600}
    renderer.SetCamera( asge::video::Camera{ .m_X = 0.0f, .m_Y = 0.0f, .m_Zoom = 1.0f } );

    auto entity = registry.CreateEntity();
    ASSERT_TRUE(entity.IsOk());
    // Target center: 500 - 800/2 = 100 on X, 300 - 600/2 = 0 on Y.
    ASSERT_TRUE(registry.AddComponent(entity.Value(), Transform{ .m_WorldCoordinates = {500.0f, 300.0f} }).IsOk());
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
    ASSERT_TRUE(registry.AddComponent(entity.Value(), Transform{ .m_WorldCoordinates = {500.0f, 300.0f} }).IsOk());
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
