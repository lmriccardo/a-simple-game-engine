#include <ASGE/Utils/Functools.hpp>

#include <gtest/gtest.h>

#include <functional>
#include <string>
#include <type_traits>

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

}
