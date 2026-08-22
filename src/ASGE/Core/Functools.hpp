#pragma once

#include <type_traits>
#include <tuple>
#include <functional>
#include <memory>
#include "Traits.hpp"
#include "TraitFunctions.hpp"

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

/**
 * Creates a callback wrapper that holds a weak reference to an object and
 * safely invokes a member function on that object if it is still alive.
 * 
 * This function accepts a member function pointer and a shared pointer to an object
 * and returns a callable lambda that, when invoked, attempts to lock the weak pointer
 * to the object. If successful, it calls the member function otherwise do nothing.
 * 
 * @tparam T The class type of the object owning the member function
 * @tparam _Callable Type of the member function pointer
 * 
 * @param func The member function pointer to invoke on the object
 * @param ptr  Shared pointer to the object instance
 */
template<typename T, typename Callable>
requires _internal::MemberFunctionOf<Callable, T>
auto MakeCallback( Callable&& inFunc, std::shared_ptr<T> inPtr )
{
    std::weak_ptr<T> wptr = inPtr;
    return [ wptr, func = std::forward<Callable>(inFunc)](auto&&... args)
    {
        if ( std::shared_ptr<T> sptr = wptr.lock() )
        {
            ( sptr.get()->*func )( std::forward<decltype(args)>(args)... );
        }
    };
}

/**
 * The same MakeCallback with smart pointers, but using raw pointers, which means
 * that the lifetime of the object pointed to must be managed carefully, otherwise
 * segmentation fault would occur.
 */
template<typename T, typename Callable>
requires _internal::MemberFunctionOf<Callable, T>
auto MakeCallback( Callable&& inFunc, T* inPtr )
{
    return [inPtr, func = std::forward<Callable>(inFunc)](auto&&... args)
    {
        ( inPtr->*func )( std::forward<decltype(args)>(args)... );
    };
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

namespace _internal
{

// Recursive right-to-left walk backing ReduceTuple(); see its docs for the
// actual fold order and the reasoning behind the fold-type return.
template<std::size_t Icurr, std::size_t Iend, typename Tuple, typename BinOp>
auto ReduceTupleImpl( Tuple const& t, BinOp&& b )
    -> asge::_internal::traits::tuple_fold_type_t<Tuple>
{
    auto c = asge::_internal::traits::get_element_as_fold_type<Icurr>(t);
    if constexpr ( Icurr + 1 == Iend ) return c;
    else {
        return b( ReduceTupleImpl<Icurr+1,Iend>(t, std::forward<BinOp>(b)), c );
    }
}

}

/**
 * @brief Right-associative fold over a tuple's elements, no separate seed.
 *
 * The last element seeds the accumulator; @c b(accumulator, element) is
 * then applied moving leftward one element at a time. For a 3-tuple
 * (c0, c1, c2) this computes @c b(b(c2, c1), c0).
 *
 * Every value passed to @c b — the accumulator and each element alike —
 * is typed as Tuple's fold type: the shared element type if every element
 * has the same type, or a @c std::variant of the element types otherwise.
 * @c b must return that same type.
 *
 * @tparam Tuple A std::tuple<Ts...> with at least 2 elements.
 * @tparam BinOp Callable invoked as @c b(accumulator, element), both
 *               typed as Tuple's fold type.
 * @param t Tuple to fold; must have tuple_size >= 2.
 * @param b Combines the running accumulator with the next element.
 * @return The final accumulator, typed as Tuple's fold type.
 */
template<typename Tuple, typename BinOp>
requires (std::tuple_size_v<Tuple> >= 2)
auto ReduceTuple( Tuple const& t, BinOp&& b )
{
    constexpr std::size_t Size = std::tuple_size_v<Tuple>;
    return _internal::ReduceTupleImpl<0, Size>(t, std::forward<BinOp>(b));
}

namespace _internal
{

// Invokes c on each already-unpacked element and collects the results.
// c is taken by lvalue here (not forwarded) since it is reused once per
// element rather than consumed by a single call.
template<typename Callable, typename ... Args>
auto MapTupleImplV2( Callable&& c, Args&&... args )
{
    return std::make_tuple( std::invoke(c, std::forward<Args>(args))... );
}

// Unpacks t's elements via the index pack and hands them to MapTupleImplV2.
template<typename Tuple, typename Callable, std::size_t ... Is>
auto MapTupleImpl( Tuple&& t, Callable&& c, std::index_sequence<Is...> )
{
    return MapTupleImplV2( std::forward<Callable>(c),std::get<Is>(std::forward<Tuple>(t))... );
}

}

/**
 * @brief Applies a callable to every element of a tuple, collecting the results.
 *
 * Each element is passed to @c c independently via std::invoke, so @c c may
 * be generic or overloaded when Tuple is heterogeneous. The result tuple's
 * element types are whatever @c c returns for each input, decayed the same
 * way std::make_tuple decays its arguments.
 *
 * @tparam Tuple A tuple-like forwarding reference.
 * @tparam Callable Invoked once per element as @c c(element).
 * @param t Tuple whose elements are transformed.
 * @param c Callable applied to each element.
 * @return A new tuple holding std::invoke(c, element) for every element of t.
 */
template<typename Tuple, typename Callable>
auto MapTuple( Tuple&& t, Callable&& c )
{
    constexpr std::size_t N = std::tuple_size_v<std::remove_reference_t<Tuple>>;
    return _internal::MapTupleImpl(
        std::forward<Tuple>(t), std::forward<Callable>(c),
        std::make_index_sequence<N>{}
    );
}

/**
 * @brief Concatenates two tuples into one, preserving element order.
 *
 * @tparam Tuple1 First tuple-like forwarding reference.
 * @tparam Tuple2 Second tuple-like forwarding reference.
 * @param t1 Tuple contributing the leading elements.
 * @param t2 Tuple contributing the trailing elements.
 * @return A new tuple holding t1's elements followed by t2's elements.
 */
template<typename Tuple1, typename Tuple2>
auto ConcatTuples( Tuple1&& t1, Tuple2&& t2 )
{
    return std::tuple_cat( std::forward<Tuple1>(t1), std::forward<Tuple2>(t2) );
}

/**
 * @brief Returns a new tuple with value inserted at the front of t.
 *
 * @tparam T Type of the value to prepend; decayed as std::make_tuple decays it.
 * @tparam Tuple Tuple-like forwarding reference to prepend onto.
 * @param value Value to place before t's elements.
 * @param t Tuple whose elements follow value.
 * @return A new tuple: (value, t...).
 */
template<typename T, typename Tuple>
auto PrependToTuple( T&& value, Tuple&& t )
{
    return std::tuple_cat( std::make_tuple(std::forward<T>(value)), std::forward<Tuple>(t) );
}

}