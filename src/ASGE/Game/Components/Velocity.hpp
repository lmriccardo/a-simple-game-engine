#pragma once

namespace asge::game::components
{

/**
 * @brief Linear velocity — rate of change of position, in units per second.
 */
struct Velocity
{
    float m_DX{0.0f}; // change in X per second
    float m_DY{0.0f}; // change in Y per second
};

}