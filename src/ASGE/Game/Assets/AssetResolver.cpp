#include "AssetResolver.hpp"

#include <ASGE/Core/Math/Interpolation/CatmullRomSpline.hpp>
#include <ASGE/Core/Functools.hpp>

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
        inAnimation.m_CurrentFrame = 0;
        inAnimation.m_ElapsedTime = 0.0f;
        return;
    }

    auto frameTable = inAssetManager.GetFrameTable( inAnimation.m_ClipPath );
    if ( !frameTable ) { frameTable.LogError(); return; }
    inAnimation.m_Clip = frameTable.Value();
    inAnimation.m_ResolvedClipPath = inAnimation.m_ClipPath;
    inAnimation.m_CurrentFrame = 0;
    inAnimation.m_ElapsedTime = 0.0f;
}

void asge::game::asset::Resolver<asge::game::components::Animator>::operator()(
    AssetManager &inAssetManager, ecs::Registry &inRegistry,
    video::IRenderer &inRenderer, C &inAnimator) const noexcept
{
    // Flags, triggers and the state id only mean something for one machine,
    // so a new (or cleared) clip set starts the animator from scratch
    auto const reset = [&inAnimator]( C::clip_set inClipSet ) {
        inAnimator.m_ClipSet  = std::move( inClipSet );
        inAnimator.m_State    = 0;
        inAnimator.m_Frame    = 0;
        inAnimator.m_Elapsed  = 0.0f;
        inAnimator.m_Flags    = 0;
        inAnimator.m_Triggers = 0;
        inAnimator.m_Dirty    = true;
        inAnimator.m_Started  = false;
    };

    if ( inAnimator.m_ClipSetPath.empty() )
    {
        if ( inAnimator.m_ClipSet ) reset( nullptr );
        return;
    }

    if ( inAnimator.m_ClipSet && inAnimator.m_ClipSet->VirtualPath() == inAnimator.m_ClipSetPath ) return;

    auto clipSet = inAssetManager.GetClipSet( inAnimator.m_ClipSetPath );
    if ( !clipSet ) { clipSet.LogError(); return; }

    // The clip set is shared, so only the first Animator pays for resolving it
    if ( !clipSet.Value()->Get().IsResolved() )
    {
        auto resolved = clipSet.Value()->Get().ResolveAssets( inAssetManager );
        if ( !resolved ) { resolved.LogError(); return; }
    }

    reset( clipSet.Value() );
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

void asge::game::asset::Resolver<asge::game::components::Path>::operator()(
    AssetManager &inAssetManager, ecs::Registry &inRegistry,
    video::IRenderer &inRenderer, C &inPath) const noexcept
{
    if ( inPath.m_Path.HasSegments() || inPath.m_Waypoints.empty() ) return;
    math::CatmullRomSpline spline( inPath.m_Waypoints, inPath.m_Resolution, inPath.m_Linear );
    if ( !spline.HasSegments() ) return;
    inPath.m_Path = std::move( spline );
    inPath.m_Waypoints = inPath.m_Path.Waypoints();
}

void asge::game::asset::Resolver<asge::game::components::UILabel>::operator()(
    AssetManager &inAssetManager, ecs::Registry &inRegistry,
    video::IRenderer &inRenderer, C &inLabel) const noexcept
{
    // AssetManager::GetFont caches by (path, pixel height) -- the same path
    // baked at a different size is a different Font asset, so a pixel-height
    // edit alone (Font Path unchanged) must still re-resolve, not just a
    // path change.
    if ( inLabel.m_FontPath == inLabel.m_ResolvedFontPath
      && inLabel.m_FontPixelHeight == inLabel.m_ResolvedFontPixelHeight ) return;
    if ( inLabel.m_FontPath.empty() )
    {
        inLabel.m_Font = nullptr;
        inLabel.m_Texture = nullptr;
        inLabel.m_ResolvedFontPath.clear();
        inLabel.m_ResolvedFontPixelHeight = 0;
        return;
    }

    auto fontAsset = inAssetManager.GetFont( inLabel.m_FontPath, inLabel.m_FontPixelHeight);
    if ( !fontAsset ) { fontAsset.LogError(); return; }
    inLabel.m_Font = &fontAsset.Value()->Get();

    // The atlas is already a decoded Image baked into inLabel.m_Font at
    // load time -- GetFontAtlasTexture turns it into a GPU texture
    // directly, rather than (incorrectly) re-resolving m_FontPath through
    // the image/texture pool as if the font file itself were a picture.
    auto texture = inAssetManager.GetFontAtlasTexture( *inLabel.m_Font, inRenderer );
    if ( !texture ) { texture.LogError(); return; }
    inLabel.m_Texture = texture.Value();

    inLabel.m_ResolvedFontPath = inLabel.m_FontPath;
    inLabel.m_ResolvedFontPixelHeight = inLabel.m_FontPixelHeight;
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
asge::game::asset::AssetRefs<asge::game::components::Animator>::operator()(
    components::Animator const &inAnimator) const noexcept
{
    if ( inAnimator.m_ClipSetPath.empty() ) return std::nullopt;
    return AssetRef{ AssetKind::AnimationClipSet, inAnimator.m_ClipSetPath };
}

std::optional<asge::game::asset::AssetRef>
asge::game::asset::AssetRefs<asge::game::components::AudioSource>::operator()(
    components::AudioSource const &inAudioSource) const noexcept
{
    if ( inAudioSource.m_VirtualClipPath.empty() ) return std::nullopt;
    return AssetRef{ AssetKind::AudioClip, inAudioSource.m_VirtualClipPath };
}

std::optional<asge::game::asset::AssetRef>
asge::game::asset::AssetRefs<asge::game::components::UILabel>::operator()(
    components::UILabel const &inLabel) const noexcept
{
    if ( inLabel.m_FontPath.empty() ) return std::nullopt;
    return AssetRef{ AssetKind::Font, inLabel.m_FontPath };
}

std::vector<asge::game::asset::AssetRef> asge::game::asset::CollectAssetRefs(
    ecs::Registry const &inRegistry)
{
    std::vector<AssetRef> refs;

    functools::ForEachTupleType<components::SerializableComponents>
    (
        [&]<typename Ts> {
            for (auto [e, c] : inRegistry.View<Ts>()) {
                if ( auto ref = AssetRefs<Ts>{}( c.get() ) )
                {
                    refs.push_back( std::move(*ref) );
                }
            }
        }
    );

    return refs;
}