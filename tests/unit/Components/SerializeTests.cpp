#include <ASGE/Game/Components.hpp>
#include <ASGE/Game/Components/Name.hpp>
#include <ASGE/Game/Components/UI/UIButton.hpp>
#include <ASGE/Core/Configuration/TOML_Builder.hpp>

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace
{

using namespace asge::game::components;
using namespace asge::game::scene;
using asge::config::toml::TOMLBuilder;

// ─── SerializableComponents / kTableName contract ──────────────────────────

TEST(SerializableComponentsTest, ListsExactlyTransformVelocitySpriteColliderRigidbodyAnimationAudioSourceCameraPathFollowNameHierarchyUIButtonRenderInfo)
{
    static_assert(std::tuple_size_v<SerializableComponents> == 13);
    static_assert(std::is_same_v<std::tuple_element_t<0, SerializableComponents>, Transform>);
    static_assert(std::is_same_v<std::tuple_element_t<1, SerializableComponents>, Velocity>);
    static_assert(std::is_same_v<std::tuple_element_t<2, SerializableComponents>, Sprite>);
    static_assert(std::is_same_v<std::tuple_element_t<3, SerializableComponents>, Collider>);
    static_assert(std::is_same_v<std::tuple_element_t<4, SerializableComponents>, Rigidbody>);
    static_assert(std::is_same_v<std::tuple_element_t<5, SerializableComponents>, Animation>);
    static_assert(std::is_same_v<std::tuple_element_t<6, SerializableComponents>, AudioSource>);
    static_assert(std::is_same_v<std::tuple_element_t<7, SerializableComponents>, Camera>);
    static_assert(std::is_same_v<std::tuple_element_t<8, SerializableComponents>, PathFollow>);
    static_assert(std::is_same_v<std::tuple_element_t<9, SerializableComponents>, Name>);
    static_assert(std::is_same_v<std::tuple_element_t<10, SerializableComponents>, Hierarchy>);
    static_assert(std::is_same_v<std::tuple_element_t<11, SerializableComponents>, UIButton>);
    static_assert(std::is_same_v<std::tuple_element_t<12, SerializableComponents>, RenderInfo>);
    SUCCEED();
}

TEST(SerializerKTableNameTest, EachSpecializationNamesItsOwnTable)
{
    // What a generic per-entity walker would check via TOMLTableView::HasTable.
    EXPECT_EQ(Serializer<Transform>::kTableName, "Transform");
    EXPECT_EQ(Serializer<Velocity>::kTableName, "Velocity");
    EXPECT_EQ(Serializer<Sprite>::kTableName, "Sprite");
    EXPECT_EQ(Serializer<Collider>::kTableName, "Collider");
    EXPECT_EQ(Serializer<Rigidbody>::kTableName, "Rigidbody");
    EXPECT_EQ(Serializer<Animation>::kTableName, "Animation");
    EXPECT_EQ(Serializer<AudioSource>::kTableName, "AudioSource");
    EXPECT_EQ(Serializer<Camera>::kTableName, "Camera");
    EXPECT_EQ(Serializer<PathFollow>::kTableName, "PathFollow");
    EXPECT_EQ(Serializer<Name>::kTableName, "Name");
    EXPECT_EQ(Serializer<Hierarchy>::kTableName, "Hierarchy");
    EXPECT_EQ(Serializer<UIButton>::kTableName, "UIButton");
    EXPECT_EQ(Serializer<RenderInfo>::kTableName, "RenderInfo");
}

// ─── Transform ──────────────────────────────────────────────────────────────

TEST(TransformSerializerTest, ToToml_WritesAllFieldsUnderTransformTable)
{
    TOMLBuilder builder;
    Serializer<Transform>::ToToml(
        Transform{ .m_LocalCoordinates = {1.0f, 2.0f}, .m_LocalScale = {3.0f, 4.0f}, .m_LocalRotation = 0.5f },
        builder, asge::game::scene::SaveContext{} );

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
    Transform const original{ .m_LocalCoordinates = {10.0f, -5.0f}, .m_LocalScale = {2.0f, 0.5f}, .m_LocalRotation = 1.25f };
    Serializer<Transform>::ToToml( original, builder, asge::game::scene::SaveContext{} );

    Transform const restored = Serializer<Transform>::FromToml( builder, asge::game::scene::LoadContext{} );
    EXPECT_FLOAT_EQ(restored.m_LocalCoordinates.x(), original.m_LocalCoordinates.x());
    EXPECT_FLOAT_EQ(restored.m_LocalCoordinates.y(), original.m_LocalCoordinates.y());
    EXPECT_FLOAT_EQ(restored.m_LocalRotation, original.m_LocalRotation);
    EXPECT_FLOAT_EQ(restored.m_LocalScale.x(), original.m_LocalScale.x());
    EXPECT_FLOAT_EQ(restored.m_LocalScale.y(), original.m_LocalScale.y());
}

TEST(TransformSerializerTest, FromToml_MissingKeysFallBackToStructDefaults)
{
    TOMLBuilder builder;
    builder.Table("Transform"); // present but empty
    Transform const restored = Serializer<Transform>::FromToml(builder, asge::game::scene::LoadContext{});

    EXPECT_FLOAT_EQ(restored.m_LocalCoordinates.x(), 0.0f);
    EXPECT_FLOAT_EQ(restored.m_LocalCoordinates.y(), 0.0f);
    EXPECT_FLOAT_EQ(restored.m_LocalRotation, 0.0f);
    EXPECT_FLOAT_EQ(restored.m_LocalScale.x(), 1.0f); // Transform's own default, not 0
    EXPECT_FLOAT_EQ(restored.m_LocalScale.y(), 1.0f);
}

// ─── Velocity ─────────────────────────────────────────────────────────────

TEST(VelocitySerializerTest, ToToml_WritesFieldsUnderVelocityTable)
{
    TOMLBuilder builder;
    Serializer<Velocity>::ToToml(Velocity{ 12.5f, -3.0f }, builder, asge::game::scene::SaveContext{});

    auto const dump = builder.ToString();
    EXPECT_NE(dump.find("[Velocity]"), std::string::npos);
    EXPECT_NE(dump.find("m_DX = 12.5"), std::string::npos);
    EXPECT_NE(dump.find("m_DY = -3.0"), std::string::npos);
}

TEST(VelocitySerializerTest, RoundTripsThroughToTomlAndFromToml)
{
    TOMLBuilder builder;
    Velocity const original{ 12.5f, -3.0f };
    Serializer<Velocity>::ToToml(original, builder, asge::game::scene::SaveContext{});

    Velocity const restored = Serializer<Velocity>::FromToml(builder, asge::game::scene::LoadContext{});
    EXPECT_FLOAT_EQ(restored.m_DX, original.m_DX);
    EXPECT_FLOAT_EQ(restored.m_DY, original.m_DY);
}

// ─── Sprite ───────────────────────────────────────────────────────────────

TEST(SpriteSerializerTest, ToToml_WritesVirtualPathButNeverTheTexturePointer)
{
    TOMLBuilder builder;
    Sprite sprite{};
    sprite.m_VirtualPath = "textures/checker.bmp";
    Serializer<Sprite>::ToToml(sprite, builder, asge::game::scene::SaveContext{});

    auto const dump = builder.ToString();
    EXPECT_NE(dump.find(R"(m_VirtualPath = "textures/checker.bmp")"), std::string::npos);
}

TEST(SpriteSerializerTest, RoundTrip_WithoutSourceRectLeavesItNulloptAndTextureNull)
{
    TOMLBuilder builder;
    Sprite sprite{};
    sprite.m_VirtualPath = "textures/checker.bmp";
    Serializer<Sprite>::ToToml(sprite, builder, asge::game::scene::SaveContext{});

    Sprite const restored = Serializer<Sprite>::FromToml(builder, asge::game::scene::LoadContext{});
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
    Serializer<Sprite>::ToToml(sprite, builder, asge::game::scene::SaveContext{});

    auto const dump = builder.ToString();
    EXPECT_NE(dump.find("[Sprite.SourceRect]"), std::string::npos);

    Sprite const restored = Serializer<Sprite>::FromToml(builder, asge::game::scene::LoadContext{});
    ASSERT_TRUE(restored.m_SourceRect.has_value());
    EXPECT_FLOAT_EQ(restored.m_SourceRect->m_X, 16.0f);
    EXPECT_FLOAT_EQ(restored.m_SourceRect->m_Y, 32.0f);
    EXPECT_FLOAT_EQ(restored.m_SourceRect->m_Width, 8.0f);
    EXPECT_FLOAT_EQ(restored.m_SourceRect->m_Height, 8.0f);
}

// ─── Collider ─────────────────────────────────────────────────────────────

TEST(ColliderSerializerTest, ToToml_WritesShapeDiscriminatorAndResolution)
{
    TOMLBuilder builder;
    Collider collider{ .m_LocalBounds = asge::math::Rect{ 1.0f, 2.0f, 3.0f, 4.0f },
                        .m_Resolution = ResolutionType::Trigger };
    Serializer<Collider>::ToToml(collider, builder, asge::game::scene::SaveContext{});

    auto const dump = builder.ToString();
    EXPECT_NE(dump.find("[Collider]"), std::string::npos);
    EXPECT_NE(dump.find(R"(m_Shape = "Rect")"), std::string::npos);
    EXPECT_NE(dump.find(R"(m_Resolution = "Trigger")"), std::string::npos);
    EXPECT_NE(dump.find("m_OffsetX = 1.0"), std::string::npos);
    EXPECT_NE(dump.find("m_Width = 3.0"), std::string::npos);
}

TEST(ColliderSerializerTest, RoundTrip_RectShapeSolidResolution)
{
    TOMLBuilder builder;
    Collider const original{ .m_LocalBounds = asge::math::Rect{ 1.0f, 2.0f, 3.0f, 4.0f },
                              .m_Resolution = ResolutionType::Solid };
    Serializer<Collider>::ToToml(original, builder, asge::game::scene::SaveContext{});

    Collider const restored = Serializer<Collider>::FromToml(builder, asge::game::scene::LoadContext{});
    ASSERT_TRUE(std::holds_alternative<asge::math::Rect>(restored.m_LocalBounds));
    auto const& rect = std::get<asge::math::Rect>(restored.m_LocalBounds);
    EXPECT_FLOAT_EQ(rect.m_X, 1.0f);
    EXPECT_FLOAT_EQ(rect.m_Y, 2.0f);
    EXPECT_FLOAT_EQ(rect.m_Width, 3.0f);
    EXPECT_FLOAT_EQ(rect.m_Height, 4.0f);
    EXPECT_EQ(restored.m_Resolution, ResolutionType::Solid);
}

TEST(ColliderSerializerTest, RoundTrip_CircleShapeTriggerResolution)
{
    TOMLBuilder builder;
    Collider const original{
        .m_LocalBounds = asge::math::Circle{ asge::math::Float2{ 5.0f, 6.0f }, 7.0f },
        .m_Resolution = ResolutionType::Trigger
    };
    Serializer<Collider>::ToToml(original, builder, asge::game::scene::SaveContext{});

    Collider const restored = Serializer<Collider>::FromToml(builder, asge::game::scene::LoadContext{});
    ASSERT_TRUE(std::holds_alternative<asge::math::Circle>(restored.m_LocalBounds));
    auto const& circle = std::get<asge::math::Circle>(restored.m_LocalBounds);
    EXPECT_FLOAT_EQ(circle.m_Center.x(), 5.0f);
    EXPECT_FLOAT_EQ(circle.m_Center.y(), 6.0f);
    EXPECT_FLOAT_EQ(circle.m_Radius, 7.0f);
    EXPECT_EQ(restored.m_Resolution, ResolutionType::Trigger);
}

TEST(ColliderSerializerTest, FromToml_MissingShapeKeyDefaultsToRect)
{
    // A scene file saved before Circle colliders existed has no "m_Shape" key at all.
    TOMLBuilder builder;
    builder.Table("Collider").Set("m_Width", 3.0f).Set("m_Height", 4.0f);

    Collider const restored = Serializer<Collider>::FromToml(builder, asge::game::scene::LoadContext{});
    EXPECT_TRUE(std::holds_alternative<asge::math::Rect>(restored.m_LocalBounds));
}

TEST(ColliderSerializerTest, FromToml_MissingResolutionKeyDefaultsToSolidNotUnknown)
{
    // A scene file saved before Trigger colliders existed has no
    // "m_Resolution" key at all -- must still behave exactly like the
    // in-code default (Solid), not silently become Unknown (which
    // DetectCollisions ignores entirely -- see PhysicsSystem.hpp).
    TOMLBuilder builder;
    builder.Table("Collider")
           .Set<std::string>("m_Shape", "Rect")
           .Set("m_Width", 3.0f).Set("m_Height", 4.0f);

    Collider const restored = Serializer<Collider>::FromToml(builder, asge::game::scene::LoadContext{});
    EXPECT_EQ(restored.m_Resolution, ResolutionType::Solid);
}

TEST(ColliderSerializerTest, FromToml_UnrecognizedResolutionValueBecomesUnknown)
{
    TOMLBuilder builder;
    builder.Table("Collider")
           .Set<std::string>("m_Shape", "Rect")
           .Set<std::string>("m_Resolution", "NotARealValue");

    Collider const restored = Serializer<Collider>::FromToml(builder, asge::game::scene::LoadContext{});
    EXPECT_EQ(restored.m_Resolution, ResolutionType::Unknown);
}

// ─── Collider — m_Layer / m_Mask ───────────────────────────────────────────

TEST(ColliderSerializerTest, RoundTrip_DefaultLayerAndMaskSurviveExactly)
{
    // The regression case: m_Mask's default is ~0u (every bit set). A float
    // can't exactly hold every uint32_t -- its 24-bit mantissa rounds
    // 4294967295u up to 2^32, which then isn't representable as uint32_t at
    // all on the read side. Storing through `int` instead (2's-complement
    // wraparound, well-defined since C++20) must round-trip this exactly.
    TOMLBuilder builder;
    Collider const original{ .m_LocalBounds = asge::math::Rect{ 0.0f, 0.0f, 1.0f, 1.0f } };
    Serializer<Collider>::ToToml(original, builder, asge::game::scene::SaveContext{});

    Collider const restored = Serializer<Collider>::FromToml(builder, asge::game::scene::LoadContext{});
    EXPECT_EQ(restored.m_Layer, original.m_Layer);
    EXPECT_EQ(restored.m_Mask, original.m_Mask);
    EXPECT_EQ(restored.m_Mask, ~CollisionLayer{0});
}

TEST(ColliderSerializerTest, RoundTrip_CustomLayerAndMask)
{
    TOMLBuilder builder;
    Collider const original{ .m_LocalBounds = asge::math::Rect{ 0.0f, 0.0f, 1.0f, 1.0f },
                              .m_Layer = 1u << 2,
                              .m_Mask = (1u << 0) | (1u << 2) };
    Serializer<Collider>::ToToml(original, builder, asge::game::scene::SaveContext{});

    Collider const restored = Serializer<Collider>::FromToml(builder, asge::game::scene::LoadContext{});
    EXPECT_EQ(restored.m_Layer, original.m_Layer);
    EXPECT_EQ(restored.m_Mask, original.m_Mask);
}

TEST(ColliderSerializerTest, FromToml_MissingLayerAndMaskKeysDefaultToCollideWithEverything)
{
    // A scene file saved before CollisionLayer existed has neither key at all.
    TOMLBuilder builder;
    builder.Table("Collider").Set<std::string>("m_Shape", "Rect");

    Collider const restored = Serializer<Collider>::FromToml(builder, asge::game::scene::LoadContext{});
    EXPECT_EQ(restored.m_Layer, 1u);
    EXPECT_EQ(restored.m_Mask, ~CollisionLayer{0});
}

// ─── Animation ──────────────────────────────────────────────────────────────

TEST(AnimationSerializerTest, ToToml_WritesClipPathAndFrameDurationUnderAnimationTable)
{
    TOMLBuilder builder;
    Animation const anim{ .m_ClipPath = "clips/walk.toml", .m_FrameDuration = 0.2f };
    Serializer<Animation>::ToToml(anim, builder, asge::game::scene::SaveContext{});

    auto const dump = builder.ToString();
    EXPECT_NE(dump.find("[Animation]"), std::string::npos);
    EXPECT_NE(dump.find(R"(m_ClipPath = "clips/walk.toml")"), std::string::npos);
    EXPECT_NE(dump.find("m_FrameDuration = 0.2"), std::string::npos);
}

TEST(AnimationSerializerTest, RoundTrip_ClipPathAndDurationPreserved)
{
    TOMLBuilder builder;
    Animation const original{ .m_ClipPath = "clips/walk.toml", .m_FrameDuration = 0.15f };
    Serializer<Animation>::ToToml(original, builder, asge::game::scene::SaveContext{});

    Animation const restored = Serializer<Animation>::FromToml(builder, asge::game::scene::LoadContext{});
    EXPECT_EQ(restored.m_ClipPath, original.m_ClipPath);
    EXPECT_FLOAT_EQ(restored.m_FrameDuration, original.m_FrameDuration);
}

TEST(AnimationSerializerTest, FromToml_ClipAndPlaybackStateAlwaysResetToStructDefaults)
{
    // Serializer<Animation> only round-trips m_ClipPath and m_FrameDuration
    // -- FromToml must never carry over m_Clip (a scene file can't describe
    // a resolved asset) or playback progress, since neither is something a
    // scene file describes at all.
    TOMLBuilder builder;
    Serializer<Animation>::ToToml(Animation{ .m_ClipPath = "clips/walk.toml" }, builder, asge::game::scene::SaveContext{});

    Animation const restored = Serializer<Animation>::FromToml(builder, asge::game::scene::LoadContext{});
    EXPECT_EQ(restored.m_Clip, nullptr);
    EXPECT_EQ(restored.m_CurrentFrame, 0u);
    EXPECT_FLOAT_EQ(restored.m_ElapsedTime, 0.0f);
    EXPECT_TRUE(restored.m_Loop);
    EXPECT_TRUE(restored.m_Playing);
}

TEST(AnimationSerializerTest, FromToml_MissingClipPathKeyDefaultsToEmptyString)
{
    // A hand-written or pre-Animation scene file has no "m_ClipPath" key at
    // all -- AnimationSystem already treats an empty m_ClipPath as "not
    // animating" (see RenderSystemTests.cpp), so this must not error.
    TOMLBuilder builder;
    builder.Table("Animation").Set("m_FrameDuration", 0.1f);

    Animation const restored = Serializer<Animation>::FromToml(builder, asge::game::scene::LoadContext{});
    EXPECT_TRUE(restored.m_ClipPath.empty());
}

// ─── AudioSource ────────────────────────────────────────────────────────────

TEST(AudioSourceSerializerTest, ToToml_WritesVirtualClipPathUnderAudioSourceTable)
{
    TOMLBuilder builder;
    AudioSource source{};
    source.m_VirtualClipPath = "audio/theme.ogg";
    Serializer<AudioSource>::ToToml(source, builder, asge::game::scene::SaveContext{});

    auto const dump = builder.ToString();
    EXPECT_NE(dump.find("[AudioSource]"), std::string::npos);
    EXPECT_NE(dump.find(R"(m_VirtualClipPath = "audio/theme.ogg")"), std::string::npos);
}

TEST(AudioSourceSerializerTest, RoundTrip_VirtualClipPathPreserved)
{
    TOMLBuilder builder;
    AudioSource source{};
    source.m_VirtualClipPath = "audio/theme.ogg";
    Serializer<AudioSource>::ToToml(source, builder, asge::game::scene::SaveContext{});

    AudioSource const restored = Serializer<AudioSource>::FromToml(builder, asge::game::scene::LoadContext{});
    EXPECT_EQ(restored.m_VirtualClipPath, "audio/theme.ogg");
}

TEST(AudioSourceSerializerTest, FromToml_ClipAndPlaybackStateAlwaysResetToStructDefaults)
{
    // Serializer<AudioSource> only round-trips m_VirtualClipPath -- FromToml
    // must never carry over m_Clip/m_Stream (a scene file can't describe a
    // resolved asset or a live device stream) or playback flags, since
    // neither is something a scene file describes at all.
    TOMLBuilder builder;
    AudioSource source{};
    source.m_VirtualClipPath = "audio/theme.ogg";
    Serializer<AudioSource>::ToToml(source, builder, asge::game::scene::SaveContext{});

    AudioSource const restored = Serializer<AudioSource>::FromToml(builder, asge::game::scene::LoadContext{});
    EXPECT_EQ(restored.m_Clip, nullptr);
    EXPECT_EQ(restored.m_Stream, nullptr);
    EXPECT_FALSE(restored.m_Playing);
    EXPECT_FALSE(restored.m_Loop);
    EXPECT_FLOAT_EQ(restored.m_Volume, 1.0f);
}

TEST(AudioSourceSerializerTest, FromToml_MissingVirtualClipPathKeyDefaultsToEmptyString)
{
    // A hand-written or pre-AudioSource scene file has no "m_VirtualClipPath"
    // key at all -- AssetManager::ResolveAssets already treats an empty path
    // as "nothing to resolve", so this must not error.
    TOMLBuilder builder;
    builder.Table("AudioSource");

    AudioSource const restored = Serializer<AudioSource>::FromToml(builder, asge::game::scene::LoadContext{});
    EXPECT_TRUE(restored.m_VirtualClipPath.empty());
}

// ─── Camera ─────────────────────────────────────────────────────────────────

TEST(CameraSerializerTest, ToToml_WritesFieldsUnderCameraTable)
{
    TOMLBuilder builder;
    Serializer<Camera>::ToToml(Camera{ .m_Zoom = 2.5f, .m_Smoothing = 8.0f }, builder, asge::game::scene::SaveContext{});

    auto const dump = builder.ToString();
    EXPECT_NE(dump.find("[Camera]"), std::string::npos);
    EXPECT_NE(dump.find("m_Zoom = 2.5"), std::string::npos);
    EXPECT_NE(dump.find("m_Smoothing = 8.0"), std::string::npos);
}

TEST(CameraSerializerTest, RoundTripsThroughToTomlAndFromToml)
{
    TOMLBuilder builder;
    Camera const original{ .m_Zoom = 2.5f, .m_Smoothing = 8.0f };
    Serializer<Camera>::ToToml(original, builder, asge::game::scene::SaveContext{});

    Camera const restored = Serializer<Camera>::FromToml(builder, asge::game::scene::LoadContext{});
    EXPECT_FLOAT_EQ(restored.m_Zoom, original.m_Zoom);
    EXPECT_FLOAT_EQ(restored.m_Smoothing, original.m_Smoothing);
}

TEST(CameraSerializerTest, FromToml_MissingKeysFallBackToStructDefaults)
{
    TOMLBuilder builder;
    builder.Table("Camera");

    Camera const restored = Serializer<Camera>::FromToml(builder, asge::game::scene::LoadContext{});
    EXPECT_FLOAT_EQ(restored.m_Zoom, 1.0f);
    EXPECT_FLOAT_EQ(restored.m_Smoothing, 0.0f);
}

// ─── PathFollow ─────────────────────────────────────────────────────────────

TEST(PathFollowSerializerTest, ToToml_WritesWaypointsAndFieldsUnderPathFollowTable)
{
    TOMLBuilder builder;
    PathFollow value{};
    value.m_Waypoints = { asge::math::Float2{ 0.0f, 0.0f }, asge::math::Float2{ 10.0f, 20.0f } };
    value.m_Speed = 4.5f;
    value.m_Loop = true;
    value.m_Resolution = 16;
    Serializer<PathFollow>::ToToml(value, builder, asge::game::scene::SaveContext{});

    auto const dump = builder.ToString();
    EXPECT_NE(dump.find("[PathFollow]"), std::string::npos);
    EXPECT_NE(dump.find("m_WaypointX"), std::string::npos);
    EXPECT_NE(dump.find("m_WaypointY"), std::string::npos);
    EXPECT_NE(dump.find("m_Speed = 4.5"), std::string::npos);
    EXPECT_NE(dump.find("m_Loop = true"), std::string::npos);
    EXPECT_NE(dump.find("m_Resolution = 16"), std::string::npos);
}

TEST(PathFollowSerializerTest, RoundTripsWaypointsAndFieldsThroughToTomlAndFromToml)
{
    TOMLBuilder builder;
    PathFollow original{};
    original.m_Waypoints = {
        asge::math::Float2{ 0.0f, 0.0f }, asge::math::Float2{ 10.0f, 20.0f }, asge::math::Float2{ -5.0f, 30.0f }
    };
    original.m_Speed = 4.5f;
    original.m_Loop = true;
    original.m_Resolution = 16;
    Serializer<PathFollow>::ToToml(original, builder, asge::game::scene::SaveContext{});

    PathFollow const restored = Serializer<PathFollow>::FromToml(builder, asge::game::scene::LoadContext{});
    ASSERT_EQ(restored.m_Waypoints.size(), original.m_Waypoints.size());
    for ( std::size_t ii = 0; ii < original.m_Waypoints.size(); ++ii )
    {
        EXPECT_FLOAT_EQ(restored.m_Waypoints[ii].x(), original.m_Waypoints[ii].x());
        EXPECT_FLOAT_EQ(restored.m_Waypoints[ii].y(), original.m_Waypoints[ii].y());
    }
    EXPECT_FLOAT_EQ(restored.m_Speed, original.m_Speed);
    EXPECT_EQ(restored.m_Loop, original.m_Loop);
    EXPECT_EQ(restored.m_Resolution, original.m_Resolution);
}

TEST(PathFollowSerializerTest, FromToml_MissingKeysFallBackToStructDefaults)
{
    // Regression test: m_Resolution used to read via table.Get<int>("m_Resolution")
    // with no explicit default, silently falling back to int{} (0) rather
    // than PathFollow's own in-code default (32) whenever the key was
    // missing -- e.g. a hand-authored or pre-PathFollow scene file. A
    // resolution of 0 collapses every segment's arc-length table to a
    // single zero-length sample (see CatmullRomSpline::BuildArcLengthTable),
    // degenerating the whole spline.
    TOMLBuilder builder;
    builder.Table("PathFollow");

    PathFollow const restored = Serializer<PathFollow>::FromToml(builder, asge::game::scene::LoadContext{});
    EXPECT_TRUE(restored.m_Waypoints.empty());
    EXPECT_FLOAT_EQ(restored.m_Speed, 1.0f);
    EXPECT_FALSE(restored.m_Loop);
    EXPECT_EQ(restored.m_Resolution, 32u);
}

TEST(PathFollowSerializerTest, FromToml_RuntimeOnlyFieldsAlwaysResetToStructDefaults)
{
    // m_Path/m_Traveled/m_Finished are never written by ToToml in the first
    // place, so FromToml can't restore them either -- a scene file
    // describes an entity's authored path, not how far a previous run got
    // along it (see AudioSource's Serializer doc comment for the same
    // reasoning applied to playback state).
    TOMLBuilder builder;
    PathFollow value{};
    value.m_Waypoints = { asge::math::Float2{ 0.0f, 0.0f }, asge::math::Float2{ 10.0f, 0.0f } };
    Serializer<PathFollow>::ToToml(value, builder, asge::game::scene::SaveContext{});

    PathFollow const restored = Serializer<PathFollow>::FromToml(builder, asge::game::scene::LoadContext{});
    EXPECT_FALSE(restored.m_Path.HasSegments());
    EXPECT_FLOAT_EQ(restored.m_Path.Length(), 0.0f);
    EXPECT_FLOAT_EQ(restored.m_Traveled, 0.0f);
    EXPECT_FALSE(restored.m_Finished);
}

TEST(PathFollowSerializerTest, FromToml_MismatchedWaypointArrayLengthsUsesTheShorterOne)
{
    // ToToml always writes m_WaypointX/Y with equal lengths, but FromToml's
    // own loop only trusts xs.size() -- a hand-edited file with a shorter
    // m_WaypointY must not read past its end.
    TOMLBuilder builder;
    auto table = builder.Table("PathFollow");
    table.SetArray("m_WaypointX", std::vector<float>{ 0.0f, 10.0f, 20.0f });
    table.SetArray("m_WaypointY", std::vector<float>{ 0.0f, 5.0f });

    PathFollow const restored = Serializer<PathFollow>::FromToml(builder, asge::game::scene::LoadContext{});
    ASSERT_EQ(restored.m_Waypoints.size(), 2u); // min(3, 2) -- the third X has no matching Y
    EXPECT_FLOAT_EQ(restored.m_Waypoints[1].x(), 10.0f);
    EXPECT_FLOAT_EQ(restored.m_Waypoints[1].y(), 5.0f);
}

// ─── Name ───────────────────────────────────────────────────────────────────

TEST(NameSerializerTest, ToToml_WritesNameUnderNameTable)
{
    TOMLBuilder builder;
    Serializer<Name>::ToToml(Name{ "Player" }, builder, asge::game::scene::SaveContext{});

    auto const dump = builder.ToString();
    EXPECT_NE(dump.find("[Name]"), std::string::npos);
    EXPECT_NE(dump.find(R"(m_Name = "Player")"), std::string::npos);
}

TEST(NameSerializerTest, RoundTripsThroughToTomlAndFromToml)
{
    TOMLBuilder builder;
    Name const original{ "Enemy Spawner" };
    Serializer<Name>::ToToml(original, builder, asge::game::scene::SaveContext{});

    Name const restored = Serializer<Name>::FromToml(builder, asge::game::scene::LoadContext{});
    EXPECT_EQ(restored.m_Name, original.m_Name);
}

TEST(NameSerializerTest, FromToml_MissingKeyDefaultsToEmptyString)
{
    TOMLBuilder builder;
    builder.Table("Name"); // present but empty

    Name const restored = Serializer<Name>::FromToml(builder, asge::game::scene::LoadContext{});
    EXPECT_TRUE(restored.m_Name.empty());
}

// ─── Hierarchy ──────────────────────────────────────────────────────────────
//
// Unlike every other component here, Hierarchy's fields are themselves
// Entity references -- a raw handle can't survive a save/load round trip
// (Load() creates entirely new entities), so ToToml/FromToml go through
// SaveContext::Resolve/LoadContext::Resolve instead of writing/reading the
// Entity directly. These tests exercise that resolution explicitly, with
// SaveContext/LoadContext built by hand rather than relying on a real
// SceneSerializer -- see SceneSerializerTests.cpp for the full round trip
// through an actual parent/child graph.

TEST(HierarchySerializerTest, ToToml_WritesEachFieldAsItsSaveContextResolvedId)
{
    TOMLBuilder builder;
    asge::ecs::Entity const parent{ 3, 0 };
    asge::ecs::Entity const firstChild{ 5, 0 };
    asge::ecs::Entity const lastChild{ 7, 0 };

    asge::game::scene::SaveContext ctx;
    ctx.m_Ids[parent] = 10;
    ctx.m_Ids[firstChild] = 11;
    ctx.m_Ids[lastChild] = 12;

    Hierarchy const value{ .m_Parent = parent, .m_FirstChild = firstChild, .m_LastChild = lastChild };
    // m_PrevSibling/m_NextSibling left at Entity::Null().
    Serializer<Hierarchy>::ToToml( value, builder, ctx );

    auto const dump = builder.ToString();
    EXPECT_NE(dump.find("[Hierarchy]"), std::string::npos);
    EXPECT_NE(dump.find("m_Parent = 10"), std::string::npos);
    EXPECT_NE(dump.find("m_FirstChild = 11"), std::string::npos);
    EXPECT_NE(dump.find("m_LastChild = 12"), std::string::npos);
    EXPECT_NE(dump.find("m_PrevSibling = -1"), std::string::npos);
    EXPECT_NE(dump.find("m_NextSibling = -1"), std::string::npos);
}

TEST(HierarchySerializerTest, ToToml_EntityNotInSaveContextResolvesToNegativeOne)
{
    // Resolve returns -1 for an Entity SaveContext never learned about --
    // same as Entity::Null() -- rather than throwing or crashing.
    TOMLBuilder builder;
    asge::game::scene::SaveContext const emptyCtx;
    Hierarchy const value{ .m_Parent = asge::ecs::Entity{ 9, 0 } };
    Serializer<Hierarchy>::ToToml( value, builder, emptyCtx );

    auto const dump = builder.ToString();
    EXPECT_NE(dump.find("m_Parent = -1"), std::string::npos);
}

TEST(HierarchySerializerTest, FromToml_ResolvesIdsBackToTheCorrectEntitiesViaLoadContext)
{
    TOMLBuilder builder;
    auto table = builder.Table("Hierarchy");
    table.Set<int>( "m_Parent", 10 );
    table.Set<int>( "m_FirstChild", 11 );
    table.Set<int>( "m_LastChild", -1 );
    table.Set<int>( "m_PrevSibling", -1 );
    table.Set<int>( "m_NextSibling", -1 );

    asge::ecs::Entity const parent{ 3, 0 };
    asge::ecs::Entity const firstChild{ 5, 1 };
    asge::game::scene::LoadContext ctx;
    ctx.m_Entities[10] = parent;
    ctx.m_Entities[11] = firstChild;

    Hierarchy const restored = Serializer<Hierarchy>::FromToml( builder, ctx );
    EXPECT_EQ(restored.m_Parent, parent);
    EXPECT_EQ(restored.m_FirstChild, firstChild);
    EXPECT_EQ(restored.m_LastChild, asge::ecs::Entity::Null());
    EXPECT_EQ(restored.m_PrevSibling, asge::ecs::Entity::Null());
    EXPECT_EQ(restored.m_NextSibling, asge::ecs::Entity::Null());
}

TEST(HierarchySerializerTest, FromToml_MissingKeysDefaultEveryFieldToNullEntity)
{
    TOMLBuilder builder;
    builder.Table("Hierarchy"); // present but empty

    // Present in the context but never referenced by the (empty) table --
    // proves the default comes from the missing key, not an empty context.
    asge::game::scene::LoadContext ctx;
    ctx.m_Entities[10] = asge::ecs::Entity{ 3, 0 };

    Hierarchy const restored = Serializer<Hierarchy>::FromToml( builder, ctx );
    EXPECT_EQ(restored.m_Parent, asge::ecs::Entity::Null());
    EXPECT_EQ(restored.m_FirstChild, asge::ecs::Entity::Null());
    EXPECT_EQ(restored.m_LastChild, asge::ecs::Entity::Null());
    EXPECT_EQ(restored.m_PrevSibling, asge::ecs::Entity::Null());
    EXPECT_EQ(restored.m_NextSibling, asge::ecs::Entity::Null());
}

TEST(HierarchySerializerTest, RoundTripsThroughToTomlAndFromTomlWithMatchingContexts)
{
    TOMLBuilder builder;
    asge::ecs::Entity const parent{ 1, 0 };
    asge::ecs::Entity const firstChild{ 2, 0 };
    asge::ecs::Entity const lastChild{ 3, 0 };
    asge::ecs::Entity const prevSibling{ 4, 0 };
    asge::ecs::Entity const nextSibling{ 5, 0 };

    asge::game::scene::SaveContext saveCtx;
    saveCtx.m_Ids[parent] = 0;
    saveCtx.m_Ids[firstChild] = 1;
    saveCtx.m_Ids[lastChild] = 2;
    saveCtx.m_Ids[prevSibling] = 3;
    saveCtx.m_Ids[nextSibling] = 4;

    Hierarchy const original{
        .m_Parent = parent, .m_FirstChild = firstChild, .m_LastChild = lastChild,
        .m_NextSibling = nextSibling, .m_PrevSibling = prevSibling
    };
    Serializer<Hierarchy>::ToToml( original, builder, saveCtx );

    // A real Load() maps the same saved ids onto freshly created entities,
    // never the originals -- use different Entity values here than saveCtx
    // did, so this only passes if FromToml actually performs the id lookup
    // rather than smuggling the original handle through some other path.
    asge::ecs::Entity const newParent{ 10, 0 };
    asge::ecs::Entity const newFirstChild{ 11, 0 };
    asge::ecs::Entity const newLastChild{ 12, 0 };
    asge::ecs::Entity const newPrevSibling{ 13, 0 };
    asge::ecs::Entity const newNextSibling{ 14, 0 };

    asge::game::scene::LoadContext loadCtx;
    loadCtx.m_Entities[0] = newParent;
    loadCtx.m_Entities[1] = newFirstChild;
    loadCtx.m_Entities[2] = newLastChild;
    loadCtx.m_Entities[3] = newPrevSibling;
    loadCtx.m_Entities[4] = newNextSibling;

    Hierarchy const restored = Serializer<Hierarchy>::FromToml( builder, loadCtx );
    EXPECT_EQ(restored.m_Parent, newParent);
    EXPECT_EQ(restored.m_FirstChild, newFirstChild);
    EXPECT_EQ(restored.m_LastChild, newLastChild);
    EXPECT_EQ(restored.m_PrevSibling, newPrevSibling);
    EXPECT_EQ(restored.m_NextSibling, newNextSibling);
}

// ─── RGBA_Color ─────────────────────────────────────────────────────────────

TEST(RGBAColorSerializerTest, ToToml_WritesRGBAFieldsAsInts)
{
    TOMLBuilder builder;
    Serializer<asge::graphics::RGBA_Color>::ToToml( { 10, 20, 30, 40 }, builder );

    auto const dump = builder.ToString();
    EXPECT_NE(dump.find("m_Red = 10"), std::string::npos);
    EXPECT_NE(dump.find("m_Green = 20"), std::string::npos);
    EXPECT_NE(dump.find("m_Blue = 30"), std::string::npos);
    EXPECT_NE(dump.find("m_Alpha = 40"), std::string::npos);
}

TEST(RGBAColorSerializerTest, RoundTripsThroughToTomlAndFromToml)
{
    TOMLBuilder builder;
    asge::graphics::RGBA_Color const original{ 12, 34, 56, 78 };
    Serializer<asge::graphics::RGBA_Color>::ToToml( original, builder );

    asge::graphics::RGBA_Color const restored = Serializer<asge::graphics::RGBA_Color>::FromToml( builder );
    EXPECT_EQ(restored.r, original.r);
    EXPECT_EQ(restored.g, original.g);
    EXPECT_EQ(restored.b, original.b);
    EXPECT_EQ(restored.a, original.a);
}

TEST(RGBAColorSerializerTest, FromToml_MissingKeysFallBackToOpaqueWhiteDefault)
{
    TOMLBuilder builder; // no keys set at all

    asge::graphics::RGBA_Color const restored = Serializer<asge::graphics::RGBA_Color>::FromToml( builder );
    EXPECT_EQ(restored.r, 255);
    EXPECT_EQ(restored.g, 255);
    EXPECT_EQ(restored.b, 255);
    EXPECT_EQ(restored.a, 255);
}

// ─── UIButton ───────────────────────────────────────────────────────────────

TEST(UIButtonSerializerTest, ToToml_WritesFieldsUnderUIButtonTable)
{
    TOMLBuilder builder;
    UIButton value{};
    value.m_FontVirtualPath = "fonts/ui.ttf";
    value.m_Text = "Start";
    value.m_TextAlignment = asge::str::TextAlign::Center;
    value.m_Size = asge::math::Float2{ 100.0f, 30.0f };
    Serializer<UIButton>::ToToml( value, builder, asge::game::scene::SaveContext{} );

    auto const dump = builder.ToString();
    EXPECT_NE(dump.find("[UIButton]"), std::string::npos);
    EXPECT_NE(dump.find(R"(m_FontVirtualPath = "fonts/ui.ttf")"), std::string::npos);
    EXPECT_NE(dump.find(R"(m_Text = "Start")"), std::string::npos);
    EXPECT_NE(dump.find(R"(m_TextAlign = "center")"), std::string::npos);
    EXPECT_NE(dump.find("m_SizeX = 100.0"), std::string::npos);
    EXPECT_NE(dump.find("m_SizeY = 30.0"), std::string::npos);
}

TEST(UIButtonSerializerTest, RoundTripsThroughToTomlAndFromToml)
{
    TOMLBuilder builder;
    UIButton original{};
    original.m_FontVirtualPath = "fonts/ui.ttf";
    original.m_Text = "Quit";
    original.m_TextAlignment = asge::str::TextAlign::Right;
    original.m_Size = asge::math::Float2{ 64.0f, 20.0f };
    Serializer<UIButton>::ToToml( original, builder, asge::game::scene::SaveContext{} );

    UIButton const restored = Serializer<UIButton>::FromToml( builder, asge::game::scene::LoadContext{} );
    EXPECT_EQ(restored.m_FontVirtualPath, original.m_FontVirtualPath);
    EXPECT_EQ(restored.m_Text, original.m_Text);
    EXPECT_EQ(restored.m_TextAlignment, original.m_TextAlignment);
    EXPECT_FLOAT_EQ(restored.m_Size.x(), original.m_Size.x());
    EXPECT_FLOAT_EQ(restored.m_Size.y(), original.m_Size.y());
}

TEST(UIButtonSerializerTest, RoundTrips_ColorHoverColorAndPressedColor)
{
    TOMLBuilder builder;
    UIButton original{};
    original.m_Color = { 1, 2, 3, 255 };
    original.m_HoverColor = { 4, 5, 6, 255 };
    original.m_PressedColor = { 7, 8, 9, 255 };
    Serializer<UIButton>::ToToml( original, builder, asge::game::scene::SaveContext{} );

    UIButton const restored = Serializer<UIButton>::FromToml( builder, asge::game::scene::LoadContext{} );
    EXPECT_EQ(restored.m_Color.r, 1);
    EXPECT_EQ(restored.m_Color.g, 2);
    EXPECT_EQ(restored.m_Color.b, 3);
    EXPECT_EQ(restored.m_HoverColor.r, 4);
    EXPECT_EQ(restored.m_HoverColor.g, 5);
    EXPECT_EQ(restored.m_HoverColor.b, 6);
    EXPECT_EQ(restored.m_PressedColor.r, 7);
    EXPECT_EQ(restored.m_PressedColor.g, 8);
    EXPECT_EQ(restored.m_PressedColor.b, 9);
}

TEST(UIButtonSerializerTest, FromToml_MissingKeysFallBackToStructDefaults)
{
    TOMLBuilder builder;
    builder.Table("UIButton"); // present but empty

    UIButton const restored = Serializer<UIButton>::FromToml( builder, asge::game::scene::LoadContext{} );
    EXPECT_TRUE(restored.m_FontVirtualPath.empty());
    EXPECT_EQ(restored.m_Text, "Click Me"); // UIButton's own default, not empty
    EXPECT_EQ(restored.m_TextAlignment, asge::str::TextAlign::None);
    EXPECT_FLOAT_EQ(restored.m_Size.x(), 80.0f);
    EXPECT_FLOAT_EQ(restored.m_Size.y(), 24.0f);
}

TEST(UIButtonSerializerTest, FromToml_UnrecognizedTextAlignValueBecomesNone)
{
    TOMLBuilder builder;
    builder.Table("UIButton").Set<std::string>( "m_TextAlign", "diagonal" );

    UIButton const restored = Serializer<UIButton>::FromToml( builder, asge::game::scene::LoadContext{} );
    EXPECT_EQ(restored.m_TextAlignment, asge::str::TextAlign::None);
}

TEST(UIButtonSerializerTest, FromToml_ClickAndHoverStateAlwaysResetToStructDefaults)
{
    // Serializer<UIButton> only round-trips the four authored fields --
    // FromToml must never carry over m_Hovered/m_Held (neither
    // is something a scene file describes) or reconstruct m_OnClick's
    // subscribers, since a Signal isn't serializable at all.
    TOMLBuilder builder;
    Serializer<UIButton>::ToToml( UIButton{ .m_Text = "Ok" }, builder, asge::game::scene::SaveContext{} );

    UIButton const restored = Serializer<UIButton>::FromToml( builder, asge::game::scene::LoadContext{} );
    EXPECT_FALSE(restored.m_Hovered);
    EXPECT_FALSE(restored.m_Held);
}

// ─── RenderInfo ─────────────────────────────────────────────────────────────

TEST(RenderInfoSerializerTest, ToToml_WritesAllFieldsUnderRenderInfoTable)
{
    TOMLBuilder builder;
    Serializer<RenderInfo>::ToToml(
        RenderInfo{
            .m_Layer = 3, .m_YSort = true, .m_ScreenSpace = true,
            .m_InheritSortFromParent = true, .m_LocalOrder = -1
        },
        builder, asge::game::scene::SaveContext{} );

    auto const dump = builder.ToString();
    EXPECT_NE(dump.find("[RenderInfo]"), std::string::npos);
    EXPECT_NE(dump.find("m_Layer = 3"), std::string::npos);
    EXPECT_NE(dump.find("m_YSort = true"), std::string::npos);
    EXPECT_NE(dump.find("m_ScreenSpace = true"), std::string::npos);
    EXPECT_NE(dump.find("m_InheritSortFromParent = true"), std::string::npos);
    EXPECT_NE(dump.find("m_LocalOrder = -1"), std::string::npos);
}

TEST(RenderInfoSerializerTest, RoundTripsThroughToTomlAndFromToml)
{
    TOMLBuilder builder;
    RenderInfo const original{
        .m_Layer = 5, .m_YSort = true, .m_ScreenSpace = true,
        .m_InheritSortFromParent = true, .m_LocalOrder = 2
    };
    Serializer<RenderInfo>::ToToml( original, builder, asge::game::scene::SaveContext{} );

    RenderInfo const restored = Serializer<RenderInfo>::FromToml( builder, asge::game::scene::LoadContext{} );
    EXPECT_EQ(restored.m_Layer, original.m_Layer);
    EXPECT_EQ(restored.m_YSort, original.m_YSort);
    EXPECT_EQ(restored.m_ScreenSpace, original.m_ScreenSpace);
    EXPECT_EQ(restored.m_InheritSortFromParent, original.m_InheritSortFromParent);
    EXPECT_EQ(restored.m_LocalOrder, original.m_LocalOrder);
}

TEST(RenderInfoSerializerTest, FromToml_MissingKeysFallBackToStructDefaults)
{
    TOMLBuilder builder;
    builder.Table("RenderInfo"); // present but empty

    RenderInfo const restored = Serializer<RenderInfo>::FromToml( builder, asge::game::scene::LoadContext{} );
    EXPECT_EQ(restored.m_Layer, 0);
    EXPECT_FALSE(restored.m_YSort);
    EXPECT_FALSE(restored.m_ScreenSpace);
    EXPECT_FALSE(restored.m_InheritSortFromParent);
    EXPECT_EQ(restored.m_LocalOrder, 0);
}

}
