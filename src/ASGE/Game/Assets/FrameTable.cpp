#include "FrameTable.hpp"

#include <ASGE/Core/Configuration/TOML_Parser.hpp>
#include <ASGE/Core/Configuration/TOML_TableView.hpp>
#include <ASGE/Core/Strings.hpp>

asge::Result<asge::game::asset::FrameTable> asge::game::asset::FrameTable::Load( 
    asge::filesystem::Path const& inPath )
{
    auto parsed = asge::config::toml::Parse( inPath );
    if ( !parsed ) return Result<FrameTable>::Err( parsed.Error() );

    asge::config::toml::TOMLTableView root( parsed.Value() );
    auto table = root.Table( str::String(kTableName) );

    math::Rect const cell{
        table.Get( "x", 0.0f ), table.Get( "y", 0.0f ),
        table.Get( "w", 0.0f ), table.Get( "h", 0.0f )
    };

    auto const columns = static_cast<std::size_t>( table.Get<int>( "columns", 0 ) );
    auto const count    = static_cast<std::size_t>( table.Get<int>( "count", 0 ) );

    return Result<FrameTable>::Ok( 
        FrameTable{ MakeGridFrames( cell, columns, count ) } );
}

std::vector<asge::math::Rect> asge::game::asset::MakeGridFrames(
    math::Rect inSheetCell, std::size_t inColumns, std::size_t inCount) noexcept
{
    std::vector<math::Rect> frames{};
    if ( inCount == 0 || inColumns == 0 ) return frames;

    frames.reserve( inCount );
    for ( std::size_t ii = 0; ii < inCount; ++ii )
    {
        std::size_t const col = ii % inColumns;
        std::size_t const row = ii / inColumns;

        frames.push_back( math::Rect{
            inSheetCell.x + static_cast<float>(col) * inSheetCell.w,
            inSheetCell.y + static_cast<float>(row) * inSheetCell.h,
            inSheetCell.w,
            inSheetCell.h
        });
    }

    return frames;
}