#include "VideoSystem.hpp"
#include "VideoError.hpp"
#include "Graphics/GraphicsFactory.hpp"

using namespace asge;

void asge::video::VideoSystem::Shutdown()
{
    // Destroy renderer and window first (their destructors release the backend resources)
    m_Renderer.reset();
    m_Window.reset();

    if (m_BackendInitialized)
    {
        video::ShutdownBackend(m_Backend);
        m_BackendInitialized = false;
    }
}

BoolResult asge::video::VideoSystem::Initialize(std::string const &inTitle, int inWidth, int inHeight,
    GraphicsBackend inBackend)
{
    m_Backend = inBackend;

    auto const backendResult = video::InitializeBackend(inBackend);
    if (!backendResult)
    {
        return backendResult;
    }
    m_BackendInitialized = true;

    m_Window = video::CreateWindow(inBackend, inTitle, inWidth, inHeight);
    if (!m_Window || !m_Window->IsValid())
    {
        Shutdown();
        return BoolResult::Err(make_error_code(errors::VideoError::WindowCreationFailed));
    }

    m_Renderer = video::CreateRenderer(inBackend, *m_Window);
    if (!m_Renderer || !m_Renderer->IsValid())
    {
        Shutdown();
        return BoolResult::Err(make_error_code(errors::VideoError::RendererCreationFailed));
    }

    return BoolResult::Ok();
}

video::IRenderer &asge::video::VideoSystem::GetRenderer()
{
    return *m_Renderer;
}

video::IRenderer const &asge::video::VideoSystem::GetRenderer() const
{
    return *m_Renderer;
}
