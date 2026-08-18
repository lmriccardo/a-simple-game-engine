#include "SDLTexture.hpp"
#include "../RenderError.hpp"

asge::video::SDLTexture::SDLTexture(SDL_Renderer *inRenderer, graphics::Image const &inImage)
{
    auto const dims = inImage.Dimensions();
    m_Handle = SDL_CreateTexture(
        inRenderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC,
        dims.x(), dims.y()
    );

    if ( !m_Handle )
    {
        LogError( make_error_code( errors::RenderError::TextureCreationFailed ), SDL_GetError() );
        return;
    }
    
    bool updateResult = SDL_UpdateTexture( m_Handle, nullptr, inImage.Data(),
        static_cast<int>(inImage.Stride()) );

    if ( !updateResult )
    {
        LogError( make_error_code( errors::RenderError::TextureUpdateFailed ), SDL_GetError() );
        Destroy(); // Don't leave a texture with no uploaded pixel data reporting IsValid() == true
        return;
    }
}

asge::video::SDLTexture::SDLTexture(SDLTexture &&inOther)
: m_Handle( inOther.m_Handle )
{
    inOther.m_Handle = nullptr;
}

asge::video::SDLTexture &asge::video::SDLTexture::operator=(SDLTexture &&inOther)
{
    if ( this != &inOther )
    {
        m_Handle = inOther.m_Handle;
        inOther.m_Handle = nullptr;
    }

    return *this;
}

asge::video::SDLTexture::~SDLTexture()
{
    Destroy();
}

asge::math::Int2 asge::video::SDLTexture::Size() const noexcept
{
    if ( !m_Handle ) return { 0, 0 };
    return { m_Handle->w, m_Handle->h };
}

void *asge::video::SDLTexture::NativeHandle() const noexcept
{
    return m_Handle;
}

bool asge::video::SDLTexture::IsValid() const noexcept
{
    // Calls SDL_GetTextureProperties to check if it is a valid texture
    return SDL_GetTextureProperties( m_Handle ) != 0;
}

void asge::video::SDLTexture::Destroy()
{
    if ( !m_Handle ) return;
    SDL_DestroyTexture( m_Handle );
    m_Handle = nullptr;
}
