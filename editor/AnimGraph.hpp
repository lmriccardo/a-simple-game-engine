#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include <ASGE/Core/Errors.hpp>
#include <ASGE/Core/Formats/XML/XML.hpp>

/** @brief What a state does at the last frame of its clip; mirrors the `end` attribute. */
enum class AnimEnd { Loop, Hold, Restore };

/** @brief One transition of a state, as drawn on an edge of the graph. */
struct AnimTransition
{
    std::string              m_To;                // Target state name
    std::string              m_Trigger;           // Trigger name; empty = none
    std::vector<std::string> m_FlagsSet;          // Flags that must be set
    std::vector<std::string> m_FlagsClear;        // Flags that must be clear
    bool                     m_AtEnd    = false;  // Only once the state finished
    int                      m_Priority = 0;      // Higher fires first
};

/** @brief One state, as drawn on a node of the graph. */
struct AnimState
{
    std::string                 m_Name;                    // State name
    std::string                 m_Clip;                    // Clip name
    AnimEnd                     m_End = AnimEnd::Loop;     // What happens at the last frame
    std::vector<AnimTransition> m_Transitions;             // Outgoing edges, in document order
};

/**
 * @brief The editor's own copy of a state machine file.
 *
 * Plain data the graph edits freely, by name. It is not an engine object and
 * has no rules of its own: the engine's loader is the only judge of what is
 * valid (see ValidateMachine).
 */
struct AnimMachineDoc
{
    std::string              m_Initial;   // Name of the initial state
    std::vector<std::string> m_Flags;     // Flag names
    std::vector<std::string> m_Triggers;  // Trigger names
    std::vector<std::string> m_Clips;     // Clip names
    std::vector<AnimState>   m_States;    // States
};

/** @brief One clip of a clip set file. */
struct AnimClipBinding
{
    std::string m_Name;                  // Clip name
    std::string m_Path;                  // FrameTable virtual path
    float       m_FrameDuration = 0.1f;  // Seconds per frame
};

/** @brief The editor's own copy of a clip set file; see AnimMachineDoc. */
struct AnimClipSetDoc
{
    std::string                  m_Machine;  // Virtual path of the machine file
    std::vector<AnimClipBinding> m_Clips;    // Clip bindings
};

/**
 * @brief Reads a state machine XML file into the editor's model.
 *
 * Only reads the XML: it does not judge the content, so a file the engine
 * would reject still opens for fixing, with whatever is missing left empty.
 * Elements the schema does not know are skipped. Fails only if the file
 * cannot be read or parsed.
 */
[[nodiscard]] asge::Result<AnimMachineDoc> ReadMachine( std::filesystem::path const& inPath );

/** @brief Builds the XML tree of a machine; empty names are left out so the engine reports them as missing. */
[[nodiscard]] asge::formats::xml::XMLNode BuildMachineXml( AnimMachineDoc const& inDoc );

/**
 * @brief Checks a machine with the engine's own loader.
 *
 * Writes it to a temporary file, loads that with AnimationStateMachine::Load
 * and returns its error, with the element and attribute in the detail. The
 * temporary file is always removed.
 */
[[nodiscard]] asge::BoolResult ValidateMachine( AnimMachineDoc const& inDoc );

/**
 * @brief Writes a machine to inPath if the engine accepts it.
 *
 * ValidateMachine runs first; on failure its error is returned and the file
 * is not touched, so whatever is on disk always loads.
 */
[[nodiscard]] asge::BoolResult WriteMachine( AnimMachineDoc const& inDoc, std::filesystem::path const& inPath );

/** @brief Where a state box sits on the graph canvas. */
struct AnimNodePos
{
    float m_X = 0.0f;  // Canvas x
    float m_Y = 0.0f;  // Canvas y
};

/** @brief Canvas position of each state box, by state name. */
using AnimLayout = std::map<std::string, AnimNodePos, std::less<>>;

/** @brief Where a machine's layout is stored: the machine path plus ".layout", next to it. */
[[nodiscard]] std::filesystem::path LayoutFilePath( std::filesystem::path const& inMachinePath ) noexcept;

/**
 * @brief Writes the layout of a machine to its sidecar file.
 *
 * The layout is editor-only data that the engine never reads. An empty
 * layout removes the file instead of writing an empty one.
 */
[[nodiscard]] asge::BoolResult SaveLayout( std::filesystem::path const& inMachinePath, AnimLayout const& inLayout );

/** @brief Reads a machine's sidecar layout; a missing or unreadable file gives an empty layout. */
[[nodiscard]] AnimLayout LoadLayout( std::filesystem::path const& inMachinePath );

/**
 * @brief Places every state of inDoc that has no position in ioLayout.
 *
 * States that already have one stay where they are. New ones go in columns by
 * their distance from the initial state, one row per state, and states that
 * cannot be reached from it go in a last column. Entries for states that no
 * longer exist are dropped.
 */
void AutoLayout( AnimMachineDoc const& inDoc, AnimLayout& ioLayout );

/** @brief Reads a clip set XML file into the editor's model; same rules as ReadMachine. */
[[nodiscard]] asge::Result<AnimClipSetDoc> ReadClipSet( std::filesystem::path const& inPath );

/** @brief Builds the XML tree of a clip set; empty values are left out. */
[[nodiscard]] asge::formats::xml::XMLNode BuildClipSetXml( AnimClipSetDoc const& inDoc );

/** @brief Checks a clip set file with AnimationClipSet::Load; the machine and frame tables are not opened. */
[[nodiscard]] asge::BoolResult ValidateClipSet( AnimClipSetDoc const& inDoc );

/** @brief Writes a clip set to inPath if the engine accepts it; see WriteMachine. */
[[nodiscard]] asge::BoolResult WriteClipSet( AnimClipSetDoc const& inDoc, std::filesystem::path const& inPath );
