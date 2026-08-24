#include "AssetManager.hpp"

asge::Result<asge::game::asset::AssetManager::asset_ptr<asge::graphics::Image>> 
asge::game::asset::AssetManager::LoadImage(str::StringCRef inVirtualPath)
{
    return m_ImagePool.GetOrLoad( m_Vfs, inVirtualPath );
}

asge::Result<asge::game::asset::AssetManager::asset_ptr<asge::graphics::Font>> 
asge::game::asset::AssetManager::LoadFont(str::StringCRef inVirtualPath, int inPixelHeight)
{
    return m_FontPool.GetOrLoad( m_Vfs, inVirtualPath, inPixelHeight );
}
