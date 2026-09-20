#include <ASGE/Game/Components/Sprite.hpp>
#include <ASGE/Video/Graphics/Texture.hpp>

#include <gtest/gtest.h>

#include <cmath>

namespace
{

using asge::game::components::Sprite;
using asge::game::components::SpriteDrawCorners;
using asge::game::components::SpriteGetDrawCorners;
using asge::game::components::SpriteGetDstRect;
using asge::game::components::Transform;

constexpr float kPi = 3.14159265358979323846f;

// Minimal ITexture stub reporting a fixed size -- no SDL/GPU resource
// needed for SpriteGetDstRect's own math (mirrors RenderSystemTests.cpp's
// FakeTexture).
class FakeTexture final : public asge::video::ITexture
{
public:
    explicit FakeTexture(asge::math::Int2 inSize) : m_Size(inSize) {}

    [[nodiscard]] asge::math::Int2 Size() const noexcept override { return m_Size; }
    [[nodiscard]] void* NativeHandle() const noexcept override { return nullptr; }
    [[nodiscard]] bool IsValid() const noexcept override { return true; }

    void SetColorMod(asge::media::RGBA_Color) noexcept override {}

    [[nodiscard]] asge::Result<asge::media::RGBA_Color> GetColorMod() const noexcept override
    {
        return asge::Result<asge::media::RGBA_Color>::Ok(asge::media::RGBA_Color{});
    }

private:
    asge::math::Int2 m_Size;
};

// ─── SpriteGetDstRect ────────────────────────────────────────────────────────

TEST(SpriteGetDstRectTest, NullTexture_ReturnsNullopt)
{
    Sprite const sprite{ .m_Texture = nullptr };
    Transform const transform{};

    EXPECT_FALSE(SpriteGetDstRect(sprite, transform).has_value());
}

TEST(SpriteGetDstRectTest, NoSourceRect_SizedFromFullTextureScaledByTransform)
{
    FakeTexture texture(asge::math::Int2{ 32, 16 });
    Sprite const sprite{ .m_Texture = &texture };
    Transform const transform{ .m_WorldCoordinates = {10.0f, 20.0f}, .m_WorldScale = {2.0f, 3.0f} };

    auto const dst = SpriteGetDstRect(sprite, transform);

    ASSERT_TRUE(dst.has_value());
    EXPECT_FLOAT_EQ(dst->m_X, 10.0f);
    EXPECT_FLOAT_EQ(dst->m_Y, 20.0f);
    EXPECT_FLOAT_EQ(dst->m_Width, 64.0f);  // 32 * 2
    EXPECT_FLOAT_EQ(dst->m_Height, 48.0f); // 16 * 3
}

TEST(SpriteGetDstRectTest, SourceRectSet_SizedFromSourceRectNotFullTexture)
{
    FakeTexture texture(asge::math::Int2{ 256, 256 }); // a big spritesheet
    Sprite const sprite{ .m_Texture = &texture, .m_SourceRect = asge::math::Rect{ 0.0f, 0.0f, 16.0f, 24.0f } };
    Transform const transform{ .m_WorldCoordinates = {5.0f, 5.0f}, .m_WorldScale = {1.0f, 1.0f} };

    auto const dst = SpriteGetDstRect(sprite, transform);

    ASSERT_TRUE(dst.has_value());
    EXPECT_FLOAT_EQ(dst->m_Width, 16.0f);  // cropped cell's own width, not the sheet's
    EXPECT_FLOAT_EQ(dst->m_Height, 24.0f);
}

// ─── SpriteGetDrawCorners ────────────────────────────────────────────────────

TEST(SpriteGetDrawCornersTest, ZeroRotation_ReproducesTheRectSOwnCorners)
{
    asge::math::Rect const dst{ 0.0f, 0.0f, 20.0f, 10.0f };

    SpriteDrawCorners const corners = SpriteGetDrawCorners(dst, 0.0f);

    EXPECT_FLOAT_EQ(corners.m_Origin.x(), 0.0f);  // top-left
    EXPECT_FLOAT_EQ(corners.m_Origin.y(), 0.0f);
    EXPECT_FLOAT_EQ(corners.m_Right.x(), 20.0f);  // top-right
    EXPECT_FLOAT_EQ(corners.m_Right.y(), 0.0f);
    EXPECT_FLOAT_EQ(corners.m_Down.x(), 0.0f);    // bottom-left
    EXPECT_FLOAT_EQ(corners.m_Down.y(), 10.0f);
}

TEST(SpriteGetDrawCornersTest, NinetyDegrees_RotatesClockwiseOnScreen)
{
    // Screen-space Y grows downward, so positive rotation reads as
    // clockwise -- the rect's own top-left corner (relative to center
    // (10,5): (-10,-5)) ends up at (15,-5) after a 90-degree turn.
    asge::math::Rect const dst{ 0.0f, 0.0f, 20.0f, 10.0f };

    SpriteDrawCorners const corners = SpriteGetDrawCorners(dst, kPi * 0.5f);

    EXPECT_NEAR(corners.m_Origin.x(), 15.0f, 1e-3f);
    EXPECT_NEAR(corners.m_Origin.y(), -5.0f, 1e-3f);
    EXPECT_NEAR(corners.m_Right.x(), 15.0f, 1e-3f);
    EXPECT_NEAR(corners.m_Right.y(), 15.0f, 1e-3f);
    EXPECT_NEAR(corners.m_Down.x(), 5.0f, 1e-3f);
    EXPECT_NEAR(corners.m_Down.y(), -5.0f, 1e-3f);
}

TEST(SpriteGetDrawCornersTest, OneEightyDegrees_ReflectsEachCornerThroughTheCenter)
{
    asge::math::Rect const dst{ 0.0f, 0.0f, 20.0f, 10.0f };

    SpriteDrawCorners const corners = SpriteGetDrawCorners(dst, kPi);

    // Origin (was top-left) lands where the rect's bottom-right corner was;
    // right (was top-right) lands at the old bottom-left; down (was
    // bottom-left) lands at the old top-right.
    EXPECT_NEAR(corners.m_Origin.x(), 20.0f, 1e-3f);
    EXPECT_NEAR(corners.m_Origin.y(), 10.0f, 1e-3f);
    EXPECT_NEAR(corners.m_Right.x(), 0.0f, 1e-3f);
    EXPECT_NEAR(corners.m_Right.y(), 10.0f, 1e-3f);
    EXPECT_NEAR(corners.m_Down.x(), 20.0f, 1e-3f);
    EXPECT_NEAR(corners.m_Down.y(), 0.0f, 1e-3f);
}

TEST(SpriteGetDrawCornersTest, TwoSeventyDegrees_RotatesClockwiseThreeQuarterTurns)
{
    asge::math::Rect const dst{ 0.0f, 0.0f, 20.0f, 10.0f };

    SpriteDrawCorners const corners = SpriteGetDrawCorners(dst, kPi * 1.5f);

    EXPECT_NEAR(corners.m_Origin.x(), 5.0f, 1e-3f);
    EXPECT_NEAR(corners.m_Origin.y(), 15.0f, 1e-3f);
    EXPECT_NEAR(corners.m_Right.x(), 5.0f, 1e-3f);
    EXPECT_NEAR(corners.m_Right.y(), -5.0f, 1e-3f);
    EXPECT_NEAR(corners.m_Down.x(), 15.0f, 1e-3f);
    EXPECT_NEAR(corners.m_Down.y(), 15.0f, 1e-3f);
}

TEST(SpriteGetDrawCornersTest, ArbitraryAngle_KeepsTheRectSCenterFixed)
{
    // (m_Right + m_Down) / 2 == the rect's own center for any rotation,
    // since m_Right and m_Down are diagonally opposite corners of the
    // parallelogram m_Origin/m_Right/m_Down/(implicit 4th corner) forms
    // around it -- true regardless of angle, so this doesn't depend on
    // hand-derived trig values the way the axis-aligned-angle tests above do.
    asge::math::Rect const dst{ 3.0f, 7.0f, 40.0f, 24.0f };
    float const centerX = dst.m_X + dst.m_Width * 0.5f;
    float const centerY = dst.m_Y + dst.m_Height * 0.5f;

    SpriteDrawCorners const corners = SpriteGetDrawCorners(dst, kPi / 6.0f); // 30 degrees

    EXPECT_NEAR((corners.m_Right.x() + corners.m_Down.x()) * 0.5f, centerX, 1e-3f);
    EXPECT_NEAR((corners.m_Right.y() + corners.m_Down.y()) * 0.5f, centerY, 1e-3f);
}

TEST(SpriteGetDrawCornersTest, ArbitraryAngle_PreservesRectWidthAndHeightAndRightAngle)
{
    // (m_Right - m_Origin) and (m_Down - m_Origin) must stay the original
    // width/height in length and perpendicular to each other -- rotation is
    // rigid, it can't stretch or shear the rect.
    asge::math::Rect const dst{ 0.0f, 0.0f, 40.0f, 24.0f };

    SpriteDrawCorners const corners = SpriteGetDrawCorners(dst, kPi / 5.0f); // an off-axis angle

    float const rightDX = corners.m_Right.x() - corners.m_Origin.x();
    float const rightDY = corners.m_Right.y() - corners.m_Origin.y();
    float const downDX  = corners.m_Down.x()  - corners.m_Origin.x();
    float const downDY  = corners.m_Down.y()  - corners.m_Origin.y();

    EXPECT_NEAR( std::sqrt(rightDX * rightDX + rightDY * rightDY), 40.0f, 1e-3f );
    EXPECT_NEAR( std::sqrt(downDX * downDX + downDY * downDY), 24.0f, 1e-3f );
    EXPECT_NEAR( rightDX * downDX + rightDY * downDY, 0.0f, 1e-2f ); // dot product == 0 -> perpendicular
}

}
