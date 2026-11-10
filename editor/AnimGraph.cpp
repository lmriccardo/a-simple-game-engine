#include "AnimGraph.hpp"

#include <ASGE/Game/Assets/AnimStateMachine.hpp>
#include <ASGE/Game/Assets/AnimationClipSet.hpp>

#include <ASGE/Core/Formats/TOML/TOML_Builder.hpp>
#include <ASGE/Core/Formats/TOML/TOML_Parser.hpp>

#include <algorithm>
#include <atomic>
#include <format>
#include <sstream>

namespace
{

using asge::formats::xml::XMLNode;

/** @brief The attribute's text, or an empty string if the node has no such attribute. */
std::string AttrOf( XMLNode const& inNode, std::string_view inName )
{
    auto const* attr = inNode.Find( inName );
    return attr ? std::string( attr->Value() ) : std::string{};
}

/** @brief Adds an attribute unless its value is empty, so the engine reports it as missing. */
void SetAttr( XMLNode& ioNode, std::string_view inName, std::string_view inValue )
{
    if ( inValue.empty() ) return;
    if ( auto added = ioNode.AddAttribute( inName, inValue ); !added ) added.LogError();
}

/** @brief Names of every `<inItem name="...">` child of the first child called inList. */
std::vector<std::string> ReadNameList( XMLNode const& inRoot, std::string_view inList, std::string_view inItem )
{
    std::vector<std::string> names;
    for ( auto const& list : inRoot )
    {
        if ( list.Name() != inList ) continue;
        for ( auto const& item : list )
        {
            if ( item.Name() == inItem ) names.push_back( AttrOf( item, "name" ) );
        }
    }
    return names;
}

void WriteNameList( XMLNode& ioRoot, std::string_view inList, std::string_view inItem,
                    std::vector<std::string> const& inNames )
{
    if ( inNames.empty() ) return;

    XMLNode& list = ioRoot.AddChild();
    list.SetName( inList );
    for ( auto const& name : inNames )
    {
        XMLNode& item = list.AddChild();
        item.SetName( inItem );
        SetAttr( item, "name", name );
    }
}

AnimEnd ReadEnd( std::string const& inText ) noexcept
{
    if ( inText == "hold" )    return AnimEnd::Hold;
    if ( inText == "restore" ) return AnimEnd::Restore;
    return AnimEnd::Loop;
}

std::string_view EndName( AnimEnd inEnd ) noexcept
{
    switch ( inEnd )
    {
    case AnimEnd::Loop:    return "loop";
    case AnimEnd::Hold:    return "hold";
    case AnimEnd::Restore: return "restore";
    }
    return "loop";
}

/** @brief Splits a `when` text such as `running !grounded` into set and clear flags. */
void ReadWhen( std::string const& inWhen, AnimTransition& ioTransition )
{
    std::istringstream words( inWhen );
    for ( std::string word; words >> word; )
    {
        if ( word.starts_with( '!' ) ) ioTransition.m_FlagsClear.push_back( word.substr( 1 ) );
        else                           ioTransition.m_FlagsSet.push_back( word );
    }
}

/** @brief Joins flags as `a b !c`: set flags first, then cleared ones. */
std::string JoinWhen( AnimTransition const& inTransition )
{
    std::string out;
    auto append = [&out]( std::string const& inText ) {
        if ( !out.empty() ) out += ' ';
        out += inText;
    };

    for ( auto const& name : inTransition.m_FlagsSet )   append( name );
    for ( auto const& name : inTransition.m_FlagsClear ) append( "!" + name );
    return out;
}

AnimTransition ReadTransition( XMLNode const& inNode )
{
    AnimTransition out;
    out.m_To      = AttrOf( inNode, "to" );
    out.m_Trigger = AttrOf( inNode, "trigger" );
    out.m_AtEnd   = AttrOf( inNode, "atEnd" ) == "true";
    ReadWhen( AttrOf( inNode, "when" ), out );

    if ( auto const* attr = inNode.Find( "priority" ) )
    {
        if ( auto value = attr->AsInt() ) out.m_Priority = value.Value();
    }
    return out;
}

/** @brief A path in the temp directory that no other call in this process uses at the same time. */
std::filesystem::path TempFilePath( std::string_view inExtension )
{
    static std::atomic<unsigned> s_Counter{ 0 };
    return std::filesystem::temp_directory_path()
        / std::format( "asge_anim_check_{}_{}{}", reinterpret_cast<std::uintptr_t>( &s_Counter ), s_Counter++, inExtension );
}

}

asge::Result<AnimMachineDoc> ReadMachine( std::filesystem::path const& inPath )
{
    auto parsed = asge::formats::xml::Parse( inPath );
    if ( !parsed ) return asge::Result<AnimMachineDoc>::Err( parsed.Error() );

    auto const& root = parsed.Value();
    AnimMachineDoc doc;
    doc.m_Initial  = AttrOf( root, "initial" );
    doc.m_Flags    = ReadNameList( root, "Flags", "Flag" );
    doc.m_Triggers = ReadNameList( root, "Triggers", "Trigger" );
    doc.m_Clips    = ReadNameList( root, "Clips", "Clip" );

    for ( auto const& child : root )
    {
        if ( child.Name() != "State" ) continue;

        AnimState state;
        state.m_Name = AttrOf( child, "name" );
        state.m_Clip = AttrOf( child, "clip" );
        state.m_End  = ReadEnd( AttrOf( child, "end" ) );
        for ( auto const& transition : child )
        {
            if ( transition.Name() == "Transition" ) state.m_Transitions.push_back( ReadTransition( transition ) );
        }
        doc.m_States.push_back( std::move( state ) );
    }

    return asge::Result<AnimMachineDoc>::Ok( std::move( doc ) );
}

asge::formats::xml::XMLNode BuildMachineXml( AnimMachineDoc const& inDoc )
{
    XMLNode root( "AnimStateMachine" );
    SetAttr( root, "initial", inDoc.m_Initial );

    WriteNameList( root, "Flags", "Flag", inDoc.m_Flags );
    WriteNameList( root, "Triggers", "Trigger", inDoc.m_Triggers );
    WriteNameList( root, "Clips", "Clip", inDoc.m_Clips );

    for ( auto const& state : inDoc.m_States )
    {
        XMLNode& stateNode = root.AddChild();
        stateNode.SetName( "State" );
        SetAttr( stateNode, "name", state.m_Name );
        SetAttr( stateNode, "clip", state.m_Clip );
        SetAttr( stateNode, "end", EndName( state.m_End ) );

        for ( auto const& transition : state.m_Transitions )
        {
            XMLNode& node = stateNode.AddChild();
            node.SetName( "Transition" );
            SetAttr( node, "to", transition.m_To );
            SetAttr( node, "trigger", transition.m_Trigger );
            SetAttr( node, "when", JoinWhen( transition ) );
            if ( transition.m_AtEnd ) SetAttr( node, "atEnd", "true" );
            if ( transition.m_Priority != 0 ) SetAttr( node, "priority", std::to_string( transition.m_Priority ) );
        }
    }

    return root;
}

asge::BoolResult ValidateMachine( AnimMachineDoc const& inDoc )
{
    auto const tmp = TempFilePath( ".animsm" );
    auto saved = BuildMachineXml( inDoc ).SaveToFile( tmp );
    if ( !saved ) return saved;

    auto loaded = asge::game::asset::AnimationStateMachine::Load( tmp );
    std::error_code ec;
    std::filesystem::remove( tmp, ec );

    if ( !loaded ) return asge::BoolResult::Err( loaded.Error() );
    return asge::BoolResult::Ok();
}

asge::BoolResult WriteMachine( AnimMachineDoc const& inDoc, std::filesystem::path const& inPath )
{
    if ( auto valid = ValidateMachine( inDoc ); !valid ) return valid;
    return BuildMachineXml( inDoc ).SaveToFile( inPath );
}

std::filesystem::path LayoutFilePath( std::filesystem::path const& inMachinePath ) noexcept
{
    return std::filesystem::path( inMachinePath.string() + ".layout" );
}

asge::BoolResult SaveLayout( std::filesystem::path const& inMachinePath, AnimLayout const& inLayout )
{
    auto const path = LayoutFilePath( inMachinePath );

    if ( inLayout.empty() )
    {
        std::error_code ec;
        std::filesystem::remove( path, ec );
        return asge::BoolResult::Ok();
    }

    asge::formats::toml::TOMLBuilder builder;
    for ( auto const& [name, pos] : inLayout )
    {
        auto table = builder.ArrayTable( "State" );
        table.Set( "Name", name );
        table.Set( "X", pos.m_X );
        table.Set( "Y", pos.m_Y );
    }

    return builder.SaveToFile( path );
}

AnimLayout LoadLayout( std::filesystem::path const& inMachinePath )
{
    AnimLayout layout;

    auto const path = LayoutFilePath( inMachinePath );
    std::error_code ec;
    if ( !std::filesystem::exists( path, ec ) ) return layout;

    auto parsed = asge::formats::toml::Parse( path );
    if ( !parsed ) return layout;
    asge::formats::toml::TOMLTableView const doc( parsed.Value() );

    for ( int i = 0;; ++i )
    {
        auto table = doc.GetTable( "State", i );
        if ( !table ) break;

        auto name = table.Value().Get<std::string>( "Name", std::string{} );
        if ( name.empty() ) continue;
        layout[std::move( name )] = AnimNodePos{ table.Value().Get( "X", 0.0f ), table.Value().Get( "Y", 0.0f ) };
    }

    return layout;
}

void AutoLayout( AnimMachineDoc const& inDoc, AnimLayout& ioLayout )
{
    constexpr float kColumnWidth = 260.0f;
    constexpr float kRowHeight   = 140.0f;

    // Forget states that were deleted since the layout was saved
    for ( auto it = ioLayout.begin(); it != ioLayout.end(); )
    {
        bool const exists = std::ranges::any_of( inDoc.m_States,
            [&]( AnimState const& inState ) { return inState.m_Name == it->first; } );
        it = exists ? std::next( it ) : ioLayout.erase( it );
    }

    // Breadth-first distance from the initial state, by state name
    std::map<std::string, int, std::less<>> depth;
    std::vector<std::string> queue;
    if ( !inDoc.m_Initial.empty() ) { depth[inDoc.m_Initial] = 0; queue.push_back( inDoc.m_Initial ); }
    for ( std::size_t head = 0; head < queue.size(); ++head )
    {
        auto const current = queue[head];
        auto const state = std::ranges::find( inDoc.m_States, current, &AnimState::m_Name );
        if ( state == inDoc.m_States.end() ) continue;

        for ( auto const& transition : state->m_Transitions )
        {
            if ( depth.contains( transition.m_To ) ) continue;
            depth[transition.m_To] = depth[current] + 1;
            queue.push_back( transition.m_To );
        }
    }

    int lastColumn = 0;
    for ( auto const& [name, d] : depth ) lastColumn = std::max( lastColumn, d );
    ++lastColumn; // unreachable states

    std::map<int, int> nextRow;
    for ( auto const& state : inDoc.m_States )
    {
        if ( ioLayout.contains( state.m_Name ) ) continue;

        auto const found = depth.find( state.m_Name );
        int const column = found != depth.end() ? found->second : lastColumn;
        ioLayout[state.m_Name] = AnimNodePos{ column * kColumnWidth, static_cast<float>( nextRow[column]++ ) * kRowHeight };
    }
}

asge::Result<AnimClipSetDoc> ReadClipSet( std::filesystem::path const& inPath )
{
    auto parsed = asge::formats::xml::Parse( inPath );
    if ( !parsed ) return asge::Result<AnimClipSetDoc>::Err( parsed.Error() );

    auto const& root = parsed.Value();
    AnimClipSetDoc doc;
    doc.m_Machine = AttrOf( root, "machine" );

    for ( auto const& child : root )
    {
        if ( child.Name() != "Clip" ) continue;

        AnimClipBinding clip;
        clip.m_Name = AttrOf( child, "name" );
        clip.m_Path = AttrOf( child, "path" );
        if ( auto const* attr = child.Find( "frameDuration" ) )
        {
            if ( auto value = attr->AsFloat() ) clip.m_FrameDuration = value.Value();
        }
        doc.m_Clips.push_back( std::move( clip ) );
    }

    return asge::Result<AnimClipSetDoc>::Ok( std::move( doc ) );
}

asge::formats::xml::XMLNode BuildClipSetXml( AnimClipSetDoc const& inDoc )
{
    XMLNode root( "AnimClipSet" );
    SetAttr( root, "machine", inDoc.m_Machine );

    for ( auto const& clip : inDoc.m_Clips )
    {
        XMLNode& node = root.AddChild();
        node.SetName( "Clip" );
        SetAttr( node, "name", clip.m_Name );
        SetAttr( node, "path", clip.m_Path );
        SetAttr( node, "frameDuration", std::format( "{}", clip.m_FrameDuration ) );
    }

    return root;
}

asge::BoolResult ValidateClipSet( AnimClipSetDoc const& inDoc )
{
    auto const tmp = TempFilePath( ".animset" );
    auto saved = BuildClipSetXml( inDoc ).SaveToFile( tmp );
    if ( !saved ) return saved;

    auto loaded = asge::game::asset::AnimationClipSet::Load( tmp );
    std::error_code ec;
    std::filesystem::remove( tmp, ec );

    if ( !loaded ) return asge::BoolResult::Err( loaded.Error() );
    return asge::BoolResult::Ok();
}

asge::BoolResult WriteClipSet( AnimClipSetDoc const& inDoc, std::filesystem::path const& inPath )
{
    if ( auto valid = ValidateClipSet( inDoc ); !valid ) return valid;
    return BuildClipSetXml( inDoc ).SaveToFile( inPath );
}
