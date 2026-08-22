#include <ASGE/Events/Events.hpp>

#include <SDL3/SDL_keycode.h>

#include <gtest/gtest.h>

#include <array>

namespace
{

using asge::event::_internal::ToKeycode;
using asge::event::_internal::ToSdlKeycode;
using asge::input::Keycode;

// Every named key this engine currently maps, paired with its SDL keycode.
// Both translation directions and the round-trip test walk this table, so
// adding a new key only means adding one row here.
struct KeycodeMapping
{
    Keycode     asgeKeycode;
    SDL_Keycode sdlKeycode;
};

constexpr std::array kKnownMappings = {
    KeycodeMapping{ Keycode::ESCAPE, SDLK_ESCAPE },
    KeycodeMapping{ Keycode::SPACE,  SDLK_SPACE },
    KeycodeMapping{ Keycode::A, SDLK_A }, KeycodeMapping{ Keycode::B, SDLK_B },
    KeycodeMapping{ Keycode::C, SDLK_C }, KeycodeMapping{ Keycode::D, SDLK_D },
    KeycodeMapping{ Keycode::E, SDLK_E }, KeycodeMapping{ Keycode::F, SDLK_F },
    KeycodeMapping{ Keycode::G, SDLK_G }, KeycodeMapping{ Keycode::H, SDLK_H },
    KeycodeMapping{ Keycode::I, SDLK_I }, KeycodeMapping{ Keycode::J, SDLK_J },
    KeycodeMapping{ Keycode::K, SDLK_K }, KeycodeMapping{ Keycode::L, SDLK_L },
    KeycodeMapping{ Keycode::M, SDLK_M }, KeycodeMapping{ Keycode::N, SDLK_N },
    KeycodeMapping{ Keycode::O, SDLK_O }, KeycodeMapping{ Keycode::P, SDLK_P },
    KeycodeMapping{ Keycode::Q, SDLK_Q }, KeycodeMapping{ Keycode::R, SDLK_R },
    KeycodeMapping{ Keycode::S, SDLK_S }, KeycodeMapping{ Keycode::T, SDLK_T },
    KeycodeMapping{ Keycode::U, SDLK_U }, KeycodeMapping{ Keycode::V, SDLK_V },
    KeycodeMapping{ Keycode::W, SDLK_W }, KeycodeMapping{ Keycode::X, SDLK_X },
    KeycodeMapping{ Keycode::Y, SDLK_Y }, KeycodeMapping{ Keycode::Z, SDLK_Z },
};

// ─── Keycode is dense ───────────────────────────────────────────────────────

TEST(KeycodeTest, Values_AreContiguousFromZero)
{
    // The whole point of reworking Keycode away from SDL's ASCII-derived
    // values was to make it usable as an array index (e.g. a future
    // InputState's per-key table). If this ever breaks, that guarantee is gone.
    for (std::size_t i = 0; i < static_cast<std::size_t>(Keycode::COUNT); ++i)
        EXPECT_EQ(i, static_cast<std::size_t>(static_cast<Keycode>(i)));
}

// ─── SDL -> asge ────────────────────────────────────────────────────────────

TEST(KeycodeTranslationTest, ToKeycode_KnownSdlKeycodesMapCorrectly)
{
    for (auto const& mapping : kKnownMappings)
        EXPECT_EQ(ToKeycode(mapping.sdlKeycode), mapping.asgeKeycode);
}

TEST(KeycodeTranslationTest, ToKeycode_UnmappedSdlKeycodeIsUnknown)
{
    // SDLK_RETURN isn't in the table yet -- must fall back to UNKNOWN
    // rather than aliasing some unrelated Keycode.
    EXPECT_EQ(ToKeycode(SDLK_RETURN), Keycode::UNKNOWN);
    EXPECT_EQ(ToKeycode(SDLK_UNKNOWN), Keycode::UNKNOWN);
}

// ─── asge -> SDL ────────────────────────────────────────────────────────────

TEST(KeycodeTranslationTest, ToSdlKeycode_KnownKeycodesMapCorrectly)
{
    for (auto const& mapping : kKnownMappings)
        EXPECT_EQ(ToSdlKeycode(mapping.asgeKeycode), mapping.sdlKeycode);
}

TEST(KeycodeTranslationTest, ToSdlKeycode_UnknownMapsToSdlUnknown)
{
    EXPECT_EQ(ToSdlKeycode(Keycode::UNKNOWN), SDLK_UNKNOWN);
}

// ─── Round-trip ─────────────────────────────────────────────────────────────

TEST(KeycodeTranslationTest, RoundTrip_SdlToAsgeToSdlIsLossless)
{
    for (auto const& mapping : kKnownMappings)
        EXPECT_EQ(ToSdlKeycode(ToKeycode(mapping.sdlKeycode)), mapping.sdlKeycode);
}

TEST(KeycodeTranslationTest, RoundTrip_AsgeToSdlToAsgeIsLossless)
{
    for (auto const& mapping : kKnownMappings)
        EXPECT_EQ(ToKeycode(ToSdlKeycode(mapping.asgeKeycode)), mapping.asgeKeycode);
}

}
