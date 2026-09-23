#include "AssetManager.hpp"

#include <ASGE/Game/Components/Animation.hpp>
#include <ASGE/Game/Components/Sprite.hpp>
#include <ASGE/Game/Components/AudioSource.hpp>
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

void asge::game::asset::AssetManager::ResolveAssets(
    ecs::Registry &inRegistry, video::IRenderer &inRenderer)
{
    [&]<typename... Ts>(std::type_identity<std::tuple<Ts...>>) {
        ( [&] {
            for (auto [e, c] : inRegistry.View<Ts>()) {
                Resolver<Ts>{}(*this, inRegistry, inRenderer, c.get());
            }
        }(), ... );
    }(std::type_identity<components::SerializableComponents>{});
}
