#pragma once

#include <vector>
#include <cstddef>
#include <ASGE/Core/Math/Geometry/Rect.hpp>
#include <ASGE/Core/Errors.hpp>
#include <ASGE/Core/Filesystem/FileData.hpp>
#include <ASGE/Core/Strings.hpp>

namespace asge::game::asset
{

/**
 * @brief A named sequence of source rects into a spritesheet — one entry
 *        per animation frame, shared by every components::Animation whose
 *        m_ClipPath resolves to the same virtual path (see
 *        asset::AssetManager::GetFrameTable/ResolveAssets).
 *
 * Load() reads a small TOML meta-file (its own `[FrameTable]` table, keyed
 * by kTableName) describing one regular grid: "x"/"y"/"w"/"h" for the first
 * cell, "columns" for the grid width, and "count" for how many cells (in
 * row-major order) are actually frames — see MakeGridFrames, which does the
 * actual grid math. Not the spritesheet's pixels themselves; the entity's
 * own Sprite::m_VirtualPath still points at the texture this FrameTable's
 * rects are meant to crop.
 */
struct FrameTable
{
    static constexpr str::StringView kTableName = "FrameTable";
    std::vector<math::Rect> m_Frames;

    /** @brief Parses inPath's `[FrameTable]` TOML table into m_Frames — see the struct doc comment for its schema. */
    static Result<FrameTable> Load( filesystem::Path const& inPath );
};

/**
 * @brief Builds inCount evenly-spaced sub-rects (in texture pixels) across a
 *        regular grid spritesheet, for use as FrameTable::m_Frames.
 *
 * inSheetCell's x/y is the top-left of frame 0 and its w/h is one cell's
 * size; frame i sits at column (i % inColumns), row (i / inColumns) of that
 * grid, so inCount need not fill the grid exactly (e.g. 6 frames out of an
 * 8-cell sheet). Returns an empty vector if inColumns or inCount is 0.
 */
std::vector<math::Rect> MakeGridFrames(
    math::Rect inSheetCell, std::size_t inColumns, std::size_t inCount ) noexcept;
    
} // namespace asge::game::asset
