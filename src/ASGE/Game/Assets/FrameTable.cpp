#include "FrameTable.hpp"

#include <algorithm>
#include <ASGE/Core/Configuration/TOML_Builder.hpp>
#include <ASGE/Core/Configuration/TOML_Parser.hpp>
#include <ASGE/Core/Configuration/TOML_TableView.hpp>
#include <ASGE/Core/Strings.hpp>

namespace
{

using asge::game::asset::GridOrder;

constexpr asge::str::StringView kOrderRow    = "row";
constexpr asge::str::StringView kOrderColumn = "column";

constexpr asge::str::StringView OrderToString( GridOrder inOrder ) noexcept
{
    return inOrder == GridOrder::ColumnMajor ? kOrderColumn : kOrderRow;
}

}

asge::Result<asge::game::asset::FrameTable> asge::game::asset::FrameTable::Load(
    asge::filesystem::Path const& inPath )
{
    auto parsed = asge::config::toml::Parse( inPath );
    if ( !parsed ) return Result<FrameTable>::Err( parsed.Error() );

    asge::config::toml::TOMLTableView root( parsed.Value() );
    auto table = root.Table( str::String(kTableName) );

    // A negative value would wrap to a huge size_t, so clamp to 0 first.
    auto const getSize = [&table]( char const* inKey ) {
        return static_cast<std::size_t>( std::max( 0, table.Get<int>( inKey, 0 ) ) );
    };

    GridSpec spec{
        .m_Cell    = { table.Get( "x", 0.0f ), table.Get( "y", 0.0f ),
                       table.Get( "w", 0.0f ), table.Get( "h", 0.0f ) },
        .m_Columns = getSize( "columns" ),
        .m_Rows    = getSize( "rows" ),
        .m_First   = getSize( "first" ),
        .m_Count   = getSize( "count" ),
        .m_Spacing = table.Get( "spacing", 0.0f ),
    };

    auto const order = table.Get( "order", str::String( kOrderRow ) );
    if ( order != kOrderRow && order != kOrderColumn )
    {
        return Result<FrameTable>::Err(
            make_error_code( errors::ConfError::TomlBadFormatting ),
            "unknown grid order (expected \"row\" or \"column\")" );
    }
    spec.m_Order = ( order == kOrderColumn ) ? GridOrder::ColumnMajor : GridOrder::RowMajor;

    // m_Rows == 0 means "unknown" (every clip written before this field existed), so it can't be checked.
    if ( spec.m_Rows != 0 && spec.m_Columns != 0
         && spec.m_First + spec.m_Count > spec.m_Rows * spec.m_Columns )
    {
        return Result<FrameTable>::Err(
            make_error_code( errors::ConfError::TomlBadFormatting ),
            "clip runs past the end of the sheet grid" );
    }

    FrameTable result{ MakeGridFrames( spec ) };
    result.m_OriginAsset = table.Get( "m_OriginAsset", str::String{} );
    return Result<FrameTable>::Ok( std::move( result ) );
}

asge::BoolResult asge::game::asset::FrameTable::Save(
    filesystem::Path const& inPath, GridSpec const& inSpec, str::StringView inOriginAsset ) noexcept
{
    asge::config::toml::TOMLBuilder builder;
    auto table = builder.Table( str::String( kTableName ) );
    table.Set( "x", inSpec.m_Cell.m_X );
    table.Set( "y", inSpec.m_Cell.m_Y );
    table.Set( "w", inSpec.m_Cell.m_Width );
    table.Set( "h", inSpec.m_Cell.m_Height );
    table.Set( "columns", static_cast<int>( inSpec.m_Columns ) );
    table.Set( "rows", static_cast<int>( inSpec.m_Rows ) );
    table.Set( "first", static_cast<int>( inSpec.m_First ) );
    table.Set( "count", static_cast<int>( inSpec.m_Count ) );
    table.Set( "spacing", inSpec.m_Spacing );
    table.Set( "order", std::string( OrderToString( inSpec.m_Order ) ) );
    table.Set( "m_OriginAsset", std::string( inOriginAsset ) );
    return builder.SaveToFile( inPath );
}

bool asge::game::asset::FrameTable::IsFrameTable(filesystem::Path const &inPath)
{
    auto parsed = asge::config::toml::Parse( inPath );
    if ( !parsed ) return false;

    asge::config::toml::TOMLTableView const root( parsed.Value() );
    return root.HasTable( str::String(kTableName) );
}

std::vector<asge::math::Rect> asge::game::asset::MakeGridFrames(GridSpec const &inSpec) noexcept
{
    std::size_t wrap;
    switch ( inSpec.m_Order )
    {
    case GridOrder::RowMajor: wrap = inSpec.m_Columns; break;
    case GridOrder::ColumnMajor: wrap = inSpec.m_Rows; break;
    }

    std::vector<math::Rect> frames;
    if ( inSpec.m_Count == 0 || wrap == 0 ) return frames;

    for ( std::size_t ii = inSpec.m_First; ii < inSpec.m_First + inSpec.m_Count; ++ii )
    {
        std::size_t const col = ( inSpec.m_Order == GridOrder::RowMajor ) ? ii % wrap : ii / wrap;
        std::size_t const row = ( inSpec.m_Order == GridOrder::RowMajor ) ? ii / wrap : ii % wrap;
        frames.push_back( math::Rect{
            inSpec.m_Cell.m_X + static_cast<float>(col) * ( inSpec.m_Cell.m_Width  + inSpec.m_Spacing ),
            inSpec.m_Cell.m_Y + static_cast<float>(row) * ( inSpec.m_Cell.m_Height + inSpec.m_Spacing ),
            inSpec.m_Cell.m_Width, inSpec.m_Cell.m_Height
        } );
    }

    return frames;
}
