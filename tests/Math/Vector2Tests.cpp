#include <ASGE/Math/Vector2.hpp>

#include <gtest/gtest.h>

#include <type_traits>

namespace
{

using asge::math::Double2;
using asge::math::Float2;
using asge::math::Int2;
using asge::math::Vec2;

TEST(Vector2, ConstructsFromInitializerListAndExposesNamedComponents)
{
    Float2 vector{1.5F, -2.25F};

    EXPECT_FLOAT_EQ(vector.x(), 1.5F);
    EXPECT_FLOAT_EQ(vector.y(), -2.25F);
}

TEST(Vector2, NamedComponentsAreMutable)
{
    Int2 vector{1, 2};

    vector.x() = 10;
    vector.y() = 20;

    EXPECT_EQ(vector.x(), 10);
    EXPECT_EQ(vector.y(), 20);
}

TEST(Vector2, ZeroFactoryReturnsVec2)
{
    auto zero = Float2::Zero();

    static_assert(std::is_same_v<decltype(zero), Float2>);
    EXPECT_FLOAT_EQ(zero.x(), 0.0F);
    EXPECT_FLOAT_EQ(zero.y(), 0.0F);
}

TEST(Vector2, VectorArithmeticPreservesVec2Result)
{
    Int2 lhs{8, 12};
    Int2 rhs{2, 3};

    auto sum = lhs + rhs;
    auto difference = lhs - rhs;
    auto product = lhs * rhs;
    auto quotient = lhs / rhs;

    static_assert(std::is_same_v<decltype(sum), Int2>);
    EXPECT_EQ(sum.x(), 10);
    EXPECT_EQ(sum.y(), 15);

    EXPECT_EQ(difference.x(), 6);
    EXPECT_EQ(difference.y(), 9);

    EXPECT_EQ(product.x(), 16);
    EXPECT_EQ(product.y(), 36);

    EXPECT_EQ(quotient.x(), 4);
    EXPECT_EQ(quotient.y(), 4);
}

TEST(Vector2, MixedNumericArithmeticRebindsToCommonVec2Type)
{
    Int2 ints{1, 2};
    Double2 doubles{0.5, 1.25};

    auto result = ints + doubles;

    static_assert(std::is_same_v<decltype(result), Vec2<double>>);
    EXPECT_DOUBLE_EQ(result.x(), 1.5);
    EXPECT_DOUBLE_EQ(result.y(), 3.25);
}

TEST(Vector2, ScalarArithmeticPreservesNamedComponentAccess)
{
    Int2 vector{4, 8};

    auto multiplied = vector * 2;
    auto leftMultiplied = 2 * vector;
    auto divided = vector / 2;

    static_assert(std::is_same_v<decltype(multiplied), Int2>);
    EXPECT_EQ(multiplied.x(), 8);
    EXPECT_EQ(multiplied.y(), 16);

    EXPECT_EQ(leftMultiplied.x(), 8);
    EXPECT_EQ(leftMultiplied.y(), 16);

    EXPECT_EQ(divided.x(), 2);
    EXPECT_EQ(divided.y(), 4);
}

TEST(Vector2, CompoundVectorArithmeticReturnsMutableVec2Reference)
{
    Int2 vector{2, 4};

    auto& afterAdd = (vector += Int2{3, 6});
    static_assert(std::is_same_v<decltype(afterAdd), Int2&>);
    EXPECT_EQ(vector.x(), 5);
    EXPECT_EQ(vector.y(), 10);

    vector -= Int2{1, 2};
    EXPECT_EQ(vector.x(), 4);
    EXPECT_EQ(vector.y(), 8);
}

}
