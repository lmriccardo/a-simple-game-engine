#include <ASGE/Game/Utils/SpriteGeometry.hpp>
#include <ASGE/Video/Graphics/Texture.hpp>

#include <gtest/gtest.h>

namespace
{

using asge::game::components::Sprite;
using asge::game::components::Transform;
using asge::game::utils::SpriteGetDstRect;
using asge::game::utils::SpriteGetLocalRect;
using asge::game::utils::SpriteScaleAroundCenter;

// Minimal ITexture stub reporting a fixed size -- no SDL/GPU resource needed.
class FakeTexture final : public asge::video::ITexture
{
public:
    explicit FakeTexture(asge::math::Int2 inSize) : m_Size(inSize) {}

    [[nodiscard]] asge::math::Int2 Size() const noexcept override { return m_Size; }
    [[nodiscard]] void* NativeHandle() const noexcept override { return nullptr; }
    [[nodiscard]] bool IsValid() const noexcept override { return true; }

    void SetColorMod(asge::graphics::RGBA_Color) noexcept override {}

    [[nodiscard]] asge::Result<asge::graphics::RGBA_Color> GetColorMod() const noexcept override
    {
        return asge::Result<asge::graphics::RGBA_Color>::Ok(asge::graphics::RGBA_Color{});
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

// ─── SpriteGetLocalRect ──────────────────────────────────────────────────────

TEST(SpriteGetLocalRectTest, UsesLocalCoordinatesAndScaleNotWorld)
{
    FakeTexture texture(asge::math::Int2{ 32, 16 });
    Sprite const sprite{ .m_Texture = &texture };
    Transform const transform{ .m_LocalCoordinates = {1.0f, 2.0f}, .m_LocalScale = {2.0f, 3.0f},
                               .m_WorldCoordinates = {99.0f, 99.0f}, .m_WorldScale = {9.0f, 9.0f} };

    auto const rect = SpriteGetLocalRect(sprite, transform);

    ASSERT_TRUE(rect.has_value());
    EXPECT_FLOAT_EQ(rect->m_X, 1.0f);
    EXPECT_FLOAT_EQ(rect->m_Y, 2.0f);
    EXPECT_FLOAT_EQ(rect->m_Width, 64.0f);
    EXPECT_FLOAT_EQ(rect->m_Height, 48.0f);
    EXPECT_FALSE(SpriteGetLocalRect(Sprite{}, transform).has_value());
}

// ─── SpriteScaleAroundCenter ─────────────────────────────────────────────────

TEST(SpriteScaleAroundCenterTest, KeepsTheCenterFixedAndMarksTransformDirty)
{
    FakeTexture texture(asge::math::Int2{ 32, 16 });
    Sprite const sprite{ .m_Texture = &texture };
    Transform t{ .m_LocalCoordinates = {100.0f, 50.0f}, .m_LocalScale = {2.0f, 2.0f} }; // rect 64x32, centre (132, 66)

    SpriteScaleAroundCenter(sprite, t, {4.0f, 1.0f});                                   // rect 128x16

    auto const rect = SpriteGetLocalRect(sprite, t);
    ASSERT_TRUE(rect.has_value());
    EXPECT_FLOAT_EQ(rect->m_X + rect->m_Width * 0.5f, 132.0f);
    EXPECT_FLOAT_EQ(rect->m_Y + rect->m_Height * 0.5f, 66.0f);
    EXPECT_FLOAT_EQ(rect->m_Width, 128.0f);
    EXPECT_FLOAT_EQ(rect->m_Height, 16.0f);
    EXPECT_TRUE(t.m_Dirty);
}

TEST(SpriteScaleAroundCenterTest, NoTexture_LeavesTransformUntouched)
{
    Transform t{ .m_LocalCoordinates = {1.0f, 2.0f} };

    SpriteScaleAroundCenter(Sprite{}, t, {3.0f, 3.0f});

    EXPECT_FLOAT_EQ(t.m_LocalScale.x(), 1.0f);
    EXPECT_FLOAT_EQ(t.m_LocalCoordinates.x(), 1.0f);
    EXPECT_FALSE(t.m_Dirty);
}

}
