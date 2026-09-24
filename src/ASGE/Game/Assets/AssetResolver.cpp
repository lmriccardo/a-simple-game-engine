#include "AssetResolver.hpp"

#include <ASGE/Core/Math/Geometry/CatmullRomSpline.hpp>

void asge::game::asset::Resolver<asge::game::components::Sprite>::operator()(
    AssetManager &inAssetManager, ecs::Registry &inRegistry,
    video::IRenderer &inRenderer, C& inSprite) const noexcept
{
    if ( inSprite.m_VirtualPath == inSprite.m_ResolvedVirtualPath ) return;

    if ( inSprite.m_VirtualPath.empty() )
    {
        inSprite.m_Texture = nullptr;
        inSprite.m_ResolvedVirtualPath.clear();
        return;
    }

    auto texture = inAssetManager.GetTexture( inSprite.m_VirtualPath, inRenderer );
    if ( !texture ) { texture.LogError(); return; }
    inSprite.m_Texture = texture.Value();
    inSprite.m_ResolvedVirtualPath = inSprite.m_VirtualPath;
}

void asge::game::asset::Resolver<asge::game::components::Animation>::operator()(
    AssetManager &inAssetManager, ecs::Registry &inRegistry,
    video::IRenderer &inRenderer, C &inAnimation) const noexcept
{
    if ( inAnimation.m_ClipPath == inAnimation.m_ResolvedClipPath ) return;

    if ( inAnimation.m_ClipPath.empty() )
    {
        inAnimation.m_Clip = nullptr;
        inAnimation.m_ResolvedClipPath.clear();
        return;
    }

    auto frameTable = inAssetManager.GetFrameTable( inAnimation.m_ClipPath );
    if ( !frameTable ) { frameTable.LogError(); return; }
    inAnimation.m_Clip = frameTable.Value();
    inAnimation.m_ResolvedClipPath = inAnimation.m_ClipPath;
}

void asge::game::asset::Resolver<asge::game::components::AudioSource>::operator()(
    AssetManager &inAssetManager, ecs::Registry &inRegistry,
    video::IRenderer &inRenderer, C &inAudioSource) const noexcept
{
    if ( inAudioSource.m_VirtualClipPath == inAudioSource.m_ResolvedVirtualClipPath ) return;

    if ( inAudioSource.m_VirtualClipPath.empty() )
    {
        inAudioSource.m_Clip = nullptr;
        inAudioSource.m_ResolvedVirtualClipPath.clear();
        return;
    }

    auto audioclip = inAssetManager.GetAudio( inAudioSource.m_VirtualClipPath );
    if ( !audioclip ) { audioclip.LogError(); return; }
    inAudioSource.m_Clip = audioclip.Value();
    inAudioSource.m_ResolvedVirtualClipPath = inAudioSource.m_VirtualClipPath;
}

void asge::game::asset::Resolver<asge::game::components::PathFollow>::operator()(
    AssetManager &inAssetManager, ecs::Registry &inRegistry,
    video::IRenderer &inRenderer, C &inPathFollow) const noexcept
{
    if ( inPathFollow.m_Path.HasSegments() || inPathFollow.m_Waypoints.empty() ) return;
    math::CatmullRomSpline spline( inPathFollow.m_Waypoints, inPathFollow.m_Resolution );
    if ( !spline.HasSegments() ) return;
    inPathFollow.m_Path = std::move( spline );
    inPathFollow.m_Waypoints = inPathFollow.m_Path.Waypoints();
}

std::optional<asge::game::asset::AssetRef>
asge::game::asset::AssetRefs<asge::game::components::Sprite>::operator()(
    components::Sprite const &inSprite) const noexcept
{
    if ( inSprite.m_VirtualPath.empty() ) return std::nullopt;
    return AssetRef{ AssetKind::Texture, inSprite.m_VirtualPath };
}

std::optional<asge::game::asset::AssetRef>
asge::game::asset::AssetRefs<asge::game::components::Animation>::operator()(
    components::Animation const &inAnimation) const noexcept
{
    if ( inAnimation.m_ClipPath.empty() ) return std::nullopt;
    return AssetRef{ AssetKind::AnimationClip, inAnimation.m_ClipPath };
}

std::optional<asge::game::asset::AssetRef>
asge::game::asset::AssetRefs<asge::game::components::AudioSource>::operator()(
    components::AudioSource const &inAudioSource) const noexcept
{
    if ( inAudioSource.m_VirtualClipPath.empty() ) return std::nullopt;
    return AssetRef{ AssetKind::AudioClip, inAudioSource.m_VirtualClipPath };
}

std::vector<asge::game::asset::AssetRef> asge::game::asset::CollectAssetRefs(
    ecs::Registry const &inRegistry)
{
    std::vector<AssetRef> refs;

    [&]<typename... Ts>(std::type_identity<std::tuple<Ts...>>) {
        ( [&] {
            for (auto [e, c] : inRegistry.View<Ts>()) {
                if ( auto ref = AssetRefs<Ts>{}( c.get() ) )
                {
                    refs.push_back( std::move(*ref) );
                }
            }
        }(), ... );
    }(std::type_identity<components::SerializableComponents>{});

    return refs;
}