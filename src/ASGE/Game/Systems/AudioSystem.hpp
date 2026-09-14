#pragma once

#include <ASGE/Audio/AudioDevice.hpp>
#include <ASGE/Core/ECS/Registry.hpp>

namespace asge::game::systems
{

/**
 * @brief Drives every entity's AudioSource: starts newly-playing sources,
 *        honors PlayAudioSource's restart requests, restarts looping ones
 *        that ran out of queued audio, stops ones that finished without
 *        looping, and clears data for stopped ones.
 *
 * A source's audio::AudioStream is created (via inDevice) the first tick it
 * plays and then kept for that AudioSource's lifetime — replaying it (via
 * PlayAudioSource, whether it's still playing or already stopped) always
 * reuses that same stream rather than creating a new one.
 */
void AudioSystem( ecs::Registry& inRegistry, audio::AudioDevice& inDevice );

}