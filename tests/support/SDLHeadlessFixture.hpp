#pragma once

#include <SDL3/SDL.h>

#include <gtest/gtest.h>

#include <iostream>
#include <sstream>
#include <string>

namespace asge::test
{

/**
 * @brief Base fixture giving tests a real, display-less SDL_Window
 *
 * Forces SDL's "dummy" video driver, so no window server / GPU / real
 * display is ever touched -- safe to run unmodified on any CI runner. Only
 * creates the window, not a renderer: SDL's software renderer wraps the
 * window's surface directly, so only one renderer may exist per window at a
 * time. Subclasses that need a raw SDL_Renderer* (e.g. to hand to
 * SDLTexture's constructor directly) should create their own in an
 * overridden SetUp(); subclasses that only need SDLRenderer can construct
 * one against m_Window and, if they need the raw handle back (e.g. for
 * SDL_RenderReadPixels), fetch it via SDL_GetRenderer(m_Window) rather than
 * creating a second one.
 */
class SDLHeadlessTest : public ::testing::Test
{
protected:
    SDL_Window* m_Window{nullptr};

    void SetUp() override
    {
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
        ASSERT_TRUE(SDL_Init(SDL_INIT_VIDEO)) << SDL_GetError();

        m_Window = SDL_CreateWindow("asge-headless-test", 64, 64, SDL_WINDOW_HIDDEN);
        ASSERT_NE(m_Window, nullptr) << SDL_GetError();
    }

    void TearDown() override
    {
        if (m_Window) SDL_DestroyWindow(m_Window);
        SDL_Quit();
    }
};

/**
 * @brief RAII helper that redirects std::cout for the duration of its lifetime
 *
 * asge::LogError() writes straight to std::cout via the engine Logger, with
 * no test hook of its own. This captures that output so tests can assert an
 * error actually fired (and check its message) without adding test-only
 * seams to the logger itself.
 */
class CapturedStdout
{
    std::ostringstream m_Buffer;
    std::streambuf* m_Prior;

public:
    CapturedStdout() : m_Prior(std::cout.rdbuf(m_Buffer.rdbuf())) {}
    ~CapturedStdout() { std::cout.rdbuf(m_Prior); }

    CapturedStdout(CapturedStdout const&) = delete;
    CapturedStdout& operator=(CapturedStdout const&) = delete;

    [[nodiscard]] std::string Str() const { return m_Buffer.str(); }
};

} // namespace asge::test
