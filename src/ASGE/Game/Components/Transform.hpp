#pragma once

namespace asge::game::components
{

/**
 * @brief 2D spatial transform — position, rotation, and scale.
 */
struct Transform
{
    float m_X{0.0f};
    float m_Y{0.0f};
    float m_Rotation{0.0f}; // radians
    float m_ScaleX{1.0f};
    float m_ScaleY{1.0f};
};

}