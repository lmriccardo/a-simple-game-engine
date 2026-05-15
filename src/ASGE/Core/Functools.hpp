#pragma once

#include <type_traits>
#include <tuple>
#include <functional>

namespace asge::functools
{

namespace _internal
{

// Trait to extract various type of informations based on the callable
template<typename T>
struct function_trait;

// ─── Free function ───────────────────────────────────────────
template<typename RetType, typename... Args>
struct function_trait<RetType (*)(Args...)> 
{
    using return_type = RetType;
    using class_type  = void;
    using args_type   = std::tuple<Args...>;
};

// ─── Member function (all cv/ref qualifiers) ─────────────────
#define MEMBER_FUNCTION_TRAIT(CV, REF)                              \
template<typename RetType, typename ClassType, typename... Args>    \
struct function_trait<RetType(ClassType::*)(Args...) CV REF> {      \
    using return_type = RetType;                                    \
    using class_type  = ClassType;                                  \
    using args_type   = std::tuple<Args...>;                        \
};

MEMBER_FUNCTION_TRAIT(,)
MEMBER_FUNCTION_TRAIT(const,)
MEMBER_FUNCTION_TRAIT(volatile,)
MEMBER_FUNCTION_TRAIT(const volatile,)
MEMBER_FUNCTION_TRAIT(, &)
MEMBER_FUNCTION_TRAIT(const, &)
MEMBER_FUNCTION_TRAIT(, &&)
MEMBER_FUNCTION_TRAIT(const, &&)
MEMBER_FUNCTION_TRAIT(, noexcept)
MEMBER_FUNCTION_TRAIT(const, noexcept)

#undef MEMBER_FUNCTION_TRAIT

// ─── Functor / Lambda (delegates to operator()) ──────────────
template<typename T>
struct function_trait : function_trait<decltype(&T::operator())>
{
    using class_type = void;
};

// ─── std::function ───────────────────────────────────────────
template<typename Ret, typename... Args>
struct function_trait<std::function<Ret(Args...)>> 
{
    using return_type = Ret;
    using class_type  = void;
    using args_type   = std::tuple<Args...>;
};

// ─── Convenience helpers ─────────────────────────────────────
template<typename T>
using return_type_t = typename function_trait<T>::return_type;

template<typename T>
using class_type_t  = typename function_trait<T>::class_type;

template<typename T>
using args_type_t   = typename function_trait<T>::args_type;

template<typename T, std::size_t N>
using arg_t = std::tuple_element_t<N, args_type_t<T>>;

template<typename T>
inline constexpr std::size_t arity_v = std::tuple_size_v<args_type_t<T>>;

template<typename Callable, typename T>
concept MemberFunctionOf =
    std::is_member_function_pointer_v<std::remove_reference_t<Callable>> &&
    std::is_base_of_v<
        class_type_t<std::remove_reference_t<Callable>>,
        std::remove_reference_t<T>
    >;

}

template<typename _Callable, typename... _Args>
class Callable
{
protected:
    std::decay_t<_Callable>            m_Func;
    std::tuple<std::decay_t<_Args>...> m_Args;

    // Take the return type from function signature
    using return_type = typename _internal::return_type_t<std::decay_t<_Callable>>;
public:
    Callable( _Callable&& inFn, _Args&&... inArgs )
    : m_Func( std::forward<_Callable>( inFn ) )
    , m_Args( std::forward<_Args>(inArgs)... )
    {}

    virtual ~Callable() = default;

    Callable( Callable&& )            = default;
    Callable& operator=( Callable&& ) = default;

    virtual return_type Call()
    {
        return std::apply( m_Func, m_Args );
    }

    return_type operator()()
    {
        return Call();
    }
};

}