#include <ASGE/Core/Strings.hpp>

#include <gtest/gtest.h>

namespace
{

using namespace asge::str;

// ─── ToString / FromString (TextAlign) ─────────────────────────────────────

TEST(TextAlignToStringTest, EachKnownValueMapsToItsLowercaseName)
{
    EXPECT_EQ(ToString(TextAlign::Left), "left");
    EXPECT_EQ(ToString(TextAlign::Center), "center");
    EXPECT_EQ(ToString(TextAlign::Right), "right");
}

TEST(TextAlignToStringTest, NoneMapsToNone)
{
    EXPECT_EQ(ToString(TextAlign::None), "none");
}

TEST(TextAlignFromStringTest, EachKnownNameMapsBackToItsValue)
{
    EXPECT_EQ(FromString("left"), TextAlign::Left);
    EXPECT_EQ(FromString("center"), TextAlign::Center);
    EXPECT_EQ(FromString("right"), TextAlign::Right);
}

TEST(TextAlignFromStringTest, UnrecognizedValueMapsToNone)
{
    EXPECT_EQ(FromString("diagonal"), TextAlign::None);
    EXPECT_EQ(FromString(""), TextAlign::None);
}

TEST(TextAlignRoundTripTest, ToStringThenFromStringRecoversEveryKnownValue)
{
    for ( auto align : { TextAlign::None, TextAlign::Left, TextAlign::Center, TextAlign::Right } )
    {
        EXPECT_EQ(FromString(ToString(align)), align);
    }
}

// ─── CodePointLength ────────────────────────────────────────────────────────

TEST(CodePointLengthTest, EmptyString_ReturnsZero)
{
    EXPECT_EQ(CodePointLength(""), 0u);
}

TEST(CodePointLengthTest, PlainAscii_MatchesByteLength)
{
    EXPECT_EQ(CodePointLength("hello"), 5u);
}

TEST(CodePointLengthTest, TwoByteUtf8Sequence_CountsAsOneCodePoint)
{
    // U+00E9 (é) encodes as two bytes (0xC3 0xA9).
    EXPECT_EQ(CodePointLength("\xC3\xA9"), 1u);
}

TEST(CodePointLengthTest, ThreeByteUtf8Sequence_CountsAsOneCodePoint)
{
    // U+2764 (heavy black heart) encodes as three bytes.
    EXPECT_EQ(CodePointLength("\xE2\x9D\xA4"), 1u);
}

TEST(CodePointLengthTest, FourByteUtf8Sequence_CountsAsOneCodePoint)
{
    // U+1F600 (grinning face emoji) encodes as four bytes.
    EXPECT_EQ(CodePointLength("\xF0\x9F\x98\x80"), 1u);
}

TEST(CodePointLengthTest, MixedAsciiAndMultiByte_CountsEachSequenceOnce)
{
    // "café" -- 3 ASCII bytes + one 2-byte sequence for the accented e.
    EXPECT_EQ(CodePointLength("caf\xC3\xA9"), 4u);
}

TEST(CodePointLengthTest, MalformedLeadingByte_CountsItAsOneCodePointInsteadOfLoopingForever)
{
    // 0xFF is not a valid UTF-8 leading byte in any of the four patterns
    // the implementation recognizes; it must still advance by 1 rather
    // than infinite-loop trying to consume it as a multi-byte sequence.
    EXPECT_EQ(CodePointLength("\xFF"), 1u);
}

// ─── Justify ────────────────────────────────────────────────────────────────

TEST(JustifyTest, LeftAlign_PadsOnTheRight)
{
    EXPECT_EQ(Justify("ab", TextAlign::Left, 5), "ab   ");
}

TEST(JustifyTest, RightAlign_PadsOnTheLeft)
{
    EXPECT_EQ(Justify("ab", TextAlign::Right, 5), "   ab");
}

TEST(JustifyTest, CenterAlign_SplitsPaddingWithExtraSpaceOnTheRight)
{
    // width 5 - length 2 = 3 total padding -> left=1, right=2.
    EXPECT_EQ(Justify("ab", TextAlign::Center, 5), " ab  ");
}

TEST(JustifyTest, CenterAlign_EvenPaddingSplitsExactlyInHalf)
{
    // width 6 - length 2 = 4 total padding -> left=2, right=2.
    EXPECT_EQ(Justify("ab", TextAlign::Center, 6), "  ab  ");
}

TEST(JustifyTest, NoneAlign_ReturnsInputUnchangedEvenWhenNarrowerThanWidth)
{
    EXPECT_EQ(Justify("ab", TextAlign::None, 10), "ab");
}

TEST(JustifyTest, InputAlreadyAtWidth_ReturnsItUnchanged)
{
    EXPECT_EQ(Justify("abcde", TextAlign::Left, 5), "abcde");
}

TEST(JustifyTest, InputWiderThanTargetWidth_ReturnsItUnchangedRatherThanTruncating)
{
    EXPECT_EQ(Justify("abcdefgh", TextAlign::Left, 3), "abcdefgh");
}

TEST(JustifyTest, WidthCountsCodePointsNotBytes)
{
    // "é" is 1 code point but 2 bytes -- padding math must use the former,
    // or this would under-pad relative to a caller measuring in code points.
    EXPECT_EQ(Justify("\xC3\xA9", TextAlign::Left, 3), "\xC3\xA9  ");
}

}
