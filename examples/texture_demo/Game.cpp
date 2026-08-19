#include "Game.hpp"

#include <cmath>
#include <filesystem>

namespace
{
constexpr float ROTATION_SPEED = 1.2f; // radians per second, for the DrawTextureAffine demo
constexpr float AFFINE_SIZE = 120.0f;
const asge::math::Float2 AFFINE_CENTER{ 620.0f, 300.0f };

asge::math::Float2 Rotate(asge::math::Float2 const& inVec, float inAngle)
{
    float const c = std::cos(inAngle);
    float const s = std::sin(inAngle);
    return { inVec.x() * c - inVec.y() * s, inVec.x() * s + inVec.y() * c };
}

std::unique_ptr<asge::video::ITexture> LoadTexture(
    asge::video::IRenderer& inRenderer, char const* inFileName)
{
    // ASGE_TEXTURE_DEMO_ASSET_DIR is injected by CMakeLists.txt.
    auto const path = std::filesystem::path(ASGE_TEXTURE_DEMO_ASSET_DIR) / inFileName;

    auto imageResult = asge::graphics::Image::Load(path);
    if (!imageResult)
    {
        imageResult.LogError();
        return nullptr;
    }

    return inRenderer.CreateTexture(imageResult.Value());
}
}

void TextureDemoGame::EnsureTexturesLoaded(asge::video::IRenderer& inRenderer)
{
    if (!m_CheckerTexture) m_CheckerTexture = LoadTexture(inRenderer, "checker.bmp");
    if (!m_FrameTexture)   m_FrameTexture   = LoadTexture(inRenderer, "frame.bmp");
}

void TextureDemoGame::Update(float inDeltaTime)
{
    m_RotationAngle += ROTATION_SPEED * inDeltaTime;
}

void TextureDemoGame::Render(asge::video::IRenderer &inRenderer)
{
    inRenderer.Clear({ 20, 20, 20, 255 });

    EnsureTexturesLoaded(inRenderer);

    if (m_CheckerTexture)
    {
        // DrawTexture(Rect): scaled into a destination rectangle
        inRenderer.DrawTexture(*m_CheckerTexture, asge::math::Rect{ 20.0f, 20.0f, 160.0f, 160.0f });

        // DrawTexture(Position): drawn at native size, no scaling
        inRenderer.DrawTexture(*m_CheckerTexture, asge::math::Float2{ 620.0f, 20.0f });

        // DrawTextureTiled: repeated like wallpaper to fill the destination rect
        inRenderer.DrawTextureTiled(
            *m_CheckerTexture, 1.0f, asge::math::Rect{ 20.0f, 220.0f, 360.0f, 160.0f });

        // DrawTextureAffine: rotates the texture around AFFINE_CENTER
        float const half = AFFINE_SIZE / 2.0f;
        asge::math::Float2 origin = AFFINE_CENTER + Rotate({ -half, -half }, m_RotationAngle);
        asge::math::Float2 right  = AFFINE_CENTER + Rotate({  half, -half }, m_RotationAngle);
        asge::math::Float2 down   = AFFINE_CENTER + Rotate({ -half,  half }, m_RotationAngle);
        inRenderer.DrawTextureAffine(*m_CheckerTexture, origin, right, down);
    }

    if (m_FrameTexture)
    {
        // DrawTexture9Grid: corners stay crisp, edges stretch, center fills --
        // frame.bmp's 4px border on all sides matches these margins.
        inRenderer.DrawTexture9Grid(
            *m_FrameTexture, 4.0f, 4.0f, 4.0f, 4.0f,
            asge::math::Rect{ 220.0f, 20.0f, 360.0f, 160.0f });
    }
}

void TextureDemoGame::OnSystemEvent([[maybe_unused]] asge::event::SystemEvent const &inSysEvent)
{
}
