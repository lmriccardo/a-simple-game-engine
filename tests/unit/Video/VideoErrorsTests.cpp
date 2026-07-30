#include <ASGE/Core/Errors.hpp>

#include <gtest/gtest.h>

namespace
{

using asge::errors::VideoError;

TEST(VideoErrorsTest, RegistersAsgeVideoErrorCategory)
{
    auto const code = make_error_code(VideoError::WindowCreationFailed);
    EXPECT_STREQ(code.category().name(), "asge.video");
}

TEST(VideoErrorsTest, DistinctErrorsCompareUnequal)
{
    EXPECT_NE(
        make_error_code(VideoError::SubsystemInitFailed),
        make_error_code(VideoError::WindowCreationFailed));
}

TEST(VideoErrorsTest, ToErrorStringIsNonEmptyForEveryEnumerator)
{
    EXPECT_FALSE(make_error_code(VideoError::SubsystemInitFailed).message().empty());
    EXPECT_FALSE(make_error_code(VideoError::WindowCreationFailed).message().empty());
    EXPECT_FALSE(make_error_code(VideoError::RendererCreationFailed).message().empty());
}

} // namespace
