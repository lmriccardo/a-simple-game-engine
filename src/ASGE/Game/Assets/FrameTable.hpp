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
 * rects are meant to crop -- m_OriginAsset (also read from the same table,
 * key "m_OriginAsset") is that same texture's virtual path, recorded purely
 * as editor bookkeeping (see the level editor's "Create Clip" modal and its
 * Asset Panel import gate) so a clip's own file can point back at the
 * spritesheet it was sliced from. Load() never rejects a file missing it —
 * defaults to an empty string, same as any other unset field — it's the
 * editor's explicit "Load Asset..." import that treats an empty
 * m_OriginAsset as reason to reject a file as not one of its own clips.
 */
struct FrameTable
{
    static constexpr str::StringView kTableName = "FrameTable";
    std::vector<math::Rect> m_Frames;
    str::String             m_OriginAsset{}; // Virtual path of the texture this clip slices -- editor bookkeeping only

    /** @brief Parses inPath's `[FrameTable]` TOML table into m_Frames/m_OriginAsset — see the struct doc comment for its schema. */
    static Result<FrameTable> Load( filesystem::Path const& inPath );

    /**
     * @brief Writes inCell/inColumns/inCount/inOriginAsset as inPath's
     *        `[FrameTable]` table — the exact schema Load() reads back (see
     *        the struct doc comment). Creates inPath if it doesn't exist;
     *        overwrites it (as a fresh file containing just this table) if
     *        it does — meant for a file this call is the sole owner of (the
     *        level editor's in-editor spritesheet slicer, say), not one
     *        with other hand-authored content to preserve.
     */
    [[nodiscard]] static BoolResult Save(
        filesystem::Path const& inPath, math::Rect const& inCell, std::size_t inColumns, std::size_t inCount,
        str::StringView inOriginAsset ) noexcept;

    /**
     * @brief Checks whether inPath is a FrameTable clip — a TOML file with
     *        a top-level `[FrameTable]` table — without building the frame
     *        grid. Parses the real TOML, unlike a substring search.
     * @return false for a missing, malformed, or non-matching file alike.
     */
    [[nodiscard]] static bool IsFrameTable( filesystem::Path const& inPath );
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
