#include "AssetResolver.hpp"

#include <ASGE/Core/Math/Geometry/CatmullRomSpline.hpp>

void asge::game::asset::Resolver<asge::game::components::Sprite>::operator()(
    AssetManager &inAssetManager, ecs::Registry &inRegistry, 
    video::IRenderer &inRenderer, C& inSprite) const noexcept
{
    if ( inSprite.m_Texture || inSprite.m_VirtualPath.empty() ) return;
    auto texture = inAssetManager.GetTexture( inSprite.m_VirtualPath, inRenderer );
    if ( !texture ) { texture.LogError(); return; }
    inSprite.m_Texture = texture.Value();
}

void asge::game::asset::Resolver<asge::game::components::Animation>::operator()(
    AssetManager &inAssetManager, ecs::Registry &inRegistry, 
    video::IRenderer &inRenderer, C &inAnimation) const noexcept
{
    if ( inAnimation.m_Clip || inAnimation.m_ClipPath.empty() ) return;
    auto frameTable = inAssetManager.GetFrameTable( inAnimation.m_ClipPath );
    if ( !frameTable ) { frameTable.LogError(); return; }
    inAnimation.m_Clip = frameTable.Value();
}

void asge::game::asset::Resolver<asge::game::components::AudioSource>::operator()(
    AssetManager &inAssetManager, ecs::Registry &inRegistry, 
    video::IRenderer &inRenderer, C &inAudioSource) const noexcept
{
    if ( inAudioSource.m_Clip || inAudioSource.m_VirtualClipPath.empty() ) return;
    auto audioclip = inAssetManager.GetAudio( inAudioSource.m_VirtualClipPath );
    if ( !audioclip ) { audioclip.LogError(); return; }
    inAudioSource.m_Clip = audioclip.Value();
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