#include "AssetManager.hpp"

#include <ASGE/Game/Components/Sprite.hpp>
#include <ASGE/Game/Components/AudioSource.hpp>
#include <ASGE/Game/Components/Animator.hpp>
#include <ASGE/Game/Components/UI/UILabel.hpp>
#include <algorithm>
#include <ASGE/Video/Graphics/Rendering/RenderError.hpp>

#include "AssetResolver.hpp"

asge::Result<asge::game::asset::AssetManager::asset_ptr<asge::media::Image>>
asge::game::asset::AssetManager::GetImage(str::StringCRef inVirtualPath)
{
    return m_ImagePool.GetOrLoad( m_Vfs, inVirtualPath );
}

asge::Result<asge::game::asset::AssetManager::asset_ptr<asge::media::Font>>
asge::game::asset::AssetManager::GetFont(str::StringCRef inVirtualPath, int inPixelHeight)
{
    return m_FontPool.GetOrLoad( m_Vfs, inVirtualPath, inPixelHeight );
}

asge::Result<asge::game::asset::AssetManager::asset_ptr<asge::game::asset::FrameTable>> 
asge::game::asset::AssetManager::GetFrameTable(str::StringCRef inVirtualPath)
{
    return m_FrameTables.GetOrLoad( m_Vfs, inVirtualPath );
}

asge::Result<asge::game::asset::AssetManager::asset_ptr<asge::media::AudioClip>> 
asge::game::asset::AssetManager::GetAudio(str::StringCRef inVirtualPath)
{
    return m_AudioPool.GetOrLoad( m_Vfs, inVirtualPath );
}

asge::Result<asge::game::asset::AssetManager::asset_ptr<asge::game::asset::AnimationClipSet>> 
asge::game::asset::AssetManager::GetClipSet(str::StringCRef inVirtualPath)
{
    return m_AnimClipSet.GetOrLoad( m_Vfs, inVirtualPath );
}

asge::Result<asge::game::asset::AssetManager::asset_ptr<asge::game::asset::AnimationStateMachine>>
asge::game::asset::AssetManager::GetStateMachine(str::StringCRef inVirtualPath)
{
    return m_AnimStateMachines.GetOrLoad( m_Vfs, inVirtualPath );
}

asge::Result<asge::video::ITexture*> asge::game::asset::AssetManager::GetTexture(
    str::StringCRef inVirtualPath, video::IRenderer &inRenderer) noexcept
{
    if ( auto it = m_TextureCache.find( inVirtualPath ); it != m_TextureCache.end() )
        return Result<video::ITexture*>::Ok( it->second.get() );

    auto image = GetImage( inVirtualPath );
    if ( !image ) return Result<video::ITexture*>::Err( image.Error() );

    auto texture = inRenderer.CreateTexture( image.Value()->Get() );
    if ( !texture )
        return Result<video::ITexture*>::Err( make_error_code( errors::RenderError::TextureCreationFailed ) );

    auto* raw = texture.get();
    m_TextureCache.emplace( str::String( inVirtualPath ), std::move( texture ) );
    return Result<video::ITexture*>::Ok( raw );
}

void asge::game::asset::AssetManager::UnloadTexture(str::StringCRef inVirtualPath) noexcept
{
    m_TextureCache.erase( inVirtualPath );
}

void asge::game::asset::AssetManager::ReloadTexture(
    str::StringCRef inVirtualPath, ecs::Registry &inRegistry) noexcept
{
    // Un-resolve first: Sprite::m_Texture points into m_TextureCache, so it
    // must not be left dangling once UnloadTexture frees the entry.
    for ( auto [e, sprite] : inRegistry.View<components::Sprite>().IncludeDisabled() )
    {
        if ( sprite.get().m_ResolvedVirtualPath != inVirtualPath ) continue;
        sprite.get().m_Texture = nullptr;
        sprite.get().m_ResolvedVirtualPath.clear();
    }

    UnloadTexture( inVirtualPath );
    m_ImagePool.Remove( inVirtualPath );
}

void asge::game::asset::AssetManager::ReloadAsset( str::StringCRef inVirtualPath, ecs::Registry& inRegistry ) noexcept
{
    ReloadTexture( inVirtualPath, inRegistry );

    for ( auto [e, audio] : inRegistry.View<components::AudioSource>().IncludeDisabled() )
    {
        if ( audio.get().m_ResolvedVirtualClipPath != inVirtualPath ) continue;
        audio.get().m_Clip = nullptr;
        audio.get().m_ResolvedVirtualClipPath.clear();
    }
    m_AudioPool.Remove( inVirtualPath );

    for ( auto [e, label] : inRegistry.View<components::UILabel>().IncludeDisabled() )
    {
        auto& l = label.get();
        if ( l.m_ResolvedFontPath != inVirtualPath ) continue;
        if ( l.m_Font ) m_FontAtlasCache.erase( l.m_Font );
        m_FontPool.Remove( inVirtualPath, l.m_ResolvedFontPixelHeight );
        l.m_Font = nullptr;
        l.m_ResolvedFontPath.clear();
        l.m_ResolvedFontPixelHeight = 0;
    }

    for ( auto [e, animator] : inRegistry.View<components::Animator>().IncludeDisabled() )
    {
        auto& a = animator.get();
        if ( !a.m_ClipSet ) continue;
        auto const& set = a.m_ClipSet->Get();
        auto const paths = set.ClipPaths();
        bool const uses = a.m_ClipSet->VirtualPath() == inVirtualPath || set.MachinePath() == inVirtualPath
            || std::ranges::find( paths, inVirtualPath ) != paths.end();
        if ( !uses ) continue;
        m_AnimClipSet.Remove( str::String( a.m_ClipSet->VirtualPath() ) );
        a.m_ClipSet = nullptr;
    }
    m_AnimClipSet.Remove( inVirtualPath );
    m_AnimStateMachines.Remove( inVirtualPath );
    m_FrameTables.Remove( inVirtualPath );
}

asge::Result<asge::video::ITexture*> asge::game::asset::AssetManager::GetFontAtlasTexture(
    media::Font const &inFont, video::IRenderer &inRenderer) noexcept
{
    if ( auto it = m_FontAtlasCache.find( &inFont ); it != m_FontAtlasCache.end() )
        return Result<video::ITexture*>::Ok( it->second.get() );

    auto texture = inRenderer.CreateTexture( inFont.GetAtlasImage() );
    if ( !texture )
        return Result<video::ITexture*>::Err( make_error_code( errors::RenderError::TextureCreationFailed ) );

    auto* raw = texture.get();
    m_FontAtlasCache.emplace( &inFont, std::move( texture ) );
    return Result<video::ITexture*>::Ok( raw );
}

void asge::game::asset::AssetManager::ResolveAssets(
    ecs::Registry &inRegistry, video::IRenderer &inRenderer)
{
    [&]<typename... Ts>(std::type_identity<std::tuple<Ts...>>) {
        ( [&] {
            for (auto [e, c] : inRegistry.View<Ts>().IncludeDisabled()) {
                Resolver<Ts>{}(*this, inRegistry, inRenderer, c.get());
            }
        }(), ... );
    }(std::type_identity<components::SerializableComponents>{});
}
