#pragma once

#include "AnimStateMachine.hpp"
#include "Asset.hpp"

namespace asge::game::asset
{

class AssetManager;

class AnimationClipSet
{
public:
    struct Binding
    {
        std::shared_ptr<Asset<FrameTable>> m_Clip{}; // Null until asset resolution
        float m_FrameDuration{0.1f};                 // Seconds per frame
    };
private:
    std::vector<Binding> m_Bindings{};
    std::shared_ptr<Asset<AnimationStateMachine>> m_Machine{}; // Null until asset resolution

    // Set up once at load time; same order as m_Bindings
    str::String              m_MachinePath {}; // Path of the state machine file
    std::vector<str::String> m_ClipNames   {}; // Clip names
    std::vector<str::String> m_ClipPaths   {}; // FrameTable paths
public:
    /**
     * @brief Loads a clip set from an XML file.
     *
     * The root is `<AnimClipSet machine="...">` holding `<Clip name path
     * [frameDuration]>` elements. Only the file itself is read: the machine
     * and the frame tables are stored as paths and resolved later. Missing,
     * blank or malformed attributes, duplicate names and unknown elements
     * return a specific AnimStateMachineErrors.
     */
    [[nodiscard]] static Result<AnimationClipSet> Load(filesystem::Path const&);

    /**
     * @brief Loads the state machine and every frame table through inAssetManager.
     *
     * Every clip the machine declares must be bound by this set and every
     * clip of the set must be declared by the machine, otherwise it fails
     * with ClipNotFound. On success the bindings are reordered to the
     * machine's clip order, so Bindings()[State::m_ClipId] is the clip a state
     * plays. On failure nothing changes. Calling it again re-reads the
     * assets through the manager's caches.
     */
    [[nodiscard]] BoolResult ResolveAssets( AssetManager& inAssetManager );

    /**
     * @brief Returns the state machine this set binds clips for.
     *
     * Only valid once the machine asset has been resolved; before that the
     * handle is null and calling this is undefined.
     */
    [[nodiscard]] AnimationStateMachine const& Machine() const noexcept;

    /**
     * @brief Returns the clip bindings.
     *
     * Binding i belongs to the clip called ClipNames()[i]. That is the
     * declaration order until ResolveAssets() runs, the machine's clip order
     * afterwards.
     */
    [[nodiscard]] std::span<Binding const>     Bindings() const noexcept;

    /** @brief Returns the state machine path as written in the file. */
    [[nodiscard]] str::StringView MachinePath() const noexcept;

    /** @brief Returns the clip names, parallel to Bindings(). */
    [[nodiscard]] std::span<str::String const> ClipNames() const noexcept;

    /** @brief Returns the frame table paths, parallel to Bindings(). */
    [[nodiscard]] std::span<str::String const> ClipPaths() const noexcept;
};

}
