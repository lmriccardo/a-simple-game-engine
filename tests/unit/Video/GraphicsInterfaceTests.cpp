#include <ASGE/Video/Graphics/Window.hpp>
#include <ASGE/Video/Graphics/Renderer.hpp>
#include <ASGE/Core/Math/Math.hpp>

#include <gtest/gtest.h>

#include <memory>

// These tests exercise IWindow/IRenderer purely through hand-written fake
// backends (no SDL involved). Their point is to prove the interfaces are a
// real seam a non-SDL backend could implement -- not to test SDL itself,
// which would require a real display/headless driver unavailable in CI.
namespace
{

using asge::video::IRenderer;
using asge::video::IWindow;
using asge::graphics::RGBA_Color;

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

class FakeRenderer final : public IRenderer
{
public:
    mutable int s_ClearCalls{0};
    mutable int s_DrawRectCalls{0};
    mutable int s_DrawLineCalls{0};
    mutable int s_DrawCircleCalls{0};
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

    void Present() const override { ++s_PresentCalls; }
    bool IsValid() const override { return s_Valid; }
};

TEST(GraphicsInterfaceTest, WindowIsUsablePolymorphicallyThroughIWindow)
{
    std::unique_ptr<IWindow> window = std::make_unique<FakeWindow>(asge::math::Int2{1280, 720}, true);

    EXPECT_TRUE(window->IsValid());
    EXPECT_EQ(window->Size().x(), 1280);
    EXPECT_EQ(window->Size().y(), 720);
    EXPECT_NE(window->NativeHandle(), nullptr);
}

TEST(GraphicsInterfaceTest, RendererIsUsablePolymorphicallyThroughIRenderer)
{
    std::unique_ptr<IRenderer> renderer = std::make_unique<FakeRenderer>(true);

    renderer->Clear(RGBA_Color{10, 20, 30, 255});
    renderer->DrawRect(asge::math::Rect{0.0F, 0.0F, 64.0F, 64.0F}, RGBA_Color{}, false);
    renderer->DrawLine(asge::math::Float2{0.0F, 0.0F}, asge::math::Float2{1.0F, 1.0F}, RGBA_Color{});
    renderer->DrawCircle(asge::math::Int2{0, 0}, 8, RGBA_Color{}, false);
    renderer->Present();

    auto const& fake = static_cast<FakeRenderer const&>(*renderer);
    EXPECT_EQ(fake.s_ClearCalls, 1);
    EXPECT_EQ(fake.s_DrawRectCalls, 1);
    EXPECT_EQ(fake.s_DrawLineCalls, 1);
    EXPECT_EQ(fake.s_DrawCircleCalls, 1);
    EXPECT_EQ(fake.s_PresentCalls, 1);
    EXPECT_EQ(fake.s_LastClearColor.r, 10);
    EXPECT_TRUE(renderer->IsValid());
}

TEST(GraphicsInterfaceTest, InvalidRendererReportsInvalid)
{
    FakeRenderer renderer(false);
    EXPECT_FALSE(renderer.IsValid());
}

} // namespace
