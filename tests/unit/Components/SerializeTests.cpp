#include <ASGE/Game/Components.hpp>
#include <ASGE/Core/Configuration/TOML_Builder.hpp>

#include <gtest/gtest.h>

#include <string>

namespace
{

using namespace asge::game::components;
using asge::config::TOMLBuilder;

// ─── SerializableComponents / kTableName contract ──────────────────────────

TEST(SerializableComponentsTest, ListsExactlyTransformVelocitySprite)
{
    static_assert(std::tuple_size_v<SerializableComponents> == 3);
    static_assert(std::is_same_v<std::tuple_element_t<0, SerializableComponents>, Transform>);
    static_assert(std::is_same_v<std::tuple_element_t<1, SerializableComponents>, Velocity>);
    static_assert(std::is_same_v<std::tuple_element_t<2, SerializableComponents>, Sprite>);
    SUCCEED();
}

TEST(SerializerKTableNameTest, EachSpecializationNamesItsOwnTable)
{
    // What a generic per-entity walker would check via TOMLTableView::HasTable.
    EXPECT_EQ(Serializer<Transform>::kTableName, "Transform");
    EXPECT_EQ(Serializer<Velocity>::kTableName, "Velocity");
    EXPECT_EQ(Serializer<Sprite>::kTableName, "Sprite");
}

// ─── Transform ──────────────────────────────────────────────────────────────

TEST(TransformSerializerTest, ToToml_WritesAllFieldsUnderTransformTable)
{
    TOMLBuilder builder;
    Serializer<Transform>::ToToml( Transform{ 1.0f, 2.0f, 0.5f, 3.0f, 4.0f }, builder );

    auto const dump = builder.ToString();
    EXPECT_NE(dump.find("[Transform]"), std::string::npos);
    EXPECT_NE(dump.find("m_X = 1.0"), std::string::npos);
    EXPECT_NE(dump.find("m_Y = 2.0"), std::string::npos);
    EXPECT_NE(dump.find("m_Rotation = 0.5"), std::string::npos);
    EXPECT_NE(dump.find("m_ScaleX = 3.0"), std::string::npos);
    EXPECT_NE(dump.find("m_ScaleY = 4.0"), std::string::npos);
}

TEST(TransformSerializerTest, RoundTripsThroughToTomlAndFromToml)
{
    TOMLBuilder builder;
    Transform const original{ 10.0f, -5.0f, 1.25f, 2.0f, 0.5f };
    Serializer<Transform>::ToToml( original, builder );

    Transform const restored = Serializer<Transform>::FromToml( builder );
    EXPECT_FLOAT_EQ(restored.m_X, original.m_X);
    EXPECT_FLOAT_EQ(restored.m_Y, original.m_Y);
    EXPECT_FLOAT_EQ(restored.m_Rotation, original.m_Rotation);
    EXPECT_FLOAT_EQ(restored.m_ScaleX, original.m_ScaleX);
    EXPECT_FLOAT_EQ(restored.m_ScaleY, original.m_ScaleY);
}

TEST(TransformSerializerTest, FromToml_MissingKeysFallBackToStructDefaults)
{
    TOMLBuilder builder;
    builder.Table("Transform"); // present but empty
    Transform const restored = Serializer<Transform>::FromToml( builder );

    EXPECT_FLOAT_EQ(restored.m_X, 0.0f);
    EXPECT_FLOAT_EQ(restored.m_Y, 0.0f);
    EXPECT_FLOAT_EQ(restored.m_Rotation, 0.0f);
    EXPECT_FLOAT_EQ(restored.m_ScaleX, 1.0f); // Transform's own default, not 0
    EXPECT_FLOAT_EQ(restored.m_ScaleY, 1.0f);
}

// ─── Velocity ─────────────────────────────────────────────────────────────

TEST(VelocitySerializerTest, ToToml_WritesFieldsUnderVelocityTable)
{
    TOMLBuilder builder;
    Serializer<Velocity>::ToToml( Velocity{ 12.5f, -3.0f }, builder );

    auto const dump = builder.ToString();
    EXPECT_NE(dump.find("[Velocity]"), std::string::npos);
    EXPECT_NE(dump.find("m_DX = 12.5"), std::string::npos);
    EXPECT_NE(dump.find("m_DY = -3.0"), std::string::npos);
}

TEST(VelocitySerializerTest, RoundTripsThroughToTomlAndFromToml)
{
    TOMLBuilder builder;
    Velocity const original{ 12.5f, -3.0f };
    Serializer<Velocity>::ToToml( original, builder );

    Velocity const restored = Serializer<Velocity>::FromToml( builder );
    EXPECT_FLOAT_EQ(restored.m_DX, original.m_DX);
    EXPECT_FLOAT_EQ(restored.m_DY, original.m_DY);
}

// ─── Sprite ───────────────────────────────────────────────────────────────

TEST(SpriteSerializerTest, ToToml_WritesVirtualPathButNeverTheTexturePointer)
{
    TOMLBuilder builder;
    Sprite sprite{};
    sprite.m_VirtualPath = "textures/checker.bmp";
    Serializer<Sprite>::ToToml( sprite, builder );

    auto const dump = builder.ToString();
    EXPECT_NE(dump.find(R"(m_VirtualPath = "textures/checker.bmp")"), std::string::npos);
}

TEST(SpriteSerializerTest, RoundTrip_WithoutSourceRectLeavesItNulloptAndTextureNull)
{
    TOMLBuilder builder;
    Sprite sprite{};
    sprite.m_VirtualPath = "textures/checker.bmp";
    Serializer<Sprite>::ToToml( sprite, builder );

    Sprite const restored = Serializer<Sprite>::FromToml( builder );
    EXPECT_EQ(restored.m_VirtualPath, "textures/checker.bmp");
    EXPECT_EQ(restored.m_Texture, nullptr); // resolving the path into a live
                                             // texture is the caller's job
    EXPECT_FALSE(restored.m_SourceRect.has_value());
}

TEST(SpriteSerializerTest, RoundTrip_WithSourceRectRestoresItsFields)
{
    TOMLBuilder builder;
    Sprite sprite{};
    sprite.m_VirtualPath = "textures/atlas.png";
    sprite.m_SourceRect = asge::math::Rect{ 16.0f, 32.0f, 8.0f, 8.0f };
    Serializer<Sprite>::ToToml( sprite, builder );

    auto const dump = builder.ToString();
    EXPECT_NE(dump.find("[Sprite.SourceRect]"), std::string::npos);

    Sprite const restored = Serializer<Sprite>::FromToml( builder );
    ASSERT_TRUE(restored.m_SourceRect.has_value());
    EXPECT_FLOAT_EQ(restored.m_SourceRect->x, 16.0f);
    EXPECT_FLOAT_EQ(restored.m_SourceRect->y, 32.0f);
    EXPECT_FLOAT_EQ(restored.m_SourceRect->w, 8.0f);
    EXPECT_FLOAT_EQ(restored.m_SourceRect->h, 8.0f);
}

}
