#include "Game.hpp"

#include <algorithm>

namespace
{
using namespace asge::game::components;

constexpr char const* kAmbientPath = "audio/ambient.ogg";
constexpr char const* kBlipPath    = "audio/blip.ogg";
constexpr float kVolumeStep = 0.1f;
}

AudioDemoState::AudioDemoState(
    asge::ecs::Registry& inRegistry,
    asge::game::asset::AssetManager& inAssets,
    asge::audio::AudioDevice& inAudioDev
)
: m_Registry(inRegistry), m_Assets(inAssets), m_AudioDev(inAudioDev)
{
    SpawnSources();
}

void AudioDemoState::SpawnSources()
{
    // Unlike Sprite/Animation, resolving an AudioClip doesn't need a
    // renderer -- so both sources are resolved eagerly here instead of
    // going through AssetManager::ResolveAssets' deferred per-frame pass.
    auto ambient = m_Registry.CreateEntity();
    if ( !ambient ) { ambient.LogError(); return; }
    m_Ambient = ambient.Value();

    AudioSource ambientSource{};
    auto ambientClip = m_Assets.GetAudio(kAmbientPath);
    if ( !ambientClip ) ambientClip.LogError();
    else ambientSource.m_Clip = ambientClip.Value();

    auto addedAmbient = m_Registry.AddComponent<AudioSource>(m_Ambient, ambientSource);
    if ( !addedAmbient ) { addedAmbient.LogError(); return; }
    if ( ambientSource.m_Clip ) PlayAudioSource( addedAmbient.Value().get(), /*inLoop=*/true );

    auto blip = m_Registry.CreateEntity();
    if ( !blip ) { blip.LogError(); return; }
    m_Blip = blip.Value();

    AudioSource blipSource{};
    auto blipClip = m_Assets.GetAudio(kBlipPath);
    if ( !blipClip ) blipClip.LogError();
    else blipSource.m_Clip = blipClip.Value();

    auto addedBlip = m_Registry.AddComponent<AudioSource>(m_Blip, blipSource);
    if ( !addedBlip ) addedBlip.LogError();
}

void AudioDemoState::AdjustMasterVolume(float inDelta)
{
    m_MasterVolume = std::clamp(m_MasterVolume + inDelta, 0.0f, 1.0f);
    auto result = m_AudioDev.SetGain(m_MasterVolume);
    if ( !result ) result.LogError();
}

std::optional<asge::game::state::Transition<int>>
AudioDemoState::Update(
    [[maybe_unused]] float inDeltaTime, [[maybe_unused]] asge::input::InputState const& inInput)
{
    asge::game::systems::AudioSystem( m_Registry, m_AudioDev );
    return std::nullopt;
}

void AudioDemoState::Render(asge::video::IRenderer &inRenderer)
{
    inRenderer.Clear({ 18, 18, 24, 255 });

    bool ambientPlaying = false;
    if ( auto ambient = m_Registry.GetComponent<AudioSource>(m_Ambient) )
        ambientPlaying = ambient.Value().get().m_Playing;

    bool blipSounding = false;
    if ( auto blip = m_Registry.GetComponent<AudioSource>(m_Blip) )
    {
        auto& source = blip.Value().get();
        blipSounding = source.m_Stream && source.m_Stream->IsDataAvailable();
    }

    // "AMBIENT" indicator (left) -- lit while the looping background hum plays.
    inRenderer.DrawRect(
        asge::math::Rect{ 120.0f, 220.0f, 200.0f, 160.0f },
        ambientPlaying ? asge::media::RGBA_Color{ 80, 220, 140, 255 } : asge::media::RGBA_Color{ 55, 55, 60, 255 },
        true
    );

    // "BLIP" indicator (right) -- lit while its stream still has data queued,
    // dark red once D has detached it (SPACE then creates a fresh stream).
    bool blipDetached = true;
    if ( auto blip = m_Registry.GetComponent<AudioSource>(m_Blip) )
        blipDetached = blip.Value().get().m_Stream == nullptr;

    inRenderer.DrawRect(
        asge::math::Rect{ 480.0f, 220.0f, 200.0f, 160.0f },
        blipSounding      ? asge::media::RGBA_Color{ 240, 200, 60, 255 } :
        blipDetached      ? asge::media::RGBA_Color{ 90, 40, 40, 255 }  :
                             asge::media::RGBA_Color{ 55, 55, 60, 255 },
        true
    );

    // Master volume bar (bottom): outline is the full 0..1 range, the fill
    // tracks m_MasterVolume -- both driven by AudioDevice::SetGain via
    // AdjustMasterVolume, not a separate progress animation.
    asge::math::Rect const volumeTrack{ 120.0f, 440.0f, 560.0f, 24.0f };
    inRenderer.DrawRect(volumeTrack, asge::media::RGBA_Color{ 90, 90, 100, 255 }, false);
    inRenderer.DrawRect(
        asge::math::Rect{ volumeTrack.m_X, volumeTrack.m_Y, volumeTrack.m_Width * m_MasterVolume, volumeTrack.m_Height },
        asge::media::RGBA_Color{ 100, 170, 240, 255 },
        true
    );
}

void AudioDemoState::OnSystemEvent(asge::event::SystemEvent const &inSysEvent)
{
    auto const* keyEvent = inSysEvent.TryGet<asge::event::KeyboardEvent>();
    if ( !keyEvent ) return;
    if ( keyEvent->s_Type != asge::event::EventType::KEYBOARD_KEY_PRESSED ) return;
    if ( keyEvent->s_Repeat ) return; // edge-triggered: one toggle/replay per physical press

    if ( keyEvent->s_Keycode == asge::input::Keycode::SPACE )
    {
        auto blip = m_Registry.GetComponent<AudioSource>(m_Blip);
        if ( blip ) PlayAudioSource( blip.Value().get(), /*inLoop=*/false );
    }
    else if ( keyEvent->s_Keycode == asge::input::Keycode::L )
    {
        auto ambient = m_Registry.GetComponent<AudioSource>(m_Ambient);
        if ( !ambient ) return;

        auto& source = ambient.Value().get();
        if ( source.m_Playing ) StopAudioSource( source );
        else PlayAudioSource( source, /*inLoop=*/true );
    }
    else if ( keyEvent->s_Keycode == asge::input::Keycode::UP )
    {
        AdjustMasterVolume( kVolumeStep );
    }
    else if ( keyEvent->s_Keycode == asge::input::Keycode::DOWN )
    {
        AdjustMasterVolume( -kVolumeStep );
    }
    else if ( keyEvent->s_Keycode == asge::input::Keycode::D )
    {
        auto blip = m_Registry.GetComponent<AudioSource>(m_Blip);
        if ( !blip ) return;

        auto result = DetachAudioSource( m_AudioDev, blip.Value().get() );
        if ( !result ) result.LogError();
    }
}

AudioDemoGame::AudioDemoGame(asge::video::IRenderer& inRenderer, asge::audio::AudioDevice& inAudioDev)
: Game(inRenderer, inAudioDev)
{
    // ASGE_AUDIO_DEMO_ASSET_DIR is injected by CMakeLists.txt; mounted once
    // so clips are loaded by virtual path (see SpawnSources) instead of a
    // hardcoded OS path baked into this demo.
    auto mountResult = m_Vfs.Mount("audio", ASGE_AUDIO_DEMO_ASSET_DIR);
    if ( !mountResult ) mountResult.LogError();

    SetInitialState(0);
}

std::unique_ptr<AudioDemoGame::StateType> AudioDemoGame::CreateState([[maybe_unused]] int inId)
{
    return std::make_unique<AudioDemoState>( m_SceneManager.GetRegistry(), m_Assets, m_AudioDev );
}
