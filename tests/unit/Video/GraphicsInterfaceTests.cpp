#include <ASGE/Video/Graphics/Window.hpp>
#include <ASGE/Video/Graphics/Renderer.hpp>
#include <ASGE/Video/Graphics/Texture.hpp>
#include <ASGE/Core/Media/Image.hpp>
#include <ASGE/Core/Math/Math.hpp>

#include <gtest/gtest.h>

#include <memory>

// These tests exercise IWindow/IRenderer purely through hand-written fake
// backends (no SDL involved). Their point is to prove the interfaces are a
// real seam a non-SDL backend could implement -- not to test SDL itself,
// which would require a real display/headless driver unavailable in CI.
namespace
{

using asge::video::DrawTextureAnchored;
using asge::video::IRenderer;
using asge::video::ITexture;
using asge::video::IWindow;
using asge::media::Image;
using asge::media::PixelFormat;
using asge::media::RGBA_Color;

class FakeWindow final : public IWindow
{
private:
    asge::math::Int2 m_Size;
    bool m_Valid;

public:
    FakeWindow(asge::math::Int2 inSize, bool inValid)
    : m_Size(inSize), m_Valid(inValid)
    {}

    void* NativeHandle() const override { return const_cast<FakeWindow*>(this); }
    asge::math::Int2 Size() const override { return m_Size; }
    bool IsValid() const override { return m_Valid; }
};

class FakeTexture final : public ITexture
{
private:
    asge::math::Int2 m_Size;
    bool m_Valid;
    RGBA_Color m_ColorMod{255, 255, 255, 255};

public:
    FakeTexture(asge::math::Int2 inSize = {1, 1}, bool inValid = true)
    : m_Size(inSize), m_Valid(inValid)
    {}

    [[nodiscard]] asge::math::Int2 Size() const noexcept override { return m_Size; }
    [[nodiscard]] void* NativeHandle() const noexcept override { return const_cast<FakeTexture*>(this); }
    [[nodiscard]] bool IsValid() const noexcept override { return m_Valid; }

    void SetColorMod(RGBA_Color inColor) noexcept override { m_ColorMod = inColor; }
    [[nodiscard]] asge::Result<RGBA_Color> GetColorMod() const noexcept override
    {
        return asge::Result<RGBA_Color>::Ok(m_ColorMod);
    }
};

class FakeRenderer final : public IRenderer
{
public:
    mutable int s_ClearCalls{0};
    mutable int s_DrawRectCalls{0};
    mutable int s_DrawLineCalls{0};
    mutable int s_DrawCircleCalls{0};
    mutable int s_DrawTextureRectCalls{0};
    mutable int s_DrawTexturePositionCalls{0};
    mutable int s_DrawTextureSrcDestCalls{0};
    mutable asge::math::Rect s_LastSrcRect{};
    mutable asge::math::Rect s_LastDestRect{};
    mutable int s_DrawTexture9GridCalls{0};
    mutable int s_DrawTextureTiledCalls{0};
    mutable int s_DrawTextureAffineCalls{0};
    mutable int s_DrawStringCalls{0};
    mutable int s_CreateTextureCalls{0};
    mutable int s_PresentCalls{0};
    mutable RGBA_Color s_LastClearColor{};
    bool s_Valid;

    explicit FakeRenderer(bool inValid) : s_Valid(inValid) {}

    void Clear(RGBA_Color const& inColor) const override
    {
        ++s_ClearCalls;
        s_LastClearColor = inColor;
    }

    void DrawRect(asge::math::Rect const&, RGBA_Color const&, bool) const override
    {
        ++s_DrawRectCalls;
    }

    void DrawLine(asge::math::Float2 const&, asge::math::Float2 const&, RGBA_Color const&) const override
    {
        ++s_DrawLineCalls;
    }

    void DrawCircle(asge::math::Int2 const&, int, RGBA_Color const&, bool) const override
    {
        ++s_DrawCircleCalls;
    }

    void DrawTexture(ITexture const&, asge::math::Rect const&) const noexcept override
    {
        ++s_DrawTextureRectCalls;
    }

    void DrawTexture(ITexture const&, asge::math::Float2 const&) const noexcept override
    {
        ++s_DrawTexturePositionCalls;
    }

    void DrawTexture(ITexture const&, asge::math::Rect const& inSrcRect, asge::math::Rect const& inDestRect) const noexcept override
    {
        ++s_DrawTextureSrcDestCalls;
        s_LastSrcRect = inSrcRect;
        s_LastDestRect = inDestRect;
    }

    void DrawTexture9Grid(
        ITexture const&, float, float, float, float, asge::math::Rect const&
    ) const noexcept override
    {
        ++s_DrawTexture9GridCalls;
    }

    void DrawTextureTiled(ITexture const&, float, asge::math::Rect const&) const noexcept override
    {
        ++s_DrawTextureTiledCalls;
    }

    void DrawTextureAffine(
        ITexture const&, asge::math::Float2 const&, asge::math::Float2 const&, asge::math::Float2 const&
    ) const noexcept override
    {
        ++s_DrawTextureAffineCalls;
    }

    // Not exercised by RendererIsUsablePolymorphicallyThroughIRenderer below:
    // constructing a real graphics::Font needs actual TTF bytes via
    // Font::Load, which is out of scope for these no-SDL interface fakes.
    // The override existing at all is what keeps FakeRenderer instantiable.
    void DrawString(asge::str::StringView, asge::media::Font const&, ITexture&,
        asge::math::Float2 const&, RGBA_Color const&) const noexcept override
    {
        ++s_DrawStringCalls;
    }

    [[nodiscard]] std::unique_ptr<ITexture> CreateTexture(Image const&) const noexcept override
    {
        ++s_CreateTextureCalls;
        return std::make_unique<FakeTexture>();
    }

    void Present() const override { ++s_PresentCalls; }
    [[nodiscard]] bool IsValid() const override { return s_Valid; }
};

TEST(GraphicsInterfaceTest, WindowIsUsablePolymorphicallyThroughIWindow)
{
    std::unique_ptr<IWindow> window = std::make_unique<FakeWindow>(asge::math::Int2{1280, 720}, true);

    EXPECT_TRUE(window->IsValid());
    EXPECT_EQ(window->Size().x(), 1280);
    EXPECT_EQ(window->Size().y(), 720);
    EXPECT_NE(window->NativeHandle(), nullptr);
}

TEST(GraphicsInterfaceTest, TextureIsUsablePolymorphicallyThroughITexture)
{
    std::unique_ptr<ITexture> texture = std::make_unique<FakeTexture>(asge::math::Int2{64, 32}, true);

    EXPECT_TRUE(texture->IsValid());
    EXPECT_EQ(texture->Size().x(), 64);
    EXPECT_EQ(texture->Size().y(), 32);
    EXPECT_NE(texture->NativeHandle(), nullptr);
}

TEST(GraphicsInterfaceTest, RendererIsUsablePolymorphicallyThroughIRenderer)
{
    std::unique_ptr<IRenderer> renderer = std::make_unique<FakeRenderer>(true);

    Image::data_t pixels(1 * 1 * 4, std::uint8_t{0});
    Image image(1, 1, PixelFormat::RGBA8, pixels);
    auto texture = renderer->CreateTexture(image);
    ASSERT_NE(texture, nullptr);

    renderer->Clear(RGBA_Color{10, 20, 30, 255});
    renderer->DrawRect(asge::math::Rect{0.0F, 0.0F, 64.0F, 64.0F}, RGBA_Color{}, false);
    renderer->DrawLine(asge::math::Float2{0.0F, 0.0F}, asge::math::Float2{1.0F, 1.0F}, RGBA_Color{});
    renderer->DrawCircle(asge::math::Int2{0, 0}, 8, RGBA_Color{}, false);
    renderer->DrawTexture(*texture, asge::math::Rect{0.0F, 0.0F, 32.0F, 32.0F});
    renderer->DrawTexture(*texture, asge::math::Float2{0.0F, 0.0F});
    renderer->DrawTexture(*texture, asge::math::Rect{0.0F, 0.0F, 16.0F, 16.0F}, asge::math::Rect{0.0F, 0.0F, 32.0F, 32.0F});
    renderer->DrawTexture9Grid(*texture, 4.0F, 4.0F, 4.0F, 4.0F, asge::math::Rect{0.0F, 0.0F, 32.0F, 32.0F});
    renderer->DrawTextureTiled(*texture, 1.0F, asge::math::Rect{0.0F, 0.0F, 32.0F, 32.0F});
    renderer->DrawTextureAffine(*texture,
        asge::math::Float2{0.0F, 0.0F}, asge::math::Float2{32.0F, 0.0F}, asge::math::Float2{0.0F, 32.0F});
    renderer->Present();

    auto const& fake = static_cast<FakeRenderer const&>(*renderer);
    EXPECT_EQ(fake.s_ClearCalls, 1);
    EXPECT_EQ(fake.s_DrawRectCalls, 1);
    EXPECT_EQ(fake.s_DrawLineCalls, 1);
    EXPECT_EQ(fake.s_DrawCircleCalls, 1);
    EXPECT_EQ(fake.s_CreateTextureCalls, 1);
    EXPECT_EQ(fake.s_DrawTextureRectCalls, 1);
    EXPECT_EQ(fake.s_DrawTexturePositionCalls, 1);
    EXPECT_EQ(fake.s_DrawTextureSrcDestCalls, 1);
    EXPECT_EQ(fake.s_DrawTexture9GridCalls, 1);
    EXPECT_EQ(fake.s_DrawTextureTiledCalls, 1);
    EXPECT_EQ(fake.s_DrawTextureAffineCalls, 1);
    EXPECT_EQ(fake.s_PresentCalls, 1);
    EXPECT_EQ(fake.s_LastClearColor.r, 10);
    EXPECT_TRUE(renderer->IsValid());
}

TEST(GraphicsInterfaceTest, InvalidRendererReportsInvalid)
{
    FakeRenderer renderer(false);
    EXPECT_FALSE(renderer.IsValid());
}

// ─── DrawTextureAnchored ────────────────────────────────────────────────────

TEST(DrawTextureAnchoredTest, DefaultAnchor_ReproducesPlainDrawTexture)
{
    FakeRenderer renderer(true);
    FakeTexture texture;

    asge::math::Rect const src{4.0F, 8.0F, 16.0F, 24.0F};
    asge::math::Rect const dest{100.0F, 200.0F, 32.0F, 48.0F};

    DrawTextureAnchored(renderer, texture, src, dest);

    EXPECT_EQ(renderer.s_DrawTextureSrcDestCalls, 1);
    EXPECT_FLOAT_EQ(renderer.s_LastDestRect.x, dest.x);
    EXPECT_FLOAT_EQ(renderer.s_LastDestRect.y, dest.y);
    EXPECT_FLOAT_EQ(renderer.s_LastDestRect.w, dest.w);
    EXPECT_FLOAT_EQ(renderer.s_LastDestRect.h, dest.h);
}

TEST(DrawTextureAnchoredTest, CenterAnchor_OffsetsDestRectByHalfItsSize)
{
    FakeRenderer renderer(true);
    FakeTexture texture;

    asge::math::Rect const src{0.0F, 0.0F, 16.0F, 16.0F};
    asge::math::Rect const dest{100.0F, 200.0F, 40.0F, 20.0F};

    DrawTextureAnchored(renderer, texture, src, dest, asge::math::Float2{0.5F, 0.5F});

    EXPECT_FLOAT_EQ(renderer.s_LastDestRect.x, 80.0F);  // 100 - 0.5*40
    EXPECT_FLOAT_EQ(renderer.s_LastDestRect.y, 190.0F); // 200 - 0.5*20
    EXPECT_FLOAT_EQ(renderer.s_LastDestRect.w, 40.0F);
    EXPECT_FLOAT_EQ(renderer.s_LastDestRect.h, 20.0F);
}

TEST(DrawTextureAnchoredTest, BottomRightAnchor_OffsetsDestRectByItsFullSize)
{
    FakeRenderer renderer(true);
    FakeTexture texture;

    asge::math::Rect const src{0.0F, 0.0F, 16.0F, 16.0F};
    asge::math::Rect const dest{100.0F, 200.0F, 40.0F, 20.0F};

    DrawTextureAnchored(renderer, texture, src, dest, asge::math::Float2{1.0F, 1.0F});

    EXPECT_FLOAT_EQ(renderer.s_LastDestRect.x, 60.0F);  // 100 - 40
    EXPECT_FLOAT_EQ(renderer.s_LastDestRect.y, 180.0F); // 200 - 20
    EXPECT_FLOAT_EQ(renderer.s_LastDestRect.w, 40.0F);
    EXPECT_FLOAT_EQ(renderer.s_LastDestRect.h, 20.0F);
}

TEST(DrawTextureAnchoredTest, SrcRectIsPassedThroughUnchanged)
{
    // Only destRect is transformed by the anchor -- the source crop itself
    // (e.g. a frame's tight alpha-content bounds) must reach DrawTexture as-is.
    FakeRenderer renderer(true);
    FakeTexture texture;

    asge::math::Rect const src{4.0F, 8.0F, 16.0F, 24.0F};
    asge::math::Rect const dest{100.0F, 200.0F, 32.0F, 48.0F};

    DrawTextureAnchored(renderer, texture, src, dest, asge::math::Float2{0.5F, 1.0F});

    EXPECT_FLOAT_EQ(renderer.s_LastSrcRect.x, src.x);
    EXPECT_FLOAT_EQ(renderer.s_LastSrcRect.y, src.y);
    EXPECT_FLOAT_EQ(renderer.s_LastSrcRect.w, src.w);
    EXPECT_FLOAT_EQ(renderer.s_LastSrcRect.h, src.h);
}

} // namespace
