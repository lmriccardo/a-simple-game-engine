#include <ASGE/Core/Functools.hpp>

#include <gtest/gtest.h>

#include <functional>
#include <string>
#include <type_traits>
#include <tuple>
#include <variant>

namespace
{

using namespace asge::functools;
using namespace asge::functools::_internal;

// ─── Helper callables ─────────────────────────────────────────────────────────

void   voidFreeFunc(int) {}
int    addFreeFunc(int a, int b) { return a + b; }
double multiArgFunc(int a, double b, float c) { return a + b + c; }

struct MyClass
{
    int value{42};
    int         memberFunc(int x)       { return value + x; }
    int         constMemberFunc(int x) const { return value + x; }
};

// ─── function_trait: free functions ──────────────────────────────────────────

TEST(FunctoolsTest, FunctionTrait_FreeFuncReturnType)
{
    static_assert(std::is_same_v<return_type_t<decltype(&addFreeFunc)>, int>);
    SUCCEED();
}

TEST(FunctoolsTest, FunctionTrait_FreeFuncVoidReturnType)
{
    static_assert(std::is_same_v<return_type_t<decltype(&voidFreeFunc)>, void>);
    SUCCEED();
}

TEST(FunctoolsTest, FunctionTrait_FreeFuncClassTypeIsVoid)
{
    static_assert(std::is_same_v<class_type_t<decltype(&addFreeFunc)>, void>);
    SUCCEED();
}

TEST(FunctoolsTest, FunctionTrait_FreeFuncArgsType)
{
    using expected = std::tuple<int, int>;
    static_assert(std::is_same_v<args_type_t<decltype(&addFreeFunc)>, expected>);
    SUCCEED();
}

TEST(FunctoolsTest, FunctionTrait_FreeFuncArity)
{
    static_assert(arity_v<decltype(&addFreeFunc)> == 2);
    SUCCEED();
}

TEST(FunctoolsTest, FunctionTrait_FreeFuncArityZero)
{
    auto fn = []() {};
    static_assert(arity_v<decltype(fn)> == 0);
    SUCCEED();
}

TEST(FunctoolsTest, FunctionTrait_FreeFuncArgAtIndex)
{
    static_assert(std::is_same_v<arg_t<decltype(&addFreeFunc), 0>, int>);
    static_assert(std::is_same_v<arg_t<decltype(&addFreeFunc), 1>, int>);
    SUCCEED();
}

TEST(FunctoolsTest, FunctionTrait_MultiArgFuncArgTypes)
{
    static_assert(std::is_same_v<arg_t<decltype(&multiArgFunc), 0>, int>);
    static_assert(std::is_same_v<arg_t<decltype(&multiArgFunc), 1>, double>);
    static_assert(std::is_same_v<arg_t<decltype(&multiArgFunc), 2>, float>);
    static_assert(arity_v<decltype(&multiArgFunc)> == 3);
    SUCCEED();
}

// ─── function_trait: member functions ────────────────────────────────────────

TEST(FunctoolsTest, FunctionTrait_MemberFuncReturnType)
{
    static_assert(std::is_same_v<return_type_t<decltype(&MyClass::memberFunc)>, int>);
    SUCCEED();
}

TEST(FunctoolsTest, FunctionTrait_MemberFuncClassType)
{
    static_assert(std::is_same_v<class_type_t<decltype(&MyClass::memberFunc)>, MyClass>);
    SUCCEED();
}

TEST(FunctoolsTest, FunctionTrait_MemberFuncArity)
{
    static_assert(arity_v<decltype(&MyClass::memberFunc)> == 1);
    SUCCEED();
}

TEST(FunctoolsTest, FunctionTrait_MemberFuncArgAtIndex)
{
    static_assert(std::is_same_v<arg_t<decltype(&MyClass::memberFunc), 0>, int>);
    SUCCEED();
}

TEST(FunctoolsTest, FunctionTrait_ConstMemberFuncReturnType)
{
    static_assert(std::is_same_v<return_type_t<decltype(&MyClass::constMemberFunc)>, int>);
    SUCCEED();
}

TEST(FunctoolsTest, FunctionTrait_ConstMemberFuncClassType)
{
    static_assert(std::is_same_v<class_type_t<decltype(&MyClass::constMemberFunc)>, MyClass>);
    SUCCEED();
}

// ─── function_trait: lambdas ──────────────────────────────────────────────────

TEST(FunctoolsTest, FunctionTrait_LambdaReturnType)
{
    auto lambda = [](int a, int b) { return a + b; };
    static_assert(std::is_same_v<return_type_t<decltype(lambda)>, int>);
    SUCCEED();
}

TEST(FunctoolsTest, FunctionTrait_LambdaVoidReturnType)
{
    auto lambda = []() {};
    static_assert(std::is_same_v<return_type_t<decltype(lambda)>, void>);
    SUCCEED();
}

TEST(FunctoolsTest, FunctionTrait_LambdaClassTypeIsVoid)
{
    auto lambda = [](int) { return 0; };
    static_assert(std::is_same_v<class_type_t<decltype(lambda)>, void>);
    SUCCEED();
}

TEST(FunctoolsTest, FunctionTrait_LambdaArity)
{
    auto lambda = [](int, double, float) {};
    static_assert(arity_v<decltype(lambda)> == 3);
    SUCCEED();
}

TEST(FunctoolsTest, FunctionTrait_LambdaArgAtIndex)
{
    auto lambda = [](int, std::string) { return 0; };
    static_assert(std::is_same_v<arg_t<decltype(lambda), 0>, int>);
    static_assert(std::is_same_v<arg_t<decltype(lambda), 1>, std::string>);
    SUCCEED();
}

// ─── function_trait: std::function ───────────────────────────────────────────

TEST(FunctoolsTest, FunctionTrait_StdFunctionReturnType)
{
    using Fn = std::function<double(int, float)>;
    static_assert(std::is_same_v<return_type_t<Fn>, double>);
    SUCCEED();
}

TEST(FunctoolsTest, FunctionTrait_StdFunctionClassTypeIsVoid)
{
    using Fn = std::function<int(int)>;
    static_assert(std::is_same_v<class_type_t<Fn>, void>);
    SUCCEED();
}

TEST(FunctoolsTest, FunctionTrait_StdFunctionArity)
{
    using Fn = std::function<void(int, int, int)>;
    static_assert(arity_v<Fn> == 3);
    SUCCEED();
}

TEST(FunctoolsTest, FunctionTrait_StdFunctionArgsType)
{
    using Fn       = std::function<void(int, double)>;
    using expected = std::tuple<int, double>;
    static_assert(std::is_same_v<args_type_t<Fn>, expected>);
    SUCCEED();
}

// ─── MemberFunctionOf concept ─────────────────────────────────────────────────

struct Base          { void baseMethod() {} };
struct DerivedClass  : Base { void derivedMethod() {} };
struct Unrelated     { void method() {} };

TEST(FunctoolsTest, MemberFunctionOf_TrueForSameClass)
{
    static_assert(MemberFunctionOf<decltype(&MyClass::memberFunc), MyClass>);
    SUCCEED();
}

TEST(FunctoolsTest, MemberFunctionOf_TrueForDerivedOfBaseMethod)
{
    // is_base_of<Base, DerivedClass> == true, so a Base member ptr works on DerivedClass objects
    static_assert(MemberFunctionOf<decltype(&Base::baseMethod), DerivedClass>);
    SUCCEED();
}

TEST(FunctoolsTest, MemberFunctionOf_FalseForFreeFunction)
{
    static_assert(!MemberFunctionOf<decltype(&addFreeFunc), MyClass>);
    SUCCEED();
}

TEST(FunctoolsTest, MemberFunctionOf_FalseForUnrelatedClass)
{
    static_assert(!MemberFunctionOf<decltype(&MyClass::memberFunc), Unrelated>);
    SUCCEED();
}

TEST(FunctoolsTest, MemberFunctionOf_FalseForLambda)
{
    auto lambda = []() {};
    static_assert(!MemberFunctionOf<decltype(lambda), MyClass>);
    SUCCEED();
}

// ─── Callable: invocation via operator()() ────────────────────────────────────

TEST(FunctoolsTest, Callable_NoArgsVoidLambda)
{
    bool called = false;
    Callable c([&called]() { called = true; });
    c();
    EXPECT_TRUE(called);
}

TEST(FunctoolsTest, Callable_StoredArgPassedToFunction)
{
    int result = 0;
    Callable c([&result](int x) { result = x; }, 99);
    c();
    EXPECT_EQ(result, 99);
}

TEST(FunctoolsTest, Callable_MultipleStoredArgs)
{
    int result = 0;
    Callable c([&result](int a, int b, int cc) { result = a + b + cc; }, 1, 2, 3);
    c();
    EXPECT_EQ(result, 6);
}

TEST(FunctoolsTest, Callable_FreeFunctionStoredArgs)
{
    int result = 0;
    Callable c([&result](int x) { result = x * 2; }, 5);
    c();
    EXPECT_EQ(result, 10);
}

TEST(FunctoolsTest, Callable_LambdaWithCapture)
{
    int multiplier = 4;
    int result     = 0;
    Callable c([&result, multiplier](int x) { result = multiplier * x; }, 3);
    c();
    EXPECT_EQ(result, 12);
}

TEST(FunctoolsTest, Callable_CanBeCalledMultipleTimes)
{
    int count = 0;
    Callable c([&count]() { ++count; });
    c();
    c();
    c();
    EXPECT_EQ(count, 3);
}

TEST(FunctoolsTest, Callable_StdFunctionWrapper)
{
    int result = 0;
    std::function<void(int)> fn = [&result](int x) { result = x + 1; };
    Callable c(std::move(fn), 41);
    c();
    EXPECT_EQ(result, 42);
}

TEST(FunctoolsTest, Callable_MemberFuncViaLambda)
{
    MyClass obj;
    int result = 0;
    Callable c([&obj, &result](int x) { result = obj.memberFunc(x); }, 8);
    c();
    EXPECT_EQ(result, 50); // 42 + 8
}

TEST(FunctoolsTest, Callable_ConstMemberFuncViaLambda)
{
    const MyClass obj;
    int result = 0;
    Callable c([&obj, &result](int x) { result = obj.constMemberFunc(x); }, 3);
    c();
    EXPECT_EQ(result, 45); // 42 + 3
}

// ─── Callable: operator()() return value ─────────────────────────────────────

TEST(FunctoolsTest, Callable_ReturnsIntDirectly)
{
    Callable c([](int x) { return x * 2; }, 21);
    EXPECT_EQ(c(), 42);
}

TEST(FunctoolsTest, Callable_ReturnsDoubleDirectly)
{
    Callable c([](double a, double b) { return a * b; }, 1.5, 4.0);
    EXPECT_DOUBLE_EQ(c(), 6.0);
}

TEST(FunctoolsTest, Callable_ReturnsStringDirectly)
{
    Callable c([]() -> std::string { return "hello"; });
    EXPECT_EQ(c(), "hello");
}

TEST(FunctoolsTest, Callable_ReturnsResultOfStoredArgsComputation)
{
    Callable c([](int a, int b, int cc) { return a + b + cc; }, 10, 20, 12);
    EXPECT_EQ(c(), 42);
}

TEST(FunctoolsTest, Callable_ReturnValueConsistentAcrossMultipleCalls)
{
    Callable c([](int x) { return x; }, 7);
    EXPECT_EQ(c(), 7);
    EXPECT_EQ(c(), 7);
    EXPECT_EQ(c(), 7);
}

TEST(FunctoolsTest, Callable_ReturnsMemberFuncResult)
{
    MyClass obj;
    Callable c([&obj](int x) { return obj.memberFunc(x); }, 8);
    EXPECT_EQ(c(), 50); // 42 + 8
}

// ─── ReduceTuple ───────────────────────────────────────────────────────────────

using FoldAcc = std::variant<int, double, float>;

template<typename T> struct is_variant : std::false_type {};
template<typename... Ts> struct is_variant<std::variant<Ts...>> : std::true_type {};

// Extracts the numeric value from a ReduceTuple fold-type value (the
// accumulator or an element — both share the same type): a plain
// arithmetic value for a homogeneous tuple, or a std::variant alternative
// for a heterogeneous one.
double AccAsDouble(auto const& acc)
{
    using Acc = std::decay_t<decltype(acc)>;
    if constexpr (is_variant<Acc>::value)
        return std::visit([](auto x) { return static_cast<double>(x); }, acc);
    else
        return static_cast<double>(acc);
}

// Encodes fold order as a base-10 number: each step appends the next
// element's digit to the right of the running accumulator. A right-to-left
// fold over (1, 2, 3) therefore reads "3, then 2, then 1" -> 321. Returns a
// plain int (rather than the accumulator's own type) since that int
// converts unambiguously into any of this file's tested accumulators.
auto FoldOrderOp = []<typename Acc>(Acc const& acc, Acc const& elem)
{
    return static_cast<int>(AccAsDouble(acc)) * 10 + static_cast<int>(AccAsDouble(elem));
};

TEST(FunctoolsTest, ReduceTuple_TwoElementsCombinesRightThenLeft)
{
    std::tuple<int, double> t{ 1, 2 };
    // Seed = c1 (2), then combine(2, c0=1) -> 21.
    auto result = ReduceTuple(t, FoldOrderOp);
    ASSERT_TRUE(std::holds_alternative<int>(result));
    EXPECT_EQ(std::get<int>(result), 21);
}

TEST(FunctoolsTest, ReduceTuple_ThreeElementsFoldRightToLeft)
{
    std::tuple<int, double, float> t{ 1, 2.0, 3.0f };
    // Seed = c2 (3), combine(3, c1=2) -> 32, combine(32, c0=1) -> 321.
    auto result = ReduceTuple(t, FoldOrderOp);
    ASSERT_TRUE(std::holds_alternative<int>(result));
    EXPECT_EQ(std::get<int>(result), 321);
}

TEST(FunctoolsTest, ReduceTuple_BaseCaseElementIsUnchanged)
{
    // A BinOp that always keeps the running accumulator makes the result
    // exactly the seed (last element), holding its original alternative type.
    std::tuple<int, double, float> t{ 1, 2.0, 3.0f };
    auto keepAcc = [](FoldAcc const& acc, auto) -> FoldAcc { return acc; };

    auto result = ReduceTuple(t, keepAcc);
    ASSERT_TRUE(std::holds_alternative<float>(result));
    EXPECT_EQ(std::get<float>(result), 3.0f);
}

TEST(FunctoolsTest, ReduceTuple_WinnerCanBeAnyElementType)
{
    auto maxOp = []<typename Acc>(Acc const& acc, Acc const& elem) -> Acc
    {
        return AccAsDouble(acc) >= AccAsDouble(elem) ? acc : elem;
    };

    {
        // Largest value sits at the last (seed) position.
        std::tuple<int, double, float> t{ 1, 2.0, 100.0f };
        auto result = ReduceTuple(t, maxOp);
        ASSERT_TRUE(std::holds_alternative<float>(result));
        EXPECT_EQ(std::get<float>(result), 100.0f);
    }
    {
        // Largest value sits at the first position, so it must survive
        // every combine step back to the front of the tuple.
        std::tuple<int, double, float> t{ 500, 2.0, 3.0f };
        auto result = ReduceTuple(t, maxOp);
        ASSERT_TRUE(std::holds_alternative<int>(result));
        EXPECT_EQ(std::get<int>(result), 500);
    }
    {
        // Largest value sits in the middle.
        std::tuple<int, double, float> t{ 1, 99.0, 2.0f };
        auto result = ReduceTuple(t, maxOp);
        ASSERT_TRUE(std::holds_alternative<double>(result));
        EXPECT_EQ(std::get<double>(result), 99.0);
    }
}

// ─── ReduceTuple: homogeneous tuples ────────────────────────────────────────

TEST(FunctoolsTest, ReduceTuple_HomogeneousTupleResultIsPlainType)
{
    // All elements share type int, so the fold type collapses to plain int
    // instead of std::variant<int,int,int> — no ambiguous conversion needed.
    std::tuple<int, int, int> t{ 1, 2, 3 };
    auto sumOp = [](int acc, int elem) { return acc + elem; };

    auto result = ReduceTuple(t, sumOp);
    static_assert(std::is_same_v<decltype(result), int>);
    EXPECT_EQ(result, 6);
}

TEST(FunctoolsTest, ReduceTuple_HomogeneousTupleFoldsRightToLeft)
{
    // Non-commutative op, so only the documented right-to-left order
    // produces this result: seed=c2(2), combine(2,c1=3)->-1, combine(-1,c0=10)->-11.
    std::tuple<int, int, int> t{ 10, 3, 2 };
    auto subOp = [](int acc, int elem) { return acc - elem; };

    auto result = ReduceTuple(t, subOp);
    static_assert(std::is_same_v<decltype(result), int>);
    EXPECT_EQ(result, -11);
}

// ─── MapTuple ────────────────────────────────────────────────────────────────

TEST(FunctoolsTest, MapTuple_PreservesElementCount)
{
    std::tuple<int, int, int> t{ 1, 2, 3 };
    auto result = MapTuple(t, [](int x) { return x; });
    static_assert(std::tuple_size_v<decltype(result)> == 3);
}

TEST(FunctoolsTest, MapTuple_HomogeneousTupleAppliesCallableToEachElement)
{
    std::tuple<int, int, int> t{ 1, 2, 3 };
    auto result = MapTuple(t, [](int x) { return x * 10; });

    EXPECT_EQ(std::get<0>(result), 10);
    EXPECT_EQ(std::get<1>(result), 20);
    EXPECT_EQ(std::get<2>(result), 30);
}

TEST(FunctoolsTest, MapTuple_HeterogeneousTupleUsesGenericLambda)
{
    std::tuple<int, double, std::string> t{ 1, 2.5, "x" };
    auto toString = [](auto const& x)
    {
        if constexpr (std::is_same_v<std::decay_t<decltype(x)>, std::string>)
            return x;
        else
            return std::to_string(x);
    };

    auto result = MapTuple(t, toString);

    EXPECT_EQ(std::get<0>(result), "1");
    EXPECT_EQ(std::get<1>(result), "2.500000");
    EXPECT_EQ(std::get<2>(result), "x");
}

TEST(FunctoolsTest, MapTuple_ResultTypeCanDifferFromInputType)
{
    std::tuple<int, int> t{ 3, 4 };
    auto result = MapTuple(t, [](int x) { return static_cast<double>(x) / 2.0; });

    static_assert(std::is_same_v<std::tuple_element_t<0, decltype(result)>, double>);
    EXPECT_DOUBLE_EQ(std::get<0>(result), 1.5);
    EXPECT_DOUBLE_EQ(std::get<1>(result), 2.0);
}

TEST(FunctoolsTest, MapTuple_DoesNotMutateSourceTuple)
{
    std::tuple<int, int> t{ 1, 2 };
    auto result = MapTuple(t, [](int x) { return x + 1; });

    EXPECT_EQ(std::get<0>(t), 1);
    EXPECT_EQ(std::get<1>(t), 2);
    EXPECT_EQ(std::get<0>(result), 2);
    EXPECT_EQ(std::get<1>(result), 3);
}

TEST(FunctoolsTest, MapTuple_WorksOnRvalueTuple)
{
    auto result = MapTuple(std::tuple<int, int>{ 5, 6 }, [](int x) { return x * x; });

    EXPECT_EQ(std::get<0>(result), 25);
    EXPECT_EQ(std::get<1>(result), 36);
}

// ─── ConcatTuples ────────────────────────────────────────────────────────────

TEST(FunctoolsTest, ConcatTuples_ResultSizeIsSumOfInputs)
{
    std::tuple<int, int> t1{ 1, 2 };
    std::tuple<double> t2{ 3.0 };
    auto result = ConcatTuples(t1, t2);
    static_assert(std::tuple_size_v<decltype(result)> == 3);
}

TEST(FunctoolsTest, ConcatTuples_PreservesElementOrder)
{
    std::tuple<int, int> t1{ 1, 2 };
    std::tuple<double, std::string> t2{ 3.5, "x" };
    auto result = ConcatTuples(t1, t2);

    EXPECT_EQ(std::get<0>(result), 1);
    EXPECT_EQ(std::get<1>(result), 2);
    EXPECT_EQ(std::get<2>(result), 3.5);
    EXPECT_EQ(std::get<3>(result), "x");
}

TEST(FunctoolsTest, ConcatTuples_FirstTupleEmptyYieldsSecondTuple)
{
    std::tuple<> t1{};
    std::tuple<int, int> t2{ 7, 8 };
    auto result = ConcatTuples(t1, t2);

    static_assert(std::tuple_size_v<decltype(result)> == 2);
    EXPECT_EQ(std::get<0>(result), 7);
    EXPECT_EQ(std::get<1>(result), 8);
}

TEST(FunctoolsTest, ConcatTuples_SecondTupleEmptyYieldsFirstTuple)
{
    std::tuple<int, int> t1{ 7, 8 };
    std::tuple<> t2{};
    auto result = ConcatTuples(t1, t2);

    static_assert(std::tuple_size_v<decltype(result)> == 2);
    EXPECT_EQ(std::get<0>(result), 7);
    EXPECT_EQ(std::get<1>(result), 8);
}

TEST(FunctoolsTest, ConcatTuples_WorksOnRvalueTuples)
{
    auto result = ConcatTuples(std::tuple<int>{ 1 }, std::tuple<int>{ 2 });

    EXPECT_EQ(std::get<0>(result), 1);
    EXPECT_EQ(std::get<1>(result), 2);
}

// ─── PrependToTuple ──────────────────────────────────────────────────────────

TEST(FunctoolsTest, PrependToTuple_ResultSizeIsInputPlusOne)
{
    std::tuple<int, int> t{ 1, 2 };
    auto result = PrependToTuple(0, t);
    static_assert(std::tuple_size_v<decltype(result)> == 3);
}

TEST(FunctoolsTest, PrependToTuple_ValueLandsAtFront)
{
    std::tuple<int, std::string> t{ 2, "b" };
    auto result = PrependToTuple(1, t);

    EXPECT_EQ(std::get<0>(result), 1);
    EXPECT_EQ(std::get<1>(result), 2);
    EXPECT_EQ(std::get<2>(result), "b");
}

TEST(FunctoolsTest, PrependToTuple_OntoEmptyTupleYieldsSingleton)
{
    std::tuple<> t{};
    auto result = PrependToTuple(42, t);

    static_assert(std::tuple_size_v<decltype(result)> == 1);
    EXPECT_EQ(std::get<0>(result), 42);
}

TEST(FunctoolsTest, PrependToTuple_DecaysValueLikeMakeTuple)
{
    std::tuple<int> t{ 1 };
    char const* text = "hi";
    auto result = PrependToTuple(text, t);

    static_assert(std::is_same_v<std::tuple_element_t<0, decltype(result)>, char const*>);
    EXPECT_STREQ(std::get<0>(result), "hi");
}

TEST(FunctoolsTest, PrependToTuple_DoesNotMutateSourceTuple)
{
    std::tuple<int, int> t{ 2, 3 };
    auto result = PrependToTuple(1, t);

    EXPECT_EQ(std::get<0>(t), 2);
    EXPECT_EQ(std::get<1>(t), 3);
    EXPECT_EQ(std::get<0>(result), 1);
}

}
