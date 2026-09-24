#include <ASGE/Game/Assets/AssetResolver.hpp>
#include <ASGE/Core/ECS/Registry.hpp>
#include <ASGE/Game/Components/Sprite.hpp>
#include <ASGE/Game/Components/Animation.hpp>
#include <ASGE/Game/Components/AudioSource.hpp>
#include <ASGE/Game/Components/Transform.hpp>

#include <gtest/gtest.h>

#include <algorithm>

namespace
{

using namespace asge::game::asset;
using namespace asge::game::components;

// ─── AssetRefs<Sprite> ──────────────────────────────────────────────────────

TEST(AssetRefsTest, Sprite_WithVirtualPathReturnsTextureRef)
{
    Sprite sprite{ .m_VirtualPath = "images/hero.bmp" };
    auto ref = AssetRefs<Sprite>{}( sprite );

    ASSERT_TRUE(ref.has_value());
    EXPECT_EQ(ref->m_Kind, AssetKind::Texture);
    EXPECT_EQ(ref->m_VirtualPath, "images/hero.bmp");
}

TEST(AssetRefsTest, Sprite_WithEmptyVirtualPathReturnsNullopt)
{
    Sprite sprite{};
    EXPECT_FALSE(AssetRefs<Sprite>{}( sprite ).has_value());
}

// ─── AssetRefs<Animation> ───────────────────────────────────────────────────

TEST(AssetRefsTest, Animation_WithClipPathReturnsAnimationClipRef)
{
    Animation animation{ .m_ClipPath = "images/walk.toml" };
    auto ref = AssetRefs<Animation>{}( animation );

    ASSERT_TRUE(ref.has_value());
    EXPECT_EQ(ref->m_Kind, AssetKind::AnimationClip);
    EXPECT_EQ(ref->m_VirtualPath, "images/walk.toml");
}

TEST(AssetRefsTest, Animation_WithEmptyClipPathReturnsNullopt)
{
    Animation animation{};
    EXPECT_FALSE(AssetRefs<Animation>{}( animation ).has_value());
}

// ─── AssetRefs<AudioSource> ─────────────────────────────────────────────────

TEST(AssetRefsTest, AudioSource_WithVirtualClipPathReturnsAudioClipRef)
{
    AudioSource audioSource{ .m_VirtualClipPath = "audio/theme.ogg" };
    auto ref = AssetRefs<AudioSource>{}( audioSource );

    ASSERT_TRUE(ref.has_value());
    EXPECT_EQ(ref->m_Kind, AssetKind::AudioClip);
    EXPECT_EQ(ref->m_VirtualPath, "audio/theme.ogg");
}

TEST(AssetRefsTest, AudioSource_WithEmptyVirtualClipPathReturnsNullopt)
{
    AudioSource audioSource{};
    EXPECT_FALSE(AssetRefs<AudioSource>{}( audioSource ).has_value());
}

// ─── AssetRefs<T> — default (no specialization) ────────────────────────────

TEST(AssetRefsTest, ComponentWithNoSpecializationReturnsNullopt)
{
    // Transform owns no asset -- the primary template's default applies,
    // same as Resolver<T>'s no-op default for the same set of component types.
    Transform transform{};
    EXPECT_FALSE(AssetRefs<Transform>{}( transform ).has_value());
}

// ─── CollectAssetRefs ───────────────────────────────────────────────────────

class CollectAssetRefsTest : public ::testing::Test
{
protected:
    asge::ecs::Registry m_Registry;
};

TEST_F(CollectAssetRefsTest, EmptyRegistryReturnsEmpty)
{
    EXPECT_TRUE(CollectAssetRefs(m_Registry).empty());
}

TEST_F(CollectAssetRefsTest, EntityWithNoAssetOwningComponentsContributesNothing)
{
    auto entity = m_Registry.CreateEntity();
    ASSERT_TRUE(entity.IsOk());
    ASSERT_TRUE(m_Registry.AddComponent<Transform>(entity.Value(), {}).IsOk());

    EXPECT_TRUE(CollectAssetRefs(m_Registry).empty());
}

TEST_F(CollectAssetRefsTest, SpriteWithEmptyPathContributesNothing)
{
    auto entity = m_Registry.CreateEntity();
    ASSERT_TRUE(entity.IsOk());
    ASSERT_TRUE(m_Registry.AddComponent<Sprite>(entity.Value(), {}).IsOk());

    EXPECT_TRUE(CollectAssetRefs(m_Registry).empty());
}

TEST_F(CollectAssetRefsTest, CollectsOneRefPerAssetOwningComponentAcrossEntities)
{
    auto spriteEntity = m_Registry.CreateEntity().Value();
    auto animationEntity = m_Registry.CreateEntity().Value();
    auto audioEntity = m_Registry.CreateEntity().Value();

    ASSERT_TRUE(m_Registry.AddComponent<Sprite>(spriteEntity, { .m_VirtualPath = "images/hero.bmp" }).IsOk());
    ASSERT_TRUE(m_Registry.AddComponent<Animation>(animationEntity, { .m_ClipPath = "images/walk.toml" }).IsOk());
    ASSERT_TRUE(m_Registry.AddComponent<AudioSource>(audioEntity, { .m_VirtualClipPath = "audio/theme.ogg" }).IsOk());

    auto refs = CollectAssetRefs(m_Registry);

    ASSERT_EQ(refs.size(), 3u);
    EXPECT_NE(std::find_if(refs.begin(), refs.end(), [](AssetRef const& r) {
        return r.m_Kind == AssetKind::Texture && r.m_VirtualPath == "images/hero.bmp";
    }), refs.end());
    EXPECT_NE(std::find_if(refs.begin(), refs.end(), [](AssetRef const& r) {
        return r.m_Kind == AssetKind::AnimationClip && r.m_VirtualPath == "images/walk.toml";
    }), refs.end());
    EXPECT_NE(std::find_if(refs.begin(), refs.end(), [](AssetRef const& r) {
        return r.m_Kind == AssetKind::AudioClip && r.m_VirtualPath == "audio/theme.ogg";
    }), refs.end());
}

TEST_F(CollectAssetRefsTest, EntityWithMultipleAssetOwningComponentsContributesOneRefEach)
{
    auto entity = m_Registry.CreateEntity().Value();
    ASSERT_TRUE(m_Registry.AddComponent<Sprite>(entity, { .m_VirtualPath = "images/hero.bmp" }).IsOk());
    ASSERT_TRUE(m_Registry.AddComponent<Animation>(entity, { .m_ClipPath = "images/walk.toml" }).IsOk());

    auto refs = CollectAssetRefs(m_Registry);

    ASSERT_EQ(refs.size(), 2u);
    EXPECT_NE(std::find_if(refs.begin(), refs.end(), [](AssetRef const& r) {
        return r.m_Kind == AssetKind::Texture;
    }), refs.end());
    EXPECT_NE(std::find_if(refs.begin(), refs.end(), [](AssetRef const& r) {
        return r.m_Kind == AssetKind::AnimationClip;
    }), refs.end());
}

TEST_F(CollectAssetRefsTest, TwoEntitiesSharingTheSamePathBothContributeARef)
{
    // No deduplication -- that's left to the caller (e.g. inserting into a
    // std::set), same as the level editor's own ReferencedRoots/CollectUsedPaths
    // this is meant to replace did with their own outputs.
    auto first = m_Registry.CreateEntity().Value();
    auto second = m_Registry.CreateEntity().Value();
    ASSERT_TRUE(m_Registry.AddComponent<Sprite>(first, { .m_VirtualPath = "images/hero.bmp" }).IsOk());
    ASSERT_TRUE(m_Registry.AddComponent<Sprite>(second, { .m_VirtualPath = "images/hero.bmp" }).IsOk());

    EXPECT_EQ(CollectAssetRefs(m_Registry).size(), 2u);
}

}
