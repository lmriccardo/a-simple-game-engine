#pragma once

#include <ASGE/ASGE.hpp>

// Demonstrates AudioSource/AudioSystem: SPACE (re)plays a short one-shot
// "blip" immediately even while it's still sounding (see AudioSource's
// m_Restart), L toggles a looping ambient hum on/off. Each on-screen
// rectangle is driven straight from its own AudioSource's live state rather
// than a separate visual timer -- the blip rect is lit while its stream
// still has queued audio, the ambient rect while its m_Playing is true.
class AudioDemoState final : public asge::game::state::IGameState<int>
{
    asge::ecs::Registry&             m_Registry;
    asge::game::asset::AssetManager& m_Assets;
    asge::audio::AudioDevice&        m_AudioDev;

    asge::ecs::Entity m_Ambient{ asge::ecs::Entity::Null() };
    asge::ecs::Entity m_Blip{ asge::ecs::Entity::Null() };

    void SpawnSources();

public:
    AudioDemoState(
        asge::ecs::Registry& inRegistry,
        asge::game::asset::AssetManager& inAssets,
        asge::audio::AudioDevice& inAudioDev );

    [[nodiscard]] std::optional<asge::game::state::Transition<int>>
    Update(float inDeltaTime, asge::input::InputState const& inInput) override;
    void Render(asge::video::IRenderer& inRenderer) override;
    void OnSystemEvent(asge::event::SystemEvent const& inSysEvent) override;
};

class AudioDemoGame final : public asge::game::Game<int>
{
public:
    explicit AudioDemoGame(asge::video::IRenderer& inRenderer, asge::audio::AudioDevice& inAudioDev);

protected:
    [[nodiscard]] std::unique_ptr<StateType> CreateState(int inId) override;
};
