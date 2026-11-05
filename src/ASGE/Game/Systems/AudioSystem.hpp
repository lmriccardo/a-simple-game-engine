#pragma once

#include <ASGE/Audio/AudioDevice.hpp>
#include <ASGE/Core/ECS/Registry.hpp>

namespace asge::game::systems
{

/**
 * @brief Drives every entity's AudioSource.
 *
 * Starts newly-playing sources, honors PlayAudioSource's restarts, restarts
 * looping ones that ran dry, and stops finished non-looping ones. A source's
 * audio::AudioStream is created via inDevice on its first tick and then kept,
 * so replaying always reuses it.
 */
void AudioSystem( ecs::Registry& inRegistry, audio::AudioDevice& inDevice );

}