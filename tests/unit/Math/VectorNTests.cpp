#include <ASGE/Core/Math/VectorN.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <stdexcept>
#include <type_traits>

namespace asge::math
{

template<_internal::Numeric T>
class TestVec3 final : public VecN<3, T, TestVec3<T>>
{
public:
    using base = VecN<3, T, TestVec3<T>>;
    using base::VecN;
    using typename base::value_type;

    TestVec3(base const& inBase) : base(inBase) {}

    using base::operator+;
    using base::operator-;
    using base::operator*;
    using base::operator/;
    using base::operator+=;
    using base::operator-=;
    using base::operator*=;
    using base::operator/=;
};

namespace _internal
{

template<Numeric T>
struct rebind_derived_trait<TestVec3<T>>
{
    template<Numeric U>
    using type = TestVec3<U>;
};

}

}

namespace
{

using asge::math::TestVec3;

TEST(VectorN, SizeAndEmptyReflectTemplateExtent)
{
    TestVec3<int> vector;

    EXPECT_EQ(vector.Size(), 3U);
    EXPECT_FALSE(vector.Empty());
}

TEST(VectorN, FillConstructsEveryElementWithSameValue)
{
    TestVec3<int> vector(7);

    for (std::size_t i = 0; i < vector.Size(); ++i)
    {
        EXPECT_EQ(vector[i], 7);
    }
}

TEST(VectorN, InitializerListPadsMissingValuesWithZero)
{
    TestVec3<int> vector{1, 2};

    EXPECT_EQ(vector[0], 1);
    EXPECT_EQ(vector[1], 2);
    EXPECT_EQ(vector[2], 0);
}

TEST(VectorN, InitializerListTruncatesExtraValues)
{
    TestVec3<int> vector{1, 2, 3, 4, 5};

    EXPECT_EQ(vector[0], 1);
    EXPECT_EQ(vector[1], 2);
    EXPECT_EQ(vector[2], 3);
}

TEST(VectorN, FillAndZerosUpdateEveryElement)
{
    TestVec3<int> vector{1, 2, 3};

    vector.Fill(9);
    EXPECT_EQ(vector[0], 9);
    EXPECT_EQ(vector[1], 9);
    EXPECT_EQ(vector[2], 9);

    vector.Zeros();
    EXPECT_EQ(vector[0], 0);
    EXPECT_EQ(vector[1], 0);
    EXPECT_EQ(vector[2], 0);
}

TEST(VectorN, IndexAccessAllowsMutationAndThrowsOutOfRange)
{
    TestVec3<int> vector{1, 2, 3};
    vector[1] = 5;

    EXPECT_EQ(vector[1], 5);
    EXPECT_THROW(static_cast<void>(vector[3]), std::out_of_range);
}

TEST(VectorN, DataReturnsInternalContiguousStorage)
{
    TestVec3<int> vector{1, 2, 3};
    int* data = vector.Data();

    data[1] = 20;

    EXPECT_EQ(vector[0], 1);
    EXPECT_EQ(vector[1], 20);
    EXPECT_EQ(vector[2], 3);
}

TEST(VectorN, VectorArithmeticReturnsElementWiseResults)
{
    TestVec3<int> lhs{8, 10, 12};
    TestVec3<int> rhs{2, 5, 3};

    auto sum = lhs + rhs;
    auto difference = lhs - rhs;
    auto product = lhs * rhs;
    auto quotient = lhs / rhs;

    static_assert(std::is_same_v<decltype(sum), TestVec3<int>>);
    EXPECT_EQ(sum[0], 10);
    EXPECT_EQ(sum[1], 15);
    EXPECT_EQ(sum[2], 15);

    EXPECT_EQ(difference[0], 6);
    EXPECT_EQ(difference[1], 5);
    EXPECT_EQ(difference[2], 9);

    EXPECT_EQ(product[0], 16);
    EXPECT_EQ(product[1], 50);
    EXPECT_EQ(product[2], 36);

    EXPECT_EQ(quotient[0], 4);
    EXPECT_EQ(quotient[1], 2);
    EXPECT_EQ(quotient[2], 4);
}

TEST(VectorN, MixedNumericVectorArithmeticUsesCommonType)
{
    TestVec3<int> ints{1, 2, 3};
    TestVec3<double> doubles{0.5, 1.25, 2.5};

    auto result = ints + doubles;

    static_assert(std::is_same_v<decltype(result), TestVec3<double>>);
    EXPECT_DOUBLE_EQ(result[0], 1.5);
    EXPECT_DOUBLE_EQ(result[1], 3.25);
    EXPECT_DOUBLE_EQ(result[2], 5.5);
}

TEST(VectorN, ScalarArithmeticReturnsElementWiseResults)
{
    TestVec3<int> vector{4, 8, 12};

    auto multiplied = vector * 2;
    auto leftMultiplied = 2 * vector;
    auto divided = vector / 2;

    EXPECT_EQ(multiplied[0], 8);
    EXPECT_EQ(multiplied[1], 16);
    EXPECT_EQ(multiplied[2], 24);

    EXPECT_EQ(leftMultiplied[0], 8);
    EXPECT_EQ(leftMultiplied[1], 16);
    EXPECT_EQ(leftMultiplied[2], 24);

    EXPECT_EQ(divided[0], 2);
    EXPECT_EQ(divided[1], 4);
    EXPECT_EQ(divided[2], 6);
}

TEST(VectorN, CompoundVectorArithmeticMutatesInPlace)
{
    TestVec3<int> vector{8, 10, 12};

    vector += TestVec3<int>{1, 2, 3};
    EXPECT_EQ(vector[0], 9);
    EXPECT_EQ(vector[1], 12);
    EXPECT_EQ(vector[2], 15);

    vector -= TestVec3<int>{4, 2, 5};
    EXPECT_EQ(vector[0], 5);
    EXPECT_EQ(vector[1], 10);
    EXPECT_EQ(vector[2], 10);
}

}
