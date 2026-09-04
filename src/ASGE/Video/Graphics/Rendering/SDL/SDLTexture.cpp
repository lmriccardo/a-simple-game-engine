#include "SDLTexture.hpp"
#include "../RenderError.hpp"
#include "SDLPixelFormat.hpp"

asge::video::SDLTexture::SDLTexture(SDL_Renderer *inRenderer, media::Image const &inImage)
{
    auto const dims = inImage.Dimensions();
    m_Handle = SDL_CreateTexture(
        inRenderer, MapPixelFormat(inImage.Format()), SDL_TEXTUREACCESS_STATIC,
        dims.x(), dims.y()
    );

    if ( !m_Handle )
    {
        LogError( make_error_code( errors::RenderError::TextureCreationFailed ), SDL_GetError() );
        return;
    }

    auto const upload = ExpandPixelsForUpload( inImage );
    bool updateResult = SDL_UpdateTexture( m_Handle, nullptr, upload.s_Data.data(),
        static_cast<int>(upload.s_Stride) );

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

void asge::video::SDLTexture::SetColorMod(media::RGBA_Color inColor) noexcept
{
    if ( !SDL_SetTextureColorMod( m_Handle, inColor.r, inColor.g, inColor.b ) )
    {
        LogError( make_error_code( errors::RenderError::TextureSetColorModFailed ), SDL_GetError() );
    }

    if ( !SDL_SetTextureAlphaMod( m_Handle, inColor.a ) )
    {
        LogError( make_error_code( errors::RenderError::TextureSetColorModFailed ), SDL_GetError() );
    }
}

asge::Result<asge::media::RGBA_Color> asge::video::SDLTexture::GetColorMod() const noexcept
{
    media::RGBA_Color outColor;

    if ( !SDL_GetTextureColorMod( m_Handle, &outColor.r, &outColor.g, &outColor.b ) )
    {
        return Result<media::RGBA_Color>::Err(
            make_error_code( errors::RenderError::TextureGetColorModFailed ), 
            SDL_GetError());
    }

    if ( !SDL_GetTextureAlphaMod( m_Handle, &outColor.a ) )
    {
        return Result<media::RGBA_Color>::Err(
            make_error_code( errors::RenderError::TextureGetColorModFailed ), 
            SDL_GetError());
    }

    return Result<media::RGBA_Color>::Ok( outColor );
}

void asge::video::SDLTexture::Destroy()
{
    if ( !m_Handle ) return;
    SDL_DestroyTexture( m_Handle );
    m_Handle = nullptr;
}
