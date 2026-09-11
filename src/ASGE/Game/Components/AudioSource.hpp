#pragma once

#include <memory>
#include <ASGE/Game/Assets/Asset.hpp>
#include <ASGE/Core/Strings.hpp>
#include <ASGE/Core/Media/AudioClip.hpp>
#include "Serialize.hpp"

namespace asge::game::components
{

struct AudioSource
{
    using audio_clip_asset = std::shared_ptr<asset::Asset<media::AudioClip>>;

    audio_clip_asset    m_Clip              { nullptr };
    str::String         m_VirtualClipPath   {};
    bool                m_Playing           { false };
    bool                m_Loop              { false };
    float               m_Volume            { 1.0f };
};

inline void PlayAudioSource( AudioSource& inAudioSource, bool inLoop = true ) noexcept
{
    inAudioSource.m_Playing = true;
    inAudioSource.m_Loop = inLoop;
}

inline void StopAudioSource( AudioSource& inAudioSource ) noexcept
{
    inAudioSource.m_Playing = false;
}

template<>
struct Serializer<AudioSource>
{
    static constexpr str::StringView kTableName = "AudioSource";

    using T = AudioSource;

    static void ToToml( T inValue, asge::config::toml::TOMLTableView inTview ) noexcept
    {
        inTview.Table( str::String( kTableName ) )
               .Set<str::String>( "m_VirtualClipPath", inValue.m_VirtualClipPath );
    }

    static T FromToml( asge::config::toml::TOMLTableView inTview ) noexcept
    {
        auto table = inTview.Table( str::String( kTableName ) );
        AudioSource result;
        result.m_VirtualClipPath = table.Get("m_VirtualClipPath", str::String{});
        return result;
    }
};

}