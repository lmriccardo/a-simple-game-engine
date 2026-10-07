/**
 * @file Font.hpp
 * @brief Font loading and glyph atlas baking.
 *
 * An atlas is a single image that packs many glyph bitmaps into one
 * texture, each glyph occupying its own sub-rectangle, rather than
 * allocating a separate texture per glyph. This keeps text rendering
 * to a single texture bind per draw call instead of one per character.
 *
 * Wraps stb_truetype to load a TTF file and bake a fixed glyph range
 * into such an atlas at load time. GlyphMetrics describes where each
 * glyph's bitmap lives within the atlas and how to position it
 * relative to the pen; Font exposes lookup of those metrics plus the
 * baked atlas IImage, which callers upload via the existing
 * IImage -> ITexture path. Text layout and drawing are built on top
 * of this, not part of it.
 */

#pragma once

#include <vector>
#include <unordered_map>
#include <cstdint>
#include <ASGE/Core/Filesystem/FileIO.hpp>
#include <ASGE/Core/Errors.hpp>
#include <ASGE/Core/Math/Geometry/Rect.hpp>
#include <ASGE/Core/Math/LinearAlgebra/Vector2.hpp>
#include <ASGE/Core/Strings.hpp>
#include "Image.hpp"

namespace asge::media
{

/**
 * @brief Metrics and atlas location for a single baked glyph.
 *
 * Produced when a font is baked into an atlas (see Font::Load).
 * uv_rect locates the glyph's bitmap within the shared atlas image;
 * the remaining fields describe how to position it relative to the
 * pen when laying out text.
 */
struct GlyphMetrics
{
    math::Rect uv_rect;      // atlas UV coords
    math::Int2 size;         // glyph bitmap size in pixels
    math::Int2 bearing;      // offset from baseline/pen position
    int advance;             // pen advance in pixels (or 26.6 fixed-point)
};

class Font
{
private:
    // Per-codepoint atlas location and layout data for baked glyphs.
    std::unordered_map<char32_t, GlyphMetrics> m_Glyphs;

    // Baked bitmap packing every glyph's bitmap into sub-rectangles.
    Image m_AtlasImage;

    // Line height, ascent, descent — font-wide vertical metrics, in pixels.
    int m_LineHeight;
    int m_Ascent;
    int m_Descent;

    Font(std::unordered_map<char32_t, GlyphMetrics> inGlyphs,
         Image inAtlasImage, int inLineHeight,
         int inAscent, int inDescent) noexcept;

public:
    Font() = delete;

    Font( Font const& ) = delete;
    Font& operator=( Font const& ) = delete;
    Font( Font&& ) = default;
    Font& operator=( Font&& ) = default;

    /**
     * @brief Loads a TTF file and bakes ASCII 32-126 into a single atlas.
     * @param inPath Path to the .ttf file.
     * @param inPixelHeight Pixel height to bake glyphs at.
     */
    [[nodiscard]] static Result<Font> Load(const filesystem::Path& inPath, int inPixelHeight);

    /**
     * @brief The fixed pixel dimensions of the atlas bitmap every Font bakes
     *        its glyphs into.
     *
     * Same for every Font regardless of source file or requested pixel
     * height -- Load's ASCII 32-126 bake fails (FontError::BakeFailed) once
     * a requested height's glyphs no longer all fit within it. There's no
     * formula from this alone to a given font's own actual max safe pixel
     * height, since that also depends on that font's own glyph shapes; this
     * just gives a caller (e.g. an editor's Font Size field) something
     * concrete to reason about instead of an arbitrary guess.
     */
    [[nodiscard]] static math::Int2 GetAtlasSize() noexcept;

    /**
     * @brief The largest pixel height inPath's own font can be baked at
     *        without its glyphs overflowing GetAtlasSize()'s fixed bitmap.
     *
     * Found by binary search using Load itself as the oracle at each
     * candidate height -- there's no direct formula for this (see
     * GetAtlasSize's own doc comment for why), but Load's own success/
     * failure at a given height is exact, so the result here is too, not a
     * heuristic. Fails with whatever error Load itself hits at 1px (e.g. the
     * file isn't a valid font at all) rather than a "too big" one.
     * @param inUpperBound Highest pixel height the search considers;
     *        defaults to the atlas's own height, since no font can
     *        plausibly still fit beyond that.
     */
    [[nodiscard]] static Result<int> FindMaxPixelHeight(
        const filesystem::Path& inPath, int inUpperBound = 512 );

    [[nodiscard]] Result<GlyphMetrics> GetGlyph(char32_t inCodepoint) const;
    [[nodiscard]] const media::Image& GetAtlasImage() const noexcept;

    [[nodiscard]] int GetLineHeight() const noexcept;
    [[nodiscard]] int GetAscent() const noexcept;
    [[nodiscard]] int GetDescent() const noexcept;
    [[nodiscard]] math::Float2 Measure( str::StringView inText ) const noexcept;

    /**
     * @brief Greedy word-wraps inText to inMaxWidth pixels by inserting a newline at spaces.
     * Existing newlines are kept; a word wider than inMaxWidth gets its own line, not split.
     */
    [[nodiscard]] str::String WrapText( str::StringView inText, float inMaxWidth ) const noexcept;
};

}