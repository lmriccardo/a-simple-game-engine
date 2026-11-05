#pragma once

namespace asge::math
{

/**
 * @brief The easing curves a Tween can follow.
 *
 * Each one reshapes progress in [0, 1] over time; all of them map 0 to 0
 * and 1 to 1.
 */
enum class Easing
{
    Linear,    // constant speed
    InQuad,    // starts slow, speeds up
    OutQuad,   // starts fast, slows down
    InOutQuad, // slow at both ends, fast in the middle
    OutCubic,  // like OutQuad, with a stronger slow-down
};

/**
 * @brief Maps inT (clamped to [0, 1]) through inEasing.
 *
 * 0 stays 0 and 1 stays 1, so the result can be used directly as an
 * interpolation factor. An inT outside [0, 1] is clamped first.
 */
float Ease( Easing inEasing, float inT ) noexcept;

}
