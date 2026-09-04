#include <ASGE/Game/Assets/AssetManager.hpp>
#include <ASGE/Core/Filesystem/VirtualFileSystem.hpp>
#include <ASGE/Core/ECS/Registry.hpp>
#include <ASGE/Game/Components/Sprite.hpp>
#include <ASGE/Game/Components/Animation.hpp>
#include <ASGE/Video/Graphics/Renderer.hpp>
#include <ASGE/Core/Errors.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

// A hand-built, minimal 2x2 solid-red 24bpp BMP -- same fixture as
// ImageTests.cpp's MakeSolidRedBmp, duplicated locally so this suite doesn't
// need to reach into another test target's translation unit.
namespace
{

using namespace asge::game::asset;
using asge::errors::VfsError;
using asge::errors::ImageError;

std::vector<std::byte> MakeSolidRedBmp()
{
    auto push_u16 = [](std::vector<std::byte>& out, std::uint16_t v) {
        out.push_back(static_cast<std::byte>(v & 0xFF));
        out.push_back(static_cast<std::byte>((v >> 8) & 0xFF));
    };
    auto push_u32 = [](std::vector<std::byte>& out, std::uint32_t v) {
        out.push_back(static_cast<std::byte>(v & 0xFF));
        out.push_back(static_cast<std::byte>((v >> 8) & 0xFF));
        out.push_back(static_cast<std::byte>((v >> 16) & 0xFF));
        out.push_back(static_cast<std::byte>((v >> 24) & 0xFF));
    };

    constexpr std::uint32_t kPixelDataSize = 16; // 2 rows * 8 bytes (6 data + 2 padding)
    constexpr std::uint32_t kFileSize = 54 + kPixelDataSize;

    std::vector<std::byte> bmp;
    bmp.reserve(kFileSize);

    // BITMAPFILEHEADER (14 bytes)
    bmp.push_back(std::byte{'B'});
    bmp.push_back(std::byte{'M'});
    push_u32(bmp, kFileSize);
    push_u32(bmp, 0); // reserved
    push_u32(bmp, 54); // pixel data offset

    // BITMAPINFOHEADER (40 bytes)
    push_u32(bmp, 40); // header size
    push_u32(bmp, 2);  // width
    push_u32(bmp, 2);  // height (positive = bottom-up source rows)
    push_u16(bmp, 1);  // planes
    push_u16(bmp, 24); // bits per pixel
    push_u32(bmp, 0);  // compression = BI_RGB
    push_u32(bmp, kPixelDataSize);
    push_u32(bmp, 0);  // x pixels per meter
    push_u32(bmp, 0);  // y pixels per meter
    push_u32(bmp, 0);  // colors used
    push_u32(bmp, 0);  // important colors

    // Pixel data: 2 rows, each "B G R B G R <2 bytes padding>" for solid red
    for (int row = 0; row < 2; ++row)
    {
        for (int col = 0; col < 2; ++col)
        {
            bmp.push_back(std::byte{0x00}); // B
            bmp.push_back(std::byte{0x00}); // G
            bmp.push_back(std::byte{0xFF}); // R
        }
        bmp.push_back(std::byte{0x00}); // row padding to a multiple of 4 bytes
        bmp.push_back(std::byte{0x00});
    }

    return bmp;
}

class AssetManagerTest : public ::testing::Test
{
protected:
    std::filesystem::path m_ImagesDir;
    asge::filesystem::VirtualFileSystem m_Vfs;

    void SetUp() override
    {
        auto const uniqueName = "asge_assetmanager_test_"
            + std::to_string(reinterpret_cast<std::uintptr_t>(this));
        m_ImagesDir = std::filesystem::temp_directory_path() / uniqueName;
        std::filesystem::create_directories(m_ImagesDir);

        WriteBytes(m_ImagesDir / "hero.bmp", MakeSolidRedBmp());
        WriteText(m_ImagesDir / "garbage.bmp", "not a bmp");
        WriteText(m_ImagesDir / "walk.toml",
            "[FrameTable]\n"
            "x = 0.0\n"
            "y = 0.0\n"
            "w = 8.0\n"
            "h = 8.0\n"
            "columns = 2\n"
            "count = 4\n"
        );

        ASSERT_TRUE(m_Vfs.Mount("images", m_ImagesDir.string()).IsOk());
        ASSERT_TRUE(m_Vfs.Mount("fonts", std::string(ASGE_TEST_FONTS_DIR)).IsOk());
    }

    void TearDown() override
    {
        std::error_code ec;
        std::filesystem::remove_all(m_ImagesDir, ec);
    }

    static void WriteBytes(std::filesystem::path const& inPath, std::vector<std::byte> const& inBytes)
    {
        std::ofstream file(inPath, std::ios::trunc | std::ios::binary);
        file.write(reinterpret_cast<char const*>(inBytes.data()),
            static_cast<std::streamsize>(inBytes.size()));
    }

    static void WriteText(std::filesystem::path const& inPath, std::string const& inContent)
    {
        std::ofstream file(inPath, std::ios::trunc | std::ios::binary);
        file << inContent;
    }
};

// ─── GetImage ──────────────────────────────────────────────────────────────────

TEST_F(AssetManagerTest, GetImage_ValidPathReturnsDecodedImage)
{
    AssetManager mgr(m_Vfs);

    auto result = mgr.GetImage("images/hero.bmp");
    ASSERT_TRUE(result.IsOk());
    EXPECT_EQ(result.Value()->Get().Dimensions().x(), 2);
    EXPECT_EQ(result.Value()->Get().Dimensions().y(), 2);
    EXPECT_EQ(result.Value()->VirtualPath(), "images/hero.bmp");
}

TEST_F(AssetManagerTest, GetImage_UnresolvableVirtualPathReturnsNotMountedError)
{
    AssetManager mgr(m_Vfs);

    auto result = mgr.GetImage("images/missing.bmp");
    ASSERT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), make_error_code(VfsError::NotMounted));
}

TEST_F(AssetManagerTest, GetImage_GarbageBytesReturnsDecodeFailedError)
{
    AssetManager mgr(m_Vfs);

    auto result = mgr.GetImage("images/garbage.bmp");
    ASSERT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), make_error_code(ImageError::DecodeFailed));
}

TEST_F(AssetManagerTest, GetImage_SamePathTwiceReturnsSameCachedAsset)
{
    AssetManager mgr(m_Vfs);

    auto first = mgr.GetImage("images/hero.bmp");
    auto second = mgr.GetImage("images/hero.bmp");
    ASSERT_TRUE(first.IsOk());
    ASSERT_TRUE(second.IsOk());
    EXPECT_EQ(first.Value(), second.Value());
}

// ─── GetFont ───────────────────────────────────────────────────────────────────

TEST_F(AssetManagerTest, GetFont_ValidPathReturnsBakedFont)
{
    AssetManager mgr(m_Vfs);

    auto result = mgr.GetFont("fonts/Ahem.ttf", 32);
    ASSERT_TRUE(result.IsOk());
    EXPECT_GT(result.Value()->Get().GetLineHeight(), 0);
    EXPECT_EQ(result.Value()->VirtualPath(), "fonts/Ahem.ttf");
}

TEST_F(AssetManagerTest, GetFont_NonExistentPathReturnsNotMountedError)
{
    AssetManager mgr(m_Vfs);

    auto result = mgr.GetFont("fonts/DoesNotExist.ttf", 32);
    ASSERT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), make_error_code(VfsError::NotMounted));
}

TEST_F(AssetManagerTest, GetFont_DifferentPixelHeightsAreSeparateCacheEntries)
{
    AssetManager mgr(m_Vfs);

    auto small = mgr.GetFont("fonts/Ahem.ttf", 16);
    auto large = mgr.GetFont("fonts/Ahem.ttf", 32);
    ASSERT_TRUE(small.IsOk());
    ASSERT_TRUE(large.IsOk());
    EXPECT_NE(small.Value(), large.Value());
}

TEST_F(AssetManagerTest, GetFont_SamePathAndHeightTwiceReturnsSameCachedAsset)
{
    AssetManager mgr(m_Vfs);

    auto first = mgr.GetFont("fonts/Ahem.ttf", 32);
    auto second = mgr.GetFont("fonts/Ahem.ttf", 32);
    ASSERT_TRUE(first.IsOk());
    ASSERT_TRUE(second.IsOk());
    EXPECT_EQ(first.Value(), second.Value());
}

// ─── GetFrameTable ───────────────────────────────────────────────────────────

TEST_F(AssetManagerTest, GetFrameTable_ValidPathReturnsLoadedFrameTable)
{
    AssetManager mgr(m_Vfs);

    auto result = mgr.GetFrameTable("images/walk.toml");
    ASSERT_TRUE(result.IsOk());
    ASSERT_EQ(result.Value()->Get().m_Frames.size(), 4u);
    EXPECT_EQ(result.Value()->VirtualPath(), "images/walk.toml");
}

TEST_F(AssetManagerTest, GetFrameTable_UnresolvableVirtualPathReturnsNotMountedError)
{
    AssetManager mgr(m_Vfs);

    auto result = mgr.GetFrameTable("images/missing.toml");
    ASSERT_FALSE(result.IsOk());
    EXPECT_EQ(result.Code(), make_error_code(VfsError::NotMounted));
}

TEST_F(AssetManagerTest, GetFrameTable_SamePathTwiceReturnsSameCachedAsset)
{
    AssetManager mgr(m_Vfs);

    auto first = mgr.GetFrameTable("images/walk.toml");
    auto second = mgr.GetFrameTable("images/walk.toml");
    ASSERT_TRUE(first.IsOk());
    ASSERT_TRUE(second.IsOk());
    EXPECT_EQ(first.Value(), second.Value());
}

// ─── ResolveAssets ───────────────────────────────────────────────────────────

// Minimal ITexture stub tracking how many instances are currently alive, so
// tests can assert on AssetManager actually owning (and eventually freeing)
// what it creates -- not just that Sprite::m_Texture ends up non-null.
class FakeTexture final : public asge::video::ITexture
{
public:
    inline static int s_LiveCount = 0;

    FakeTexture() noexcept { ++s_LiveCount; }
    ~FakeTexture() override { --s_LiveCount; }

    [[nodiscard]] asge::math::Int2 Size() const noexcept override { return { 8, 8 }; }
    [[nodiscard]] void* NativeHandle() const noexcept override { return nullptr; }
    [[nodiscard]] bool IsValid() const noexcept override { return true; }

    void SetColorMod(asge::media::RGBA_Color) noexcept override {}

    [[nodiscard]] asge::Result<asge::media::RGBA_Color> GetColorMod() const noexcept override
    {
        return asge::Result<asge::media::RGBA_Color>::Ok(asge::media::RGBA_Color{});
    }
};

// Minimal IRenderer stub whose only job is CreateTexture; every draw call is
// a no-op since ResolveAssets never issues one. m_FailCreate flips
// CreateTexture to return nullptr, for the "texture creation itself fails"
// path.
class FakeRenderer final : public asge::video::IRenderer
{
public:
    bool m_FailCreate{ false };

    void Clear(asge::media::RGBA_Color const&) const override {}
    void DrawRect(asge::math::Rect const&, asge::media::RGBA_Color const&, bool) const override {}
    void DrawLine(asge::math::Float2 const&, asge::math::Float2 const&,
        asge::media::RGBA_Color const&) const override {}
    void DrawCircle(asge::math::Int2 const&, int, asge::media::RGBA_Color const&, bool) const override {}
    void DrawTexture(asge::video::ITexture const&, asge::math::Rect const&) const noexcept override {}
    void DrawTexture(asge::video::ITexture const&, asge::math::Float2 const&) const noexcept override {}
    void DrawTexture(asge::video::ITexture const&, asge::math::Rect const&,
        asge::math::Rect const&) const noexcept override {}
    void DrawTexture9Grid(asge::video::ITexture const&, float, float, float, float,
        asge::math::Rect const&) const noexcept override {}
    void DrawTextureTiled(asge::video::ITexture const&, float, asge::math::Rect const&) const noexcept override {}
    void DrawTextureAffine(asge::video::ITexture const&, asge::math::Float2 const&,
        asge::math::Float2 const&, asge::math::Float2 const&) const noexcept override {}
    void DrawString(asge::str::StringView, asge::media::Font const&, asge::video::ITexture&,
        asge::math::Float2 const&, asge::media::RGBA_Color const&) const noexcept override {}

    void Present() const override {}

    [[nodiscard]] std::unique_ptr<asge::video::ITexture> CreateTexture(
        asge::media::Image const&) const noexcept override
    {
        return m_FailCreate ? nullptr : std::make_unique<FakeTexture>();
    }

    [[nodiscard]] bool IsValid() const override { return true; }
};

class ResolveAssetsTest : public AssetManagerTest
{
protected:
    asge::ecs::Registry m_Registry;
    FakeRenderer m_Renderer;

    void SetUp() override
    {
        AssetManagerTest::SetUp();
        FakeTexture::s_LiveCount = 0;
    }
};

TEST_F(ResolveAssetsTest, SpriteWithVirtualPathGetsTextureAssigned)
{
    AssetManager mgr(m_Vfs);
    auto entity = m_Registry.CreateEntity();
    ASSERT_TRUE(m_Registry.AddComponent<asge::game::components::Sprite>(
        entity.Value(), { .m_VirtualPath = "images/hero.bmp" }).IsOk());

    mgr.ResolveAssets(m_Registry, m_Renderer);

    auto const& sprite = m_Registry.GetComponent<asge::game::components::Sprite>(entity.Value()).Value().get();
    EXPECT_NE(sprite.m_Texture, nullptr);
}

TEST_F(ResolveAssetsTest, SpriteAlreadyHavingATextureIsLeftUntouched)
{
    AssetManager mgr(m_Vfs);
    auto entity = m_Registry.CreateEntity();
    FakeTexture preExisting;
    ASSERT_TRUE(m_Registry.AddComponent<asge::game::components::Sprite>(
        entity.Value(), { .m_Texture = &preExisting, .m_VirtualPath = "images/hero.bmp" }).IsOk());

    mgr.ResolveAssets(m_Registry, m_Renderer);

    auto const& sprite = m_Registry.GetComponent<asge::game::components::Sprite>(entity.Value()).Value().get();
    EXPECT_EQ(sprite.m_Texture, &preExisting); // Not replaced with a freshly-loaded one
}

TEST_F(ResolveAssetsTest, SpriteWithEmptyVirtualPathIsSkipped)
{
    AssetManager mgr(m_Vfs);
    auto entity = m_Registry.CreateEntity();
    ASSERT_TRUE(m_Registry.AddComponent<asge::game::components::Sprite>(entity.Value(), {}).IsOk());

    mgr.ResolveAssets(m_Registry, m_Renderer);

    auto const& sprite = m_Registry.GetComponent<asge::game::components::Sprite>(entity.Value()).Value().get();
    EXPECT_EQ(sprite.m_Texture, nullptr);
}

TEST_F(ResolveAssetsTest, SpriteWithUnresolvableVirtualPathLeavesTextureNullRatherThanCrashing)
{
    AssetManager mgr(m_Vfs);
    auto entity = m_Registry.CreateEntity();
    ASSERT_TRUE(m_Registry.AddComponent<asge::game::components::Sprite>(
        entity.Value(), { .m_VirtualPath = "images/missing.bmp" }).IsOk());

    mgr.ResolveAssets(m_Registry, m_Renderer);

    auto const& sprite = m_Registry.GetComponent<asge::game::components::Sprite>(entity.Value()).Value().get();
    EXPECT_EQ(sprite.m_Texture, nullptr);
}

TEST_F(ResolveAssetsTest, TextureCreationFailureLeavesSpriteTextureNullRatherThanCrashing)
{
    AssetManager mgr(m_Vfs);
    m_Renderer.m_FailCreate = true;
    auto entity = m_Registry.CreateEntity();
    ASSERT_TRUE(m_Registry.AddComponent<asge::game::components::Sprite>(
        entity.Value(), { .m_VirtualPath = "images/hero.bmp" }).IsOk());

    mgr.ResolveAssets(m_Registry, m_Renderer);

    auto const& sprite = m_Registry.GetComponent<asge::game::components::Sprite>(entity.Value()).Value().get();
    EXPECT_EQ(sprite.m_Texture, nullptr);
}

TEST_F(ResolveAssetsTest, AnimationWithClipPathGetsClipAssigned)
{
    AssetManager mgr(m_Vfs);
    auto entity = m_Registry.CreateEntity();
    ASSERT_TRUE(m_Registry.AddComponent<asge::game::components::Animation>(
        entity.Value(), { .m_ClipPath = "images/walk.toml" }).IsOk());

    mgr.ResolveAssets(m_Registry, m_Renderer);

    auto const& anim = m_Registry.GetComponent<asge::game::components::Animation>(entity.Value()).Value().get();
    ASSERT_NE(anim.m_Clip, nullptr);
    EXPECT_EQ(anim.m_Clip->Get().m_Frames.size(), 4u);
}

TEST_F(ResolveAssetsTest, AnimationAlreadyHavingAClipIsLeftUntouched)
{
    AssetManager mgr(m_Vfs);
    auto entity = m_Registry.CreateEntity();
    auto preResolved = mgr.GetFrameTable("images/walk.toml");
    ASSERT_TRUE(preResolved.IsOk());
    ASSERT_TRUE(m_Registry.AddComponent<asge::game::components::Animation>(
        entity.Value(), { .m_ClipPath = "images/walk.toml", .m_Clip = preResolved.Value() }).IsOk());

    mgr.ResolveAssets(m_Registry, m_Renderer);

    auto const& anim = m_Registry.GetComponent<asge::game::components::Animation>(entity.Value()).Value().get();
    EXPECT_EQ(anim.m_Clip, preResolved.Value());
}

TEST_F(ResolveAssetsTest, AnimationWithEmptyClipPathIsSkipped)
{
    AssetManager mgr(m_Vfs);
    auto entity = m_Registry.CreateEntity();
    ASSERT_TRUE(m_Registry.AddComponent<asge::game::components::Animation>(entity.Value(), {}).IsOk());

    mgr.ResolveAssets(m_Registry, m_Renderer);

    auto const& anim = m_Registry.GetComponent<asge::game::components::Animation>(entity.Value()).Value().get();
    EXPECT_EQ(anim.m_Clip, nullptr);
}

TEST_F(ResolveAssetsTest, CreatedTexturesAreOwnedByAssetManagerAndFreedWithIt)
{
    // Regression guard: ResolveAssets used to call CreateTexture().release()
    // without storing the unique_ptr anywhere, leaking every texture it
    // created. The live-instance count must both go up while the
    // AssetManager holding it is alive, and back down once it's destroyed.
    auto entity = m_Registry.CreateEntity();
    ASSERT_TRUE(m_Registry.AddComponent<asge::game::components::Sprite>(
        entity.Value(), { .m_VirtualPath = "images/hero.bmp" }).IsOk());

    {
        AssetManager mgr(m_Vfs);
        mgr.ResolveAssets(m_Registry, m_Renderer);
        EXPECT_EQ(FakeTexture::s_LiveCount, 1);
    }

    EXPECT_EQ(FakeTexture::s_LiveCount, 0);
}

}
