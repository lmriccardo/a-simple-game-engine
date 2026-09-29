#include <ASGE/Game/Components/UI/UISlider.hpp>

#include <gtest/gtest.h>

namespace
{

using asge::game::components::GetStep;
using asge::game::components::GetThumbPosition;
using asge::game::components::UISlider;
using asge::math::Rect;

// ─── GetStep ────────────────────────────────────────────────────────────────

TEST(UISliderTest, GetStep_IsValueRangePerPixelOfWidth)
{
    UISlider slider;
    slider.m_Min = 10.0f;
    slider.m_Max = 20.0f;

    EXPECT_FLOAT_EQ(GetStep(slider, 100.0f), 0.1f);
}

// ─── GetThumbPosition ───────────────────────────────────────────────────────

TEST(UISliderTest, GetThumbPosition_DefaultRangeMidValue_IsHalfwayAlongWidthAndVerticallyCentered)
{
    UISlider slider;
    slider.m_Value = 0.5f;

    auto const pos = GetThumbPosition(slider, Rect{ 100.0f, 40.0f, 200.0f, 20.0f });

    EXPECT_FLOAT_EQ(pos.x(), 200.0f);
    EXPECT_FLOAT_EQ(pos.y(), 50.0f);
}

TEST(UISliderTest, GetThumbPosition_OffsetRange_MapsRelativeToMin)
{
    UISlider slider;
    slider.m_Min = 10.0f;
    slider.m_Max = 20.0f;
    slider.m_Value = 12.5f; // a quarter of the way

    auto const pos = GetThumbPosition(slider, Rect{ 0.0f, 0.0f, 100.0f, 10.0f });

    EXPECT_FLOAT_EQ(pos.x(), 25.0f);
}

TEST(UISliderTest, GetThumbPosition_ValueAtMinAndMax_LandsOnTheRectEdges)
{
    UISlider slider;
    Rect const rect{ 100.0f, 0.0f, 200.0f, 10.0f };

    slider.m_Value = slider.m_Min;
    EXPECT_FLOAT_EQ(GetThumbPosition(slider, rect).x(), 100.0f);

    slider.m_Value = slider.m_Max;
    EXPECT_FLOAT_EQ(GetThumbPosition(slider, rect).x(), 300.0f);
}

TEST(UISliderTest, GetThumbPosition_ValueOutsideRange_ClampsToTheRectEdges)
{
    UISlider slider;
    Rect const rect{ 100.0f, 0.0f, 200.0f, 10.0f };

    slider.m_Value = -5.0f;
    EXPECT_FLOAT_EQ(GetThumbPosition(slider, rect).x(), 100.0f);

    slider.m_Value = 5.0f;
    EXPECT_FLOAT_EQ(GetThumbPosition(slider, rect).x(), 300.0f);
}

TEST(UISliderTest, GetThumbPosition_EmptyRange_PinsThumbToTheLeftEdge)
{
    UISlider slider;
    slider.m_Min = 3.0f;
    slider.m_Max = 3.0f;
    slider.m_Value = 3.0f;

    auto const pos = GetThumbPosition(slider, Rect{ 100.0f, 0.0f, 200.0f, 10.0f });

    EXPECT_FLOAT_EQ(pos.x(), 100.0f);
}

}
