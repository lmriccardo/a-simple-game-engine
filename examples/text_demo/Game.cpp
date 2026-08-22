#include "Game.hpp"

#include <cmath>
#include <filesystem>

namespace
{
constexpr float HUE_CYCLE_SPEED = 0.6f; // full cycles per ~10 seconds

// Cheap HSV(hue in turns, full sat/value) -> RGB, just enough to cycle the
// headline's color smoothly without needing a real color-space library.
asge::graphics::RGBA_Color HueToColor(float inHueTurns)
{
    float const h = (inHueTurns - std::floor(inHueTurns)) * 6.0f;
    float const x = 1.0f - std::fabs(std::fmod(h, 2.0f) - 1.0f);

    float r = 0.0f, g = 0.0f, b = 0.0f;
    if      (h < 1.0f) { r = 1.0f; g = x;    b = 0.0f; }
    else if (h < 2.0f) { r = x;    g = 1.0f; b = 0.0f; }
    else if (h < 3.0f) { r = 0.0f; g = 1.0f; b = x;    }
    else if (h < 4.0f) { r = 0.0f; g = x;    b = 1.0f; }
    else if (h < 5.0f) { r = x;    g = 0.0f; b = 1.0f; }
    else                { r = 1.0f; g = 0.0f; b = x;    }

    return {
        static_cast<std::uint8_t>(r * 255.0f),
        static_cast<std::uint8_t>(g * 255.0f),
        static_cast<std::uint8_t>(b * 255.0f),
        255
    };
}

std::unique_ptr<asge::graphics::Font> LoadFont(int inPixelHeight)
{
    // ASGE_TEXT_DEMO_ASSET_DIR is injected by CMakeLists.txt.
    auto const path = std::filesystem::path(ASGE_TEXT_DEMO_ASSET_DIR) / "PTSans-Regular.ttf";

    auto fontResult = asge::graphics::Font::Load(path, inPixelHeight);
    if (!fontResult)
    {
        fontResult.LogError();
        return nullptr;
    }

    return std::make_unique<asge::graphics::Font>(std::move(fontResult).Value());
}
}

void TextDemoGame::EnsureFontsLoaded(asge::video::IRenderer& inRenderer)
{
    if (!m_SmallFont)
    {
        m_SmallFont = LoadFont(18);
        if (m_SmallFont) m_SmallAtlas = inRenderer.CreateTexture(m_SmallFont->GetAtlasImage());
    }

    if (!m_MediumFont)
    {
        m_MediumFont = LoadFont(28);
        if (m_MediumFont) m_MediumAtlas = inRenderer.CreateTexture(m_MediumFont->GetAtlasImage());
    }

    if (!m_LargeFont)
    {
        m_LargeFont = LoadFont(56);
        if (m_LargeFont) m_LargeAtlas = inRenderer.CreateTexture(m_LargeFont->GetAtlasImage());
    }
}

void TextDemoGame::Update(float inDeltaTime, [[maybe_unused]] asge::input::InputState const& inInput)
{
    m_HueTime += inDeltaTime * (HUE_CYCLE_SPEED / 10.0f);
}

void TextDemoGame::Render(asge::video::IRenderer &inRenderer)
{
    inRenderer.Clear({ 20, 20, 25, 255 });

    EnsureFontsLoaded(inRenderer);

    if (m_SmallFont && m_SmallAtlas)
    {
        inRenderer.DrawString(
            "The quick brown fox jumps over the lazy dog.",
            *m_SmallFont, *m_SmallAtlas, { 40.0f, 70.0f }, { 220, 220, 220, 255 });

        inRenderer.DrawString(
            "Font::GetAtlasImage() ->",
            *m_SmallFont, *m_SmallAtlas, { 460.0f, 400.0f }, { 150, 150, 150, 255 });
    }

    if (m_MediumFont && m_MediumAtlas)
    {
        inRenderer.DrawString(
            "DrawString renders glyphs from a baked atlas",
            *m_MediumFont, *m_MediumAtlas, { 40.0f, 150.0f }, { 120, 200, 255, 255 });
    }

    if (m_LargeFont && m_LargeAtlas)
    {
        // SetColorMod is applied per DrawString call, not baked into the
        // texture -- this line's color keeps changing every frame while
        // reusing the exact same atlas texture as every other draw below.
        inRenderer.DrawString(
            "ASGE", *m_LargeFont, *m_LargeAtlas, { 40.0f, 280.0f }, HueToColor(m_HueTime));

        // The atlas itself, drawn like any other texture -- every glyph
        // DrawString just rendered lives somewhere in this packed bitmap.
        inRenderer.DrawTexture(*m_LargeAtlas, asge::math::Rect{ 460.0f, 410.0f, 150.0f, 150.0f });
    }
}

void TextDemoGame::OnSystemEvent([[maybe_unused]] asge::event::SystemEvent const &inSysEvent)
{
}
