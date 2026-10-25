#pragma once

#include <vector>
#include <cstddef>
#include <ASGE/Core/Math/Geometry/Rect.hpp>
#include <ASGE/Core/Errors.hpp>
#include <ASGE/Core/Filesystem/FileData.hpp>
#include <ASGE/Core/Strings.hpp>

namespace asge::game::asset
{

/** @brief Direction in which frame indices walk a spritesheet grid. */
enum class GridOrder { RowMajor, ColumnMajor };

/**
 * @brief Describes one clip as a contiguous range of cells over a regular
 *        spritesheet grid — which cells, in which order, and how they are
 *        spaced. Serialized field-for-field by FrameTable::Save/Load.
 */
struct GridSpec
{
    math::Rect  m_Cell{};        // First cell: x/y = sheet origin (margin), w/h = one cell, in texture pixels
    std::size_t m_Columns{0};    // Sheet width in cells -- the wrap length in RowMajor order
    std::size_t m_Rows{0};       // Sheet height in cells -- the wrap length in ColumnMajor order; 0 = unknown
    std::size_t m_First{0};      // Index of the clip's first frame, counted in m_Order
    std::size_t m_Count{0};      // Number of frames in the clip
    float       m_Spacing{0.0f}; // Gap between adjacent cells, in pixels
    GridOrder   m_Order{GridOrder::RowMajor};
};

/**
 * @brief A named sequence of source rects into a spritesheet — one entry
 *        per animation frame, shared by every components::Animation whose
 *        m_ClipPath resolves to the same virtual path (see
 *        asset::AssetManager::GetFrameTable/ResolveAssets).
 *
 * Load() reads a small TOML meta-file (its own `[FrameTable]` table, keyed
 * by kTableName) describing a contiguous run of cells on one regular grid
 * (see GridSpec): "x"/"y"/"w"/"h" for the first cell, "columns"/"rows" for
 * the grid size, "first"/"count" for which cells (indexed in "order", either
 * "row" (default) or "column") are frames, and "spacing" for the pixel gap
 * between cells. Every key but x/y/w/h/columns/count is optional, so older
 * files still load; "rows" = 0 means unknown. Load() fails on an unknown
 * "order" or on a clip that runs past the end of a known-size grid. See
 * MakeGridFrames, which does the actual grid math. Not the spritesheet's pixels themselves; the entity's
 * own Sprite::m_VirtualPath still points at the texture this FrameTable's
 * rects are meant to crop -- m_OriginAsset (also read from the same table,
 * key "m_OriginAsset") is that same texture's virtual path, recorded purely
 * as editor bookkeeping (see the level editor's "Create Clip" modal and its
 * Asset Panel import gate) so a clip's own file can point back at the
 * spritesheet it was sliced from. Load() never rejects a file missing it —
 * defaults to an empty string, same as any other unset field — it's the
 * editor's explicit "Load Asset..." import that treats an empty
 * m_OriginAsset as reason to reject a file as not one of its own clips.
 *
 * An optional "frame_durations" float array gives each frame its own display
 * time in seconds (m_FrameDurations); see FrameDuration for the fallback.
 */
struct FrameTable
{
    static constexpr str::StringView kTableName = "FrameTable";
    std::vector<math::Rect> m_Frames;
    std::vector<float>      m_FrameDurations{}; // Optional per-frame seconds (key "frame_durations");
    str::String             m_OriginAsset{}; // Virtual path of the texture this clip slices

    /** @brief Seconds frame inIndex is shown: its m_FrameDurations entry if present and positive, else inFallback. */
    [[nodiscard]] float FrameDuration( std::size_t inIndex, float inFallback ) const noexcept
    {
        return inIndex < m_FrameDurations.size() && m_FrameDurations[inIndex] > 0.0f
            ? m_FrameDurations[inIndex] : inFallback;
    }

    /** @brief Parses inPath's `[FrameTable]` TOML table into m_Frames/m_OriginAsset */
    static Result<FrameTable> Load( filesystem::Path const& inPath );

    /**
     * @brief Writes inSpec/inOriginAsset as inPath's
     *        `[FrameTable]` table — the exact schema Load() reads back (see
     *        the struct doc comment). Creates inPath if it doesn't exist;
     *        overwrites it (as a fresh file containing just this table) if
     *        it does — meant for a file this call is the sole owner of (the
     *        level editor's in-editor spritesheet slicer, say), not one
     *        with other hand-authored content to preserve.
     */
    [[nodiscard]] static BoolResult Save(filesystem::Path const& inPath, 
                                         GridSpec const& inSpec, 
                                         str::StringView inOriginAsset,
                                         std::vector<float> const& inFrameDurations = {} ) noexcept;

    /**
     * @brief Checks whether inPath is a FrameTable clip — a TOML file with
     *        a top-level `[FrameTable]` table — without building the frame
     *        grid. Parses the real TOML, unlike a substring search.
     * @return false for a missing, malformed, or non-matching file alike.
     */
    [[nodiscard]] static bool IsFrameTable( filesystem::Path const& inPath );
};

/**
 * @brief Builds inSpec.m_Count sub-rects (in texture pixels) across a regular
 *        grid spritesheet, for use as FrameTable::m_Frames.
 *
 * Cell indices run from m_First to m_First + m_Count - 1. In RowMajor order
 * index i sits at column (i % m_Columns), row (i / m_Columns); in ColumnMajor
 * order at column (i / m_Rows), row (i % m_Rows). Adjacent cells are
 * m_Spacing pixels apart. Returns an empty vector if m_Count is 0 or the
 * wrap length for the order (m_Columns / m_Rows) is 0.
 */
std::vector<math::Rect> MakeGridFrames( GridSpec const& inSpec ) noexcept;
    
} // namespace asge::game::asset
