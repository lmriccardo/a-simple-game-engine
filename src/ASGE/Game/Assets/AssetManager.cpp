#include "AssetManager.hpp"

#include <ASGE/Game/Components/Animation.hpp>
#include <ASGE/Game/Components/Sprite.hpp>

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

void asge::game::asset::AssetManager::ResolveAssets(ecs::Registry &inRegistry, video::IRenderer &inRenderer)
{
    // First we need to resolve all textures for each sprite
    for ( auto [e, s] : inRegistry.View<components::Sprite>() )
    {
        auto& sprite = s.get();
        if ( sprite.m_Texture || sprite.m_VirtualPath.empty() ) continue;
        auto image = GetImage( sprite.m_VirtualPath );
        if ( !image ) { image.LogError(); continue; }

        auto texture = inRenderer.CreateTexture( image.Value()->Get() );
        if ( !texture ) continue; // Left null -- retried on the next ResolveAssets call

        sprite.m_Texture = texture.get();
        m_Textures.push_back( std::move(texture) ); // AssetManager keeps it alive -- Sprite::m_Texture is non-owning
    }

    // Compute also the FrameTable for each Animation component
    for ( auto [e, anim] : inRegistry.View<components::Animation>() )
    {
        auto& a = anim.get();
        if ( a.m_Clip || a.m_ClipPath.empty() ) continue;
        auto frameTable = GetFrameTable( a.m_ClipPath );
        if ( !frameTable ) { frameTable.LogError(); continue; }
        a.m_Clip = frameTable.Value();
    }
}
