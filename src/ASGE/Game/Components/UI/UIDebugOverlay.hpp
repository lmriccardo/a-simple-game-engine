#pragma once

namespace asge::game::components
{

/**
 * @brief Marks a UILabel entity as a live "FPS / entity count" readout, kept up to date by
 *        systems::UIDebugOverlaySystem. Runtime-only: never serialized.
 */
struct UIDebugOverlay
{
    float m_RefreshInterval{ 0.25f }; // seconds between text updates; also the FPS averaging window
    float m_Accumulated{ 0.0f };      // unscaled time gathered since the last update
    int   m_Frames{ 0 };              // frames counted since the last update
};

}
