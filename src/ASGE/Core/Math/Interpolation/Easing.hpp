#pragma once

namespace asge::math
{

/** @brief The easing curves a Tween can follow: how progress in [0, 1] is reshaped over time. */
enum class Easing
{
    Linear,    // constant speed
    InQuad,    // starts slow, speeds up
    OutQuad,   // starts fast, slows down
    InOutQuad, // slow at both ends, fast in the middle
    OutCubic,  // like OutQuad, with a stronger slow-down
};

/** @brief Maps inT (clamped to [0, 1]) through inEasing, so 0 stays 0 and 1 stays 1. */
float Ease( Easing inEasing, float inT ) noexcept;

}
