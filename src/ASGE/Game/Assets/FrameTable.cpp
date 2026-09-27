#include "FrameTable.hpp"

#include <ASGE/Core/Configuration/TOML_Builder.hpp>
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

    FrameTable result{ MakeGridFrames( cell, columns, count ) };
    result.m_OriginAsset = table.Get( "m_OriginAsset", str::String{} );
    return Result<FrameTable>::Ok( std::move( result ) );
}

asge::BoolResult asge::game::asset::FrameTable::Save(
    filesystem::Path const& inPath, math::Rect const& inCell, std::size_t inColumns, std::size_t inCount,
    str::StringView inOriginAsset ) noexcept
{
    asge::config::toml::TOMLBuilder builder;
    auto table = builder.Table( str::String( kTableName ) );
    table.Set( "x", inCell.m_X );
    table.Set( "y", inCell.m_Y );
    table.Set( "w", inCell.m_Width );
    table.Set( "h", inCell.m_Height );
    table.Set( "columns", static_cast<int>( inColumns ) );
    table.Set( "count", static_cast<int>( inCount ) );
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
            inSheetCell.m_X + static_cast<float>(col) * inSheetCell.m_Width,
            inSheetCell.m_Y + static_cast<float>(row) * inSheetCell.m_Height,
            inSheetCell.m_Width,
            inSheetCell.m_Height
        });
    }

    return frames;
}