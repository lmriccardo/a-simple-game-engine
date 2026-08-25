#include <ASGE/Game/Systems/RenderSystem.hpp>
#include <ASGE/Core/ECS/Registry.hpp>

#include <gtest/gtest.h>

#include <vector>

namespace
{

using asge::Result;
using asge::ecs::Registry;
using asge::game::components::Sprite;
using asge::game::components::Transform;

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
        asge::math::Rect m_DestRect;
        bool m_HadSourceRect;
    };

    mutable std::vector<DrawCall> m_Calls;

    void Clear(asge::graphics::RGBA_Color const&) const override {}
    void DrawRect(asge::math::Rect const&, asge::graphics::RGBA_Color const&, bool) const override {}
    void DrawLine(asge::math::Float2 const&, asge::math::Float2 const&,
        asge::graphics::RGBA_Color const&) const override {}
    void DrawCircle(asge::math::Int2 const&, int, asge::graphics::RGBA_Color const&, bool) const override {}

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
    void DrawTextureAffine(asge::video::ITexture const&, asge::math::Float2 const&,
        asge::math::Float2 const&, asge::math::Float2 const&) const noexcept override {}
    void DrawString(asge::str::StringView, asge::graphics::Font const&, asge::video::ITexture&,
        asge::math::Float2 const&, asge::graphics::RGBA_Color const&) const noexcept override {}

    void Present() const override {}

    [[nodiscard]] std::unique_ptr<asge::video::ITexture> CreateTexture(
        asge::graphics::Image const&) const noexcept override { return nullptr; }

    [[nodiscard]] bool IsValid() const override { return true; }
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
    EXPECT_FLOAT_EQ(destRect.x, 10.0f);
    EXPECT_FLOAT_EQ(destRect.y, 20.0f);
    EXPECT_FLOAT_EQ(destRect.w, 128.0f); // 64 * 2
    EXPECT_FLOAT_EQ(destRect.h, 96.0f);  // 32 * 3
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
    EXPECT_FLOAT_EQ(destRect.x, 5.0f);
    EXPECT_FLOAT_EQ(destRect.y, 5.0f);
    // Must come from the 32x32 source cell * scale, not the 256x256 sheet.
    EXPECT_FLOAT_EQ(destRect.w, 64.0f); // 32 * 2
    EXPECT_FLOAT_EQ(destRect.h, 64.0f); // 32 * 2
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
        EXPECT_FLOAT_EQ(call.m_DestRect.w, expected);
        EXPECT_FLOAT_EQ(call.m_DestRect.h, expected);
    }
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

}
