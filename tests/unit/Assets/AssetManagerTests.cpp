#include <ASGE/Game/Assets/AssetManager.hpp>
#include <ASGE/Core/Filesystem/VirtualFileSystem.hpp>
#include <ASGE/Core/ECS/Registry.hpp>
#include <ASGE/Game/Components/Sprite.hpp>
#include <ASGE/Game/Components/Animation.hpp>
#include <ASGE/Game/Components/AudioSource.hpp>
#include <ASGE/Game/Components/PathFollow.hpp>
#include <ASGE/Video/Graphics/Renderer.hpp>
#include <ASGE/Video/Graphics/Rendering/RenderError.hpp>
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
using asge::errors::RenderError;

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

// A minimal 16-bit PCM mono RIFF/WAVE file -- same fixture as
// AudioClipTests.cpp's MakeMinimalWav, duplicated locally for the same
// reason MakeSolidRedBmp above is.
std::vector<std::byte> MakeMinimalWav()
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
    auto push_tag = [](std::vector<std::byte>& out, char const (&tag)[5]) {
        for (int i = 0; i < 4; ++i) out.push_back(static_cast<std::byte>(tag[i]));
    };

    constexpr std::uint32_t kSampleRate = 8000;
    constexpr std::uint16_t kChannels = 1;
    constexpr std::uint16_t kBitsPerSample = 16;
    constexpr std::int16_t kSamples[] = { 0, 1000, -1000, 0 };
    constexpr std::uint32_t kDataSize = sizeof(kSamples);
    constexpr std::uint16_t kBlockAlign = kChannels * kBitsPerSample / 8;
    constexpr std::uint32_t kByteRate = kSampleRate * kBlockAlign;

    std::vector<std::byte> wav;

    push_tag(wav, "RIFF");
    push_u32(wav, 36 + kDataSize);
    push_tag(wav, "WAVE");

    push_tag(wav, "fmt ");
    push_u32(wav, 16); // fmt chunk size
    push_u16(wav, 1);  // PCM
    push_u16(wav, kChannels);
    push_u32(wav, kSampleRate);
    push_u32(wav, kByteRate);
    push_u16(wav, kBlockAlign);
    push_u16(wav, kBitsPerSample);

    push_tag(wav, "data");
    push_u32(wav, kDataSize);
    for (auto sample : kSamples) push_u16(wav, static_cast<std::uint16_t>(sample));

    return wav;
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
        WriteBytes(m_ImagesDir / "other.bmp", MakeSolidRedBmp());
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
        WriteText(m_ImagesDir / "walk2.toml",
            "[FrameTable]\n"
            "x = 0.0\n"
            "y = 0.0\n"
            "w = 8.0\n"
            "h = 8.0\n"
            "columns = 3\n"
            "count = 6\n"
        );
        WriteBytes(m_ImagesDir / "theme.wav", MakeMinimalWav());
        WriteBytes(m_ImagesDir / "other-theme.wav", MakeMinimalWav());

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

    // Not named small/large -- <windows.h> #defines both as macros (old MIDL
    // type aliases), which silently mangles "auto small = ..." on MSVC.
    auto smallFont = mgr.GetFont("fonts/Ahem.ttf", 16);
    auto largeFont = mgr.GetFont("fonts/Ahem.ttf", 32);
    ASSERT_TRUE(smallFont.IsOk());
    ASSERT_TRUE(largeFont.IsOk());
    EXPECT_NE(smallFont.Value(), largeFont.Value());
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

// ─── GetTexture / ResolveAssets test doubles ────────────────────────────────

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

    void SetColorMod(asge::graphics::RGBA_Color) noexcept override {}

    [[nodiscard]] asge::Result<asge::graphics::RGBA_Color> GetColorMod() const noexcept override
    {
        return asge::Result<asge::graphics::RGBA_Color>::Ok(asge::graphics::RGBA_Color{});
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

    void Clear(asge::graphics::RGBA_Color const&) const override {}
    void DrawRect(asge::math::Rect const&, asge::graphics::RGBA_Color const&, bool) const override {}
    void DrawLine(asge::math::Float2 const&, asge::math::Float2 const&,
        asge::graphics::RGBA_Color const&) const override {}
    void DrawCircle(asge::math::Int2 const&, int, asge::graphics::RGBA_Color const&, bool) const override {}
    void DrawTexture(asge::video::ITexture const&, asge::math::Rect const&) const noexcept override {}
    void DrawTexture(asge::video::ITexture const&, asge::math::Float2 const&) const noexcept override {}
    void DrawTexture(asge::video::ITexture const&, asge::math::Rect const&,
        asge::math::Rect const&) const noexcept override {}
    void DrawTexture9Grid(asge::video::ITexture const&, float, float, float, float,
        asge::math::Rect const&) const noexcept override {}
    void DrawTextureTiled(asge::video::ITexture const&, float, asge::math::Rect const&) const noexcept override {}
    void DrawTextureAffine(asge::video::ITexture const&, asge::math::Float2 const&,
        asge::math::Float2 const&, asge::math::Float2 const&) const noexcept override {}
    void DrawTextureAffine(asge::video::ITexture const&, asge::math::Rect const&, asge::math::Float2 const&,
        asge::math::Float2 const&, asge::math::Float2 const&) const noexcept override {}
    void DrawString(asge::str::StringView, asge::media::Font const&, asge::video::ITexture&,
        asge::math::Float2 const&, asge::graphics::RGBA_Color const&) const noexcept override {}

    void Present() const override {}

    [[nodiscard]] std::unique_ptr<asge::video::ITexture> CreateTexture(
        asge::media::Image const&) const noexcept override
    {
        return m_FailCreate ? nullptr : std::make_unique<FakeTexture>();
    }

    [[nodiscard]] bool IsValid() const override { return true; }

    void SetCamera(asge::video::Camera const& inCamera) override { m_Camera = inCamera; }
    [[nodiscard]] asge::video::Camera const& GetCamera() const override { return m_Camera; }
    void SetViewport(asge::video::Viewport const& inViewport) override { m_Viewport = inViewport; }
    [[nodiscard]] asge::video::Viewport const& GetViewport() const override { return m_Viewport; }

private:
    asge::video::Camera   m_Camera{};
    asge::video::Viewport m_Viewport{};
};

// ─── GetTexture ──────────────────────────────────────────────────────────────

class GetTextureTest : public AssetManagerTest
{
protected:
    FakeRenderer m_Renderer;

    void SetUp() override
    {
        AssetManagerTest::SetUp();
        FakeTexture::s_LiveCount = 0;
    }
};

TEST_F(GetTextureTest, ValidPathReturnsALiveTexture)
{
    AssetManager mgr(m_Vfs);

    auto result = mgr.GetTexture("images/hero.bmp", m_Renderer);

    ASSERT_TRUE(result.IsOk());
    EXPECT_TRUE(result.Value()->IsValid());
    EXPECT_EQ(FakeTexture::s_LiveCount, 1);
}

TEST_F(GetTextureTest, SamePathTwiceReturnsSameCachedTexture)
{
    AssetManager mgr(m_Vfs);

    auto first = mgr.GetTexture("images/hero.bmp", m_Renderer);
    auto second = mgr.GetTexture("images/hero.bmp", m_Renderer);

    ASSERT_TRUE(first.IsOk());
    ASSERT_TRUE(second.IsOk());
    EXPECT_EQ(first.Value(), second.Value());
    EXPECT_EQ(FakeTexture::s_LiveCount, 1);
}

TEST_F(GetTextureTest, DifferentPathsReturnDifferentTextures)
{
    AssetManager mgr(m_Vfs);

    auto first = mgr.GetTexture("images/hero.bmp", m_Renderer);
    auto second = mgr.GetTexture("images/other.bmp", m_Renderer);

    ASSERT_TRUE(first.IsOk());
    ASSERT_TRUE(second.IsOk());
    EXPECT_NE(first.Value(), second.Value());
    EXPECT_EQ(FakeTexture::s_LiveCount, 2);
}

TEST_F(GetTextureTest, UnresolvableVirtualPathReturnsNotMountedErrorAndIsNotCached)
{
    AssetManager mgr(m_Vfs);

    auto first = mgr.GetTexture("images/missing.bmp", m_Renderer);
    ASSERT_FALSE(first.IsOk());
    EXPECT_EQ(first.Code(), make_error_code(VfsError::NotMounted));

    // Not cached as a failure -- a later call retries rather than reusing a stale error.
    auto second = mgr.GetTexture("images/missing.bmp", m_Renderer);
    ASSERT_FALSE(second.IsOk());
    EXPECT_EQ(second.Code(), make_error_code(VfsError::NotMounted));
    EXPECT_EQ(FakeTexture::s_LiveCount, 0);
}

TEST_F(GetTextureTest, RendererFailureReturnsTextureCreationFailedAndIsNotCached)
{
    AssetManager mgr(m_Vfs);
    m_Renderer.m_FailCreate = true;

    auto first = mgr.GetTexture("images/hero.bmp", m_Renderer);
    ASSERT_FALSE(first.IsOk());
    EXPECT_EQ(first.Code(), make_error_code(RenderError::TextureCreationFailed));
    EXPECT_EQ(FakeTexture::s_LiveCount, 0);

    // Not cached as a failure -- a later call, once the renderer recovers, succeeds.
    m_Renderer.m_FailCreate = false;
    auto second = mgr.GetTexture("images/hero.bmp", m_Renderer);
    ASSERT_TRUE(second.IsOk());
    EXPECT_EQ(FakeTexture::s_LiveCount, 1);
}

// ─── UnloadTexture ───────────────────────────────────────────────────────────

TEST_F(GetTextureTest, RemovesTheCacheEntrySoTheNextGetTextureRecreatesIt)
{
    AssetManager mgr(m_Vfs);
    auto first = mgr.GetTexture("images/hero.bmp", m_Renderer);
    ASSERT_TRUE(first.IsOk());
    ASSERT_EQ(FakeTexture::s_LiveCount, 1);

    mgr.UnloadTexture("images/hero.bmp");
    // Freed immediately, not just uncached -- not comparing the stale pointer
    // value against the next GetTexture's result, since a freed-then-reused
    // address could legitimately coincide with it.
    EXPECT_EQ(FakeTexture::s_LiveCount, 0);

    auto second = mgr.GetTexture("images/hero.bmp", m_Renderer);
    ASSERT_TRUE(second.IsOk());
    EXPECT_EQ(FakeTexture::s_LiveCount, 1); // recreated, not still missing
}

TEST_F(GetTextureTest, UnknownPathIsANoOp)
{
    AssetManager mgr(m_Vfs);
    auto first = mgr.GetTexture("images/hero.bmp", m_Renderer);
    ASSERT_TRUE(first.IsOk());

    mgr.UnloadTexture("images/never-loaded.bmp");

    // The unrelated cache entry survives untouched.
    auto second = mgr.GetTexture("images/hero.bmp", m_Renderer);
    ASSERT_TRUE(second.IsOk());
    EXPECT_EQ(second.Value(), first.Value());
    EXPECT_EQ(FakeTexture::s_LiveCount, 1);
}

// ─── ResolveAssets ───────────────────────────────────────────────────────────

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

    auto spriteResult = m_Registry.GetComponent<asge::game::components::Sprite>(entity.Value());
    auto const& sprite = spriteResult.Value().get();
    EXPECT_NE(sprite.m_Texture, nullptr);
}

TEST_F(ResolveAssetsTest, SpriteAlreadyHavingATextureIsLeftUntouched)
{
    AssetManager mgr(m_Vfs);
    auto entity = m_Registry.CreateEntity();
    FakeTexture preExisting;
    ASSERT_TRUE(m_Registry.AddComponent<asge::game::components::Sprite>(
        entity.Value(), { .m_Texture = &preExisting, .m_VirtualPath = "images/hero.bmp",
            .m_ResolvedVirtualPath = "images/hero.bmp" }).IsOk());

    mgr.ResolveAssets(m_Registry, m_Renderer);

    auto spriteResult = m_Registry.GetComponent<asge::game::components::Sprite>(entity.Value());
    auto const& sprite = spriteResult.Value().get();
    EXPECT_EQ(sprite.m_Texture, &preExisting); // Not replaced with a freshly-loaded one
}

TEST_F(ResolveAssetsTest, SpriteWithEmptyVirtualPathIsSkipped)
{
    AssetManager mgr(m_Vfs);
    auto entity = m_Registry.CreateEntity();
    ASSERT_TRUE(m_Registry.AddComponent<asge::game::components::Sprite>(entity.Value(), {}).IsOk());

    mgr.ResolveAssets(m_Registry, m_Renderer);

    auto spriteResult = m_Registry.GetComponent<asge::game::components::Sprite>(entity.Value());
    auto const& sprite = spriteResult.Value().get();
    EXPECT_EQ(sprite.m_Texture, nullptr);
}

TEST_F(ResolveAssetsTest, SpriteWithUnresolvableVirtualPathLeavesTextureNullRatherThanCrashing)
{
    AssetManager mgr(m_Vfs);
    auto entity = m_Registry.CreateEntity();
    ASSERT_TRUE(m_Registry.AddComponent<asge::game::components::Sprite>(
        entity.Value(), { .m_VirtualPath = "images/missing.bmp" }).IsOk());

    mgr.ResolveAssets(m_Registry, m_Renderer);

    auto spriteResult = m_Registry.GetComponent<asge::game::components::Sprite>(entity.Value());
    auto const& sprite = spriteResult.Value().get();
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

    auto spriteResult = m_Registry.GetComponent<asge::game::components::Sprite>(entity.Value());
    auto const& sprite = spriteResult.Value().get();
    EXPECT_EQ(sprite.m_Texture, nullptr);
}

TEST_F(ResolveAssetsTest, TwoSpritesWithTheSameVirtualPathShareOneTexture)
{
    AssetManager mgr(m_Vfs);
    auto entityA = m_Registry.CreateEntity();
    auto entityB = m_Registry.CreateEntity();
    ASSERT_TRUE(m_Registry.AddComponent<asge::game::components::Sprite>(
        entityA.Value(), { .m_VirtualPath = "images/hero.bmp" }).IsOk());
    ASSERT_TRUE(m_Registry.AddComponent<asge::game::components::Sprite>(
        entityB.Value(), { .m_VirtualPath = "images/hero.bmp" }).IsOk());

    mgr.ResolveAssets(m_Registry, m_Renderer);

    auto spriteAResult = m_Registry.GetComponent<asge::game::components::Sprite>(entityA.Value());
    auto spriteBResult = m_Registry.GetComponent<asge::game::components::Sprite>(entityB.Value());
    auto const& spriteA = spriteAResult.Value().get();
    auto const& spriteB = spriteBResult.Value().get();
    ASSERT_NE(spriteA.m_Texture, nullptr);
    EXPECT_EQ(spriteA.m_Texture, spriteB.m_Texture);
    EXPECT_EQ(FakeTexture::s_LiveCount, 1); // one texture shared, not two
}

TEST_F(ResolveAssetsTest, AnimationWithClipPathGetsClipAssigned)
{
    AssetManager mgr(m_Vfs);
    auto entity = m_Registry.CreateEntity();
    ASSERT_TRUE(m_Registry.AddComponent<asge::game::components::Animation>(
        entity.Value(), { .m_ClipPath = "images/walk.toml" }).IsOk());

    mgr.ResolveAssets(m_Registry, m_Renderer);

    auto animResult = m_Registry.GetComponent<asge::game::components::Animation>(entity.Value());
    auto const& anim = animResult.Value().get();
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
        entity.Value(), { .m_ClipPath = "images/walk.toml", .m_Clip = preResolved.Value(),
            .m_ResolvedClipPath = "images/walk.toml" }).IsOk());

    mgr.ResolveAssets(m_Registry, m_Renderer);

    auto animResult = m_Registry.GetComponent<asge::game::components::Animation>(entity.Value());
    auto const& anim = animResult.Value().get();
    EXPECT_EQ(anim.m_Clip, preResolved.Value());
}

TEST_F(ResolveAssetsTest, AnimationWithEmptyClipPathIsSkipped)
{
    AssetManager mgr(m_Vfs);
    auto entity = m_Registry.CreateEntity();
    ASSERT_TRUE(m_Registry.AddComponent<asge::game::components::Animation>(entity.Value(), {}).IsOk());

    mgr.ResolveAssets(m_Registry, m_Renderer);

    auto animResult = m_Registry.GetComponent<asge::game::components::Animation>(entity.Value());
    auto const& anim = animResult.Value().get();
    EXPECT_EQ(anim.m_Clip, nullptr);
}

TEST_F(ResolveAssetsTest, AudioSourceWithVirtualClipPathGetsClipAssigned)
{
    AssetManager mgr(m_Vfs);
    auto entity = m_Registry.CreateEntity();
    ASSERT_TRUE(m_Registry.AddComponent<asge::game::components::AudioSource>(
        entity.Value(), { .m_VirtualClipPath = "images/theme.wav" }).IsOk());

    mgr.ResolveAssets(m_Registry, m_Renderer);

    auto audioResult = m_Registry.GetComponent<asge::game::components::AudioSource>(entity.Value());
    auto const& audioSource = audioResult.Value().get();
    ASSERT_NE(audioSource.m_Clip, nullptr);
    EXPECT_EQ(audioSource.m_Clip->VirtualPath(), "images/theme.wav");
    EXPECT_EQ(audioSource.m_ResolvedVirtualClipPath, "images/theme.wav");
}

TEST_F(ResolveAssetsTest, AudioSourceAlreadyHavingAClipIsLeftUntouched)
{
    AssetManager mgr(m_Vfs);
    auto entity = m_Registry.CreateEntity();
    auto preResolved = mgr.GetAudio("images/theme.wav");
    ASSERT_TRUE(preResolved.IsOk());
    ASSERT_TRUE(m_Registry.AddComponent<asge::game::components::AudioSource>(
        entity.Value(), { .m_Clip = preResolved.Value(), .m_VirtualClipPath = "images/theme.wav",
            .m_ResolvedVirtualClipPath = "images/theme.wav" }).IsOk());

    mgr.ResolveAssets(m_Registry, m_Renderer);

    auto audioResult = m_Registry.GetComponent<asge::game::components::AudioSource>(entity.Value());
    EXPECT_EQ(audioResult.Value().get().m_Clip, preResolved.Value());
}

TEST_F(ResolveAssetsTest, AudioSourceWithEmptyVirtualClipPathIsSkipped)
{
    AssetManager mgr(m_Vfs);
    auto entity = m_Registry.CreateEntity();
    ASSERT_TRUE(m_Registry.AddComponent<asge::game::components::AudioSource>(entity.Value(), {}).IsOk());

    mgr.ResolveAssets(m_Registry, m_Renderer);

    auto audioResult = m_Registry.GetComponent<asge::game::components::AudioSource>(entity.Value());
    EXPECT_EQ(audioResult.Value().get().m_Clip, nullptr);
}

TEST_F(ResolveAssetsTest, PathFollowWithWaypointsGetsPathBuilt)
{
    AssetManager mgr(m_Vfs);
    auto entity = m_Registry.CreateEntity();
    asge::game::components::PathFollow pathFollow;
    pathFollow.m_Waypoints = { { 0.0f, 0.0f }, { 10.0f, 0.0f }, { 10.0f, 10.0f } };
    ASSERT_TRUE(m_Registry.AddComponent( entity.Value(), pathFollow ).IsOk());

    mgr.ResolveAssets(m_Registry, m_Renderer);

    auto pathResult = m_Registry.GetComponent<asge::game::components::PathFollow>(entity.Value());
    auto const& path = pathResult.Value().get();
    EXPECT_TRUE(path.m_Path.HasSegments());
    EXPECT_GT(path.m_Path.Length(), 0.0f);
}

TEST_F(ResolveAssetsTest, PathFollowWithEmptyWaypointsIsSkipped)
{
    AssetManager mgr(m_Vfs);
    auto entity = m_Registry.CreateEntity();
    ASSERT_TRUE(m_Registry.AddComponent<asge::game::components::PathFollow>(entity.Value(), {}).IsOk());

    mgr.ResolveAssets(m_Registry, m_Renderer);

    auto pathResult = m_Registry.GetComponent<asge::game::components::PathFollow>(entity.Value());
    EXPECT_FALSE(pathResult.Value().get().m_Path.HasSegments());
}

TEST_F(ResolveAssetsTest, PathFollowWithOnlyOneWaypointLeavesPathWithoutSegments)
{
    // CatmullRomSpline itself declines to build any segments for fewer than
    // two waypoints -- distinct code path from the empty-vector skip above,
    // both landing on the same "no segments" outcome.
    AssetManager mgr(m_Vfs);
    auto entity = m_Registry.CreateEntity();
    asge::game::components::PathFollow pathFollow;
    pathFollow.m_Waypoints = { { 5.0f, 5.0f } };
    ASSERT_TRUE(m_Registry.AddComponent( entity.Value(), pathFollow ).IsOk());

    mgr.ResolveAssets(m_Registry, m_Renderer);

    auto pathResult = m_Registry.GetComponent<asge::game::components::PathFollow>(entity.Value());
    EXPECT_FALSE(pathResult.Value().get().m_Path.HasSegments());
}

TEST_F(ResolveAssetsTest, PathFollowAlreadyHavingAPathIsLeftUntouched)
{
    // Regression guard: Resolver<PathFollow> used to rebuild the spline --
    // recomputing every segment's arc-length table -- on every single
    // ResolveAssets call rather than just the first, unlike every other
    // resolver here, which skips an entity that's already resolved. Since
    // ResolveAssets runs every frame by convention (see e.g. animation_demo),
    // that meant rebuilding the whole path from scratch every frame for
    // every path-following entity.
    AssetManager mgr(m_Vfs);
    auto entity = m_Registry.CreateEntity();
    asge::game::components::PathFollow pathFollow;
    pathFollow.m_Waypoints = { { 0.0f, 0.0f }, { 10.0f, 0.0f }, { 10.0f, 10.0f } };
    ASSERT_TRUE(m_Registry.AddComponent( entity.Value(), pathFollow ).IsOk());

    mgr.ResolveAssets(m_Registry, m_Renderer);
    float const lengthAfterFirstResolve =
        m_Registry.GetComponent<asge::game::components::PathFollow>(entity.Value()).Value().get().m_Path.Length();

    // If it were to rebuild, the spline would now run through these very
    // different waypoints and its Length() would change accordingly.
    m_Registry.GetComponent<asge::game::components::PathFollow>(entity.Value()).Value().get().m_Waypoints =
        { { 0.0f, 0.0f }, { 100.0f, 0.0f }, { 100.0f, 100.0f } };

    mgr.ResolveAssets(m_Registry, m_Renderer);

    auto pathResult = m_Registry.GetComponent<asge::game::components::PathFollow>(entity.Value());
    EXPECT_FLOAT_EQ(pathResult.Value().get().m_Path.Length(), lengthAfterFirstResolve);
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

// ─── Re-resolving on a path change (issue #99) ───────────────────────────────

TEST_F(ResolveAssetsTest, Sprite_RepointingVirtualPathResolvesTheNewTexture)
{
    AssetManager mgr(m_Vfs);
    auto entity = m_Registry.CreateEntity();
    ASSERT_TRUE(m_Registry.AddComponent<asge::game::components::Sprite>(
        entity.Value(), { .m_VirtualPath = "images/hero.bmp" }).IsOk());
    mgr.ResolveAssets(m_Registry, m_Renderer);
    auto* firstTexture = m_Registry.GetComponent<asge::game::components::Sprite>(entity.Value()).Value().get().m_Texture;
    ASSERT_NE(firstTexture, nullptr);

    m_Registry.GetComponent<asge::game::components::Sprite>(entity.Value()).Value().get().m_VirtualPath = "images/other.bmp";
    mgr.ResolveAssets(m_Registry, m_Renderer);

    auto spriteResult = m_Registry.GetComponent<asge::game::components::Sprite>(entity.Value());
    auto const& sprite = spriteResult.Value().get();
    ASSERT_NE(sprite.m_Texture, nullptr);
    EXPECT_NE(sprite.m_Texture, firstTexture); // a distinct cache entry for "other.bmp"
    EXPECT_EQ(sprite.m_ResolvedVirtualPath, "images/other.bmp");
}

TEST_F(ResolveAssetsTest, Sprite_ClearingVirtualPathReleasesTheTexture)
{
    AssetManager mgr(m_Vfs);
    auto entity = m_Registry.CreateEntity();
    ASSERT_TRUE(m_Registry.AddComponent<asge::game::components::Sprite>(
        entity.Value(), { .m_VirtualPath = "images/hero.bmp" }).IsOk());
    mgr.ResolveAssets(m_Registry, m_Renderer);
    ASSERT_NE(m_Registry.GetComponent<asge::game::components::Sprite>(entity.Value()).Value().get().m_Texture, nullptr);

    m_Registry.GetComponent<asge::game::components::Sprite>(entity.Value()).Value().get().m_VirtualPath.clear();
    mgr.ResolveAssets(m_Registry, m_Renderer);

    auto spriteResult = m_Registry.GetComponent<asge::game::components::Sprite>(entity.Value());
    auto const& sprite = spriteResult.Value().get();
    EXPECT_EQ(sprite.m_Texture, nullptr);
    EXPECT_TRUE(sprite.m_ResolvedVirtualPath.empty());
}

TEST_F(ResolveAssetsTest, Sprite_UnchangedVirtualPathDoesNotReResolve)
{
    AssetManager mgr(m_Vfs);
    auto entity = m_Registry.CreateEntity();
    ASSERT_TRUE(m_Registry.AddComponent<asge::game::components::Sprite>(
        entity.Value(), { .m_VirtualPath = "images/hero.bmp" }).IsOk());
    mgr.ResolveAssets(m_Registry, m_Renderer);

    // A sentinel the resolver did not create -- if the unchanged-path guard
    // didn't skip, the next ResolveAssets call would overwrite it back with
    // AssetManager's own cached "hero.bmp" texture.
    FakeTexture sentinel;
    m_Registry.GetComponent<asge::game::components::Sprite>(entity.Value()).Value().get().m_Texture = &sentinel;

    mgr.ResolveAssets(m_Registry, m_Renderer);

    auto spriteResult = m_Registry.GetComponent<asge::game::components::Sprite>(entity.Value());
    EXPECT_EQ(spriteResult.Value().get().m_Texture, &sentinel);
}

TEST_F(ResolveAssetsTest, Sprite_FailedRepointLeavesOldTextureAndRetriesNextCall)
{
    AssetManager mgr(m_Vfs);
    auto entity = m_Registry.CreateEntity();
    ASSERT_TRUE(m_Registry.AddComponent<asge::game::components::Sprite>(
        entity.Value(), { .m_VirtualPath = "images/hero.bmp" }).IsOk());
    mgr.ResolveAssets(m_Registry, m_Renderer);
    auto* originalTexture = m_Registry.GetComponent<asge::game::components::Sprite>(entity.Value()).Value().get().m_Texture;
    ASSERT_NE(originalTexture, nullptr);

    m_Registry.GetComponent<asge::game::components::Sprite>(entity.Value()).Value().get().m_VirtualPath = "images/missing.bmp";
    mgr.ResolveAssets(m_Registry, m_Renderer);

    {
        auto spriteResult = m_Registry.GetComponent<asge::game::components::Sprite>(entity.Value());
        auto const& sprite = spriteResult.Value().get();
        EXPECT_EQ(sprite.m_Texture, originalTexture); // stale but not nulled out on a failed repoint
        EXPECT_EQ(sprite.m_ResolvedVirtualPath, "images/hero.bmp"); // not advanced -- next call retries
    }

    // Fix the path -- the earlier failure must not have gotten permanently "stuck".
    m_Registry.GetComponent<asge::game::components::Sprite>(entity.Value()).Value().get().m_VirtualPath = "images/other.bmp";
    mgr.ResolveAssets(m_Registry, m_Renderer);

    auto spriteResult = m_Registry.GetComponent<asge::game::components::Sprite>(entity.Value());
    auto const& sprite = spriteResult.Value().get();
    EXPECT_NE(sprite.m_Texture, originalTexture);
    EXPECT_EQ(sprite.m_ResolvedVirtualPath, "images/other.bmp");
}

TEST_F(ResolveAssetsTest, Animation_RepointingClipPathResolvesTheNewClip)
{
    AssetManager mgr(m_Vfs);
    auto entity = m_Registry.CreateEntity();
    ASSERT_TRUE(m_Registry.AddComponent<asge::game::components::Animation>(
        entity.Value(), { .m_ClipPath = "images/walk.toml" }).IsOk());
    mgr.ResolveAssets(m_Registry, m_Renderer);
    ASSERT_NE(m_Registry.GetComponent<asge::game::components::Animation>(entity.Value()).Value().get().m_Clip, nullptr);

    m_Registry.GetComponent<asge::game::components::Animation>(entity.Value()).Value().get().m_ClipPath = "images/walk2.toml";
    mgr.ResolveAssets(m_Registry, m_Renderer);

    auto animResult = m_Registry.GetComponent<asge::game::components::Animation>(entity.Value());
    auto const& anim = animResult.Value().get();
    ASSERT_NE(anim.m_Clip, nullptr);
    EXPECT_EQ(anim.m_Clip->VirtualPath(), "images/walk2.toml");
    EXPECT_EQ(anim.m_Clip->Get().m_Frames.size(), 6u);
    EXPECT_EQ(anim.m_ResolvedClipPath, "images/walk2.toml");
}

TEST_F(ResolveAssetsTest, Animation_ClearingClipPathReleasesTheClip)
{
    AssetManager mgr(m_Vfs);
    auto entity = m_Registry.CreateEntity();
    ASSERT_TRUE(m_Registry.AddComponent<asge::game::components::Animation>(
        entity.Value(), { .m_ClipPath = "images/walk.toml" }).IsOk());
    mgr.ResolveAssets(m_Registry, m_Renderer);
    ASSERT_NE(m_Registry.GetComponent<asge::game::components::Animation>(entity.Value()).Value().get().m_Clip, nullptr);

    m_Registry.GetComponent<asge::game::components::Animation>(entity.Value()).Value().get().m_ClipPath.clear();
    mgr.ResolveAssets(m_Registry, m_Renderer);

    auto animResult = m_Registry.GetComponent<asge::game::components::Animation>(entity.Value());
    auto const& anim = animResult.Value().get();
    EXPECT_EQ(anim.m_Clip, nullptr);
    EXPECT_TRUE(anim.m_ResolvedClipPath.empty());
}

TEST_F(ResolveAssetsTest, Animation_UnchangedClipPathDoesNotReResolve)
{
    AssetManager mgr(m_Vfs);
    auto entity = m_Registry.CreateEntity();
    ASSERT_TRUE(m_Registry.AddComponent<asge::game::components::Animation>(
        entity.Value(), { .m_ClipPath = "images/walk.toml" }).IsOk());
    mgr.ResolveAssets(m_Registry, m_Renderer);

    auto otherClip = mgr.GetFrameTable("images/walk2.toml");
    ASSERT_TRUE(otherClip.IsOk());
    m_Registry.GetComponent<asge::game::components::Animation>(entity.Value()).Value().get().m_Clip = otherClip.Value();

    mgr.ResolveAssets(m_Registry, m_Renderer);

    auto animResult = m_Registry.GetComponent<asge::game::components::Animation>(entity.Value());
    EXPECT_EQ(animResult.Value().get().m_Clip, otherClip.Value());
}

TEST_F(ResolveAssetsTest, Animation_FailedRepointLeavesOldClipAndRetriesNextCall)
{
    AssetManager mgr(m_Vfs);
    auto entity = m_Registry.CreateEntity();
    ASSERT_TRUE(m_Registry.AddComponent<asge::game::components::Animation>(
        entity.Value(), { .m_ClipPath = "images/walk.toml" }).IsOk());
    mgr.ResolveAssets(m_Registry, m_Renderer);
    auto originalClip = m_Registry.GetComponent<asge::game::components::Animation>(entity.Value()).Value().get().m_Clip;
    ASSERT_NE(originalClip, nullptr);

    m_Registry.GetComponent<asge::game::components::Animation>(entity.Value()).Value().get().m_ClipPath = "images/missing.toml";
    mgr.ResolveAssets(m_Registry, m_Renderer);

    {
        auto animResult = m_Registry.GetComponent<asge::game::components::Animation>(entity.Value());
        auto const& anim = animResult.Value().get();
        EXPECT_EQ(anim.m_Clip, originalClip);
        EXPECT_EQ(anim.m_ResolvedClipPath, "images/walk.toml");
    }

    m_Registry.GetComponent<asge::game::components::Animation>(entity.Value()).Value().get().m_ClipPath = "images/walk2.toml";
    mgr.ResolveAssets(m_Registry, m_Renderer);

    auto animResult = m_Registry.GetComponent<asge::game::components::Animation>(entity.Value());
    auto const& anim = animResult.Value().get();
    EXPECT_NE(anim.m_Clip, originalClip);
    EXPECT_EQ(anim.m_ResolvedClipPath, "images/walk2.toml");
}

TEST_F(ResolveAssetsTest, AudioSource_RepointingVirtualClipPathResolvesTheNewClip)
{
    AssetManager mgr(m_Vfs);
    auto entity = m_Registry.CreateEntity();
    ASSERT_TRUE(m_Registry.AddComponent<asge::game::components::AudioSource>(
        entity.Value(), { .m_VirtualClipPath = "images/theme.wav" }).IsOk());
    mgr.ResolveAssets(m_Registry, m_Renderer);
    ASSERT_NE(m_Registry.GetComponent<asge::game::components::AudioSource>(entity.Value()).Value().get().m_Clip, nullptr);

    m_Registry.GetComponent<asge::game::components::AudioSource>(entity.Value()).Value().get().m_VirtualClipPath = "images/other-theme.wav";
    mgr.ResolveAssets(m_Registry, m_Renderer);

    auto audioResult = m_Registry.GetComponent<asge::game::components::AudioSource>(entity.Value());
    auto const& audioSource = audioResult.Value().get();
    ASSERT_NE(audioSource.m_Clip, nullptr);
    EXPECT_EQ(audioSource.m_Clip->VirtualPath(), "images/other-theme.wav");
    EXPECT_EQ(audioSource.m_ResolvedVirtualClipPath, "images/other-theme.wav");
}

TEST_F(ResolveAssetsTest, AudioSource_ClearingVirtualClipPathReleasesTheClip)
{
    AssetManager mgr(m_Vfs);
    auto entity = m_Registry.CreateEntity();
    ASSERT_TRUE(m_Registry.AddComponent<asge::game::components::AudioSource>(
        entity.Value(), { .m_VirtualClipPath = "images/theme.wav" }).IsOk());
    mgr.ResolveAssets(m_Registry, m_Renderer);
    ASSERT_NE(m_Registry.GetComponent<asge::game::components::AudioSource>(entity.Value()).Value().get().m_Clip, nullptr);

    m_Registry.GetComponent<asge::game::components::AudioSource>(entity.Value()).Value().get().m_VirtualClipPath.clear();
    mgr.ResolveAssets(m_Registry, m_Renderer);

    auto audioResult = m_Registry.GetComponent<asge::game::components::AudioSource>(entity.Value());
    auto const& audioSource = audioResult.Value().get();
    EXPECT_EQ(audioSource.m_Clip, nullptr);
    EXPECT_TRUE(audioSource.m_ResolvedVirtualClipPath.empty());
}

TEST_F(ResolveAssetsTest, AudioSource_UnchangedVirtualClipPathDoesNotReResolve)
{
    AssetManager mgr(m_Vfs);
    auto entity = m_Registry.CreateEntity();
    ASSERT_TRUE(m_Registry.AddComponent<asge::game::components::AudioSource>(
        entity.Value(), { .m_VirtualClipPath = "images/theme.wav" }).IsOk());
    mgr.ResolveAssets(m_Registry, m_Renderer);

    auto otherClip = mgr.GetAudio("images/other-theme.wav");
    ASSERT_TRUE(otherClip.IsOk());
    m_Registry.GetComponent<asge::game::components::AudioSource>(entity.Value()).Value().get().m_Clip = otherClip.Value();

    mgr.ResolveAssets(m_Registry, m_Renderer);

    auto audioResult = m_Registry.GetComponent<asge::game::components::AudioSource>(entity.Value());
    EXPECT_EQ(audioResult.Value().get().m_Clip, otherClip.Value());
}

TEST_F(ResolveAssetsTest, AudioSource_FailedRepointLeavesOldClipAndRetriesNextCall)
{
    AssetManager mgr(m_Vfs);
    auto entity = m_Registry.CreateEntity();
    ASSERT_TRUE(m_Registry.AddComponent<asge::game::components::AudioSource>(
        entity.Value(), { .m_VirtualClipPath = "images/theme.wav" }).IsOk());
    mgr.ResolveAssets(m_Registry, m_Renderer);
    auto originalClip = m_Registry.GetComponent<asge::game::components::AudioSource>(entity.Value()).Value().get().m_Clip;
    ASSERT_NE(originalClip, nullptr);

    m_Registry.GetComponent<asge::game::components::AudioSource>(entity.Value()).Value().get().m_VirtualClipPath = "images/missing.wav";
    mgr.ResolveAssets(m_Registry, m_Renderer);

    {
        auto audioResult = m_Registry.GetComponent<asge::game::components::AudioSource>(entity.Value());
        auto const& audioSource = audioResult.Value().get();
        EXPECT_EQ(audioSource.m_Clip, originalClip);
        EXPECT_EQ(audioSource.m_ResolvedVirtualClipPath, "images/theme.wav");
    }

    m_Registry.GetComponent<asge::game::components::AudioSource>(entity.Value()).Value().get().m_VirtualClipPath = "images/other-theme.wav";
    mgr.ResolveAssets(m_Registry, m_Renderer);

    auto audioResult = m_Registry.GetComponent<asge::game::components::AudioSource>(entity.Value());
    auto const& audioSource = audioResult.Value().get();
    EXPECT_NE(audioSource.m_Clip, originalClip);
    EXPECT_EQ(audioSource.m_ResolvedVirtualClipPath, "images/other-theme.wav");
}

}
