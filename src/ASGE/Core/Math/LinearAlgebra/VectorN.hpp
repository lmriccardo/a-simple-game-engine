#pragma once

#include <cstdint>
#include <array>
#include <concepts>
#include <type_traits>
#include <algorithm>
#include <stdexcept>
#include <initializer_list>
#include <functional>
#include <cmath>

namespace asge::math
{

namespace _internal
{

/**
 * @brief Constrains to numeric types (integral or floating point, but not bool).
 */
template<typename T>
concept Numeric = 
    (    std::is_integral_v<std::remove_cvref_t<T>> 
      || std::is_floating_point_v<std::remove_cvref_t<T>> 
    ) && !std::is_same_v<std::remove_cvref_t<T>, bool>;
    
}

template<std::size_t, _internal::Numeric, typename>
class VecN;

namespace _internal
{

template<typename T>
struct rebind_derived_trait {};

template<std::size_t N, Numeric T>
struct rebind_derived_trait<VecN<N, T, void>>
{
    template<Numeric U>
    using type = VecN<N, U, void>;
};

}

/**
 * @brief A fixed-size N-component numeric vector, the base of Vec2 and Vec3.
 *
 * Stores N values of type T inline and provides element access, component-wise
 * arithmetic and scalar scaling. Derived is the CRTP type that operators
 * return (a Vec2 stays a Vec2); leave it void to use VecN directly. Mixed
 * element types are allowed, and the result takes their common type.
 */
template<std::size_t N, _internal::Numeric T, typename Derived = void>
class VecN
{
protected:
    using actual_derived = std::conditional_t<std::is_void_v<Derived>, VecN, Derived>;

    template<_internal::Numeric U>
    using rebind_actual_derived_t = typename _internal::rebind_derived_trait<actual_derived>::template type<U>;

    /**
     * @brief This vector as its most derived type.
     *
     * Lets the arithmetic operators return the derived vector (CRTP).
     */
    actual_derived& Self() { return static_cast<actual_derived&>(*this); }

    /**
     * @brief Const overload of Self().
     */
    actual_derived const& Self() const { return static_cast<actual_derived const&>(*this); }

    /**
     * @brief Writes op(a[i], b[i]) into inOut for each of the N components.
     *
     * Backs the vector-vector operators.
     */
    template<typename _OutIter, typename _InIter1, typename _InIter2, typename BinaryOp>
    static void _ApplyBinaryIter( _OutIter inOut, _InIter1 inIter1, _InIter2 inIter2, BinaryOp op )
    {
        for (size_type ii = 0; ii < N; ++ii)
        {
            *(inOut + ii) = op(*(inIter1 + ii), *(inIter2 + ii));
        }
    }

    /**
     * @brief Writes op(a[i], inScalar) into inOut for each of the N components.
     *
     * Backs the vector-scalar operators.
     */
    template<typename _OutIter, typename _InIter, typename _Scalar, typename BinaryOp>
    static void _ApplyBinaryScalar( _OutIter inOut, _InIter inIter, _Scalar inScalar, BinaryOp op )
    {
        for (size_type ii = 0; ii < N; ++ii)
        {
            *(inOut + ii) = op(*(inIter + ii), inScalar);
        }
    }

    T m_Vector[N]; // The actual container
public:
    using value_type = T;
    using reference = value_type&;
    using const_reference = value_type const&;
    using pointer = value_type*;
    using const_pointer = value_type const*;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using iterator = pointer;
    using const_iterator = const_pointer;

    /**
     * @brief Leaves the components uninitialized.
     */
    VecN() = default;

    /**
     * @brief Builds the vector from exactly N numbers, each converted to T.
     */
    template<_internal::Numeric... Us>
    requires (sizeof...(Us) == N)
    constexpr VecN( Us... inValues ) : m_Vector{ static_cast<T>(inValues)... }
    {}

    /**
     * @brief Builds a vector with every component set to inValue.
     */
    explicit VecN( value_type const inValue )
    {
        Fill( inValue );
    }

    /**
     * @brief Builds the vector from a brace list; missing components are 0.
     *
     * Extra values past the N-th are ignored.
     */
    constexpr VecN( std::initializer_list<T> inList )
    {
        Zeros(); // First set all elements to zero
        std::copy_n( inList.begin(), std::min( N, inList.size() ), m_Vector );
    }

    /**
     * @brief Copies the components of inOther.
     */
    VecN( VecN const& inOther ) noexcept
    {
        std::copy(inOther.m_Vector, inOther.m_Vector + N, m_Vector);
    }

    /**
     * @brief Moves the components of inOther (a plain copy for numbers).
     */
    VecN( VecN&& inOther ) noexcept
    {
        std::move(inOther.m_Vector, inOther.m_Vector + N, m_Vector);
    }

    /**
     * @brief Copy-assigns the components of inOther.
     */
    VecN& operator=( VecN const& inOther ) noexcept
    {
        if (this != &inOther) 
        {
            std::copy(inOther.m_Vector, inOther.m_Vector + N, m_Vector);
        }

        return *this;
    }

    /**
     * @brief Move-assigns the components of inOther (a plain copy for numbers).
     */
    VecN& operator=( VecN&& inOther ) noexcept
    {
        if (this != &inOther) 
        {
            std::move(inOther.m_Vector, inOther.m_Vector + N, m_Vector);
        }

        return *this;
    }

    /**
     * @brief Virtual so a VecN can be deleted through a base pointer.
     *
     * Note this adds a vtable pointer to every vector.
     */
    virtual ~VecN() = default;

    /**
     * @brief Returns a vector with all zeros of the corresponding type.
     */
    static VecN Zero() noexcept
    {
        return VecN( static_cast<value_type>(0) );
    }

    /**
     * @brief Fills the entire vector with the input value.
     */
    constexpr void Fill( value_type const inValue ) noexcept
    {
        std::fill(m_Vector, m_Vector + N, inValue);
    }

    /**
     * @brief Fills the vector with zeros of the corresponding type.
     */
    constexpr void Zeros() noexcept
    {
        Fill(static_cast<value_type>(0));
    }

    /**
     * @brief Returns the number of components (always N).
     */
    constexpr size_type Size() const noexcept
    {
        return N;
    }

    /**
     * @brief True if the vector has no components, which only happens for N == 0.
     */
    constexpr bool Empty() const noexcept
    {
        return Size() == 0;
    }

    /**
     * @brief Returns the element at the given position.
     *
     * Bounds-checked: throws std::out_of_range if inPos >= N. Note that Vec2
     * and Vec3 make this operator private; use their x()/y()/z() accessors.
     */
    constexpr reference operator[]( size_type inPos )
    {
        if ( inPos >= Size() )
        {
            throw std::out_of_range("Accessing an element out of range");
        }

        return m_Vector[inPos];
    }

    /**
     * @brief Returns a const reference to the element at the given position.
     *
     * Bounds-checked: throws std::out_of_range if inPos >= N.
     */
    constexpr const_reference operator[](size_type inPos) const
    {
        if ( inPos >= Size() )
        {
            throw std::out_of_range("Accessing an element out of range");
        }

        return m_Vector[inPos];
    }

    /**
     * @brief Returns a pointer to the N contiguous components.
     */
    pointer Data() noexcept
    {
        return m_Vector;
    }

    /**
     * @brief Returns a const pointer to the N contiguous components.
     */
    const_pointer Data() const noexcept
    {
        return m_Vector;
    }

    /**
     * @brief Component-wise sum of two vectors.
     *
     * The result keeps this vector's derived type, with the common type of the two
     * element types. The operators below follow the same rule: component-wise for a
     * vector, per component for a scalar, and compound forms update in place.
     * Integer division by zero is undefined behavior.
     */
    template<_internal::Numeric U, typename OtherDerived>
    auto operator+( VecN<N,U,OtherDerived> const& inOther ) const noexcept
    {
        rebind_actual_derived_t<std::common_type_t<T,U>> result;
        _ApplyBinaryIter( result.Data(), Data(), inOther.Data(), std::plus<>{} );
        return result;
    }

    /**
     * @brief Component-wise difference of two vectors.
     *
     * Same result type rules as operator+.
     */
    template<_internal::Numeric U, typename OtherDerived>
    auto operator-(VecN<N, U,OtherDerived> const& inOther) const noexcept 
    {
        rebind_actual_derived_t<std::common_type_t<T,U>> result;
        _ApplyBinaryIter( result.Data(), Data(), inOther.Data(), std::minus<>{} );
        return result;
    }

    /**
     * @brief Component-wise product of two vectors.
     *
     * Same result type rules as operator+.
     */
    template<_internal::Numeric U, typename OtherDerived>
    auto operator*(VecN<N, U,OtherDerived> const& inOther) const noexcept 
    {
        rebind_actual_derived_t<std::common_type_t<T,U>> result;
        _ApplyBinaryIter( result.Data(), Data(), inOther.Data(), std::multiplies<>{} );
        return result;
    }

    /**
     * @brief Component-wise quotient of two vectors.
     *
     * Same result type rules as operator+.
     */
    template<_internal::Numeric U, typename OtherDerived>
    auto operator/(VecN<N, U,OtherDerived> const& inOther) const noexcept 
    {
        rebind_actual_derived_t<std::common_type_t<T,U>> result;
        _ApplyBinaryIter( result.Data(), Data(), inOther.Data(), std::divides<>{} );
        return result;
    }

    /**
     * @brief Multiplies every component by inScalar.
     */
    template<_internal::Numeric U>
    auto operator*(U const& inScalar) const noexcept
    {
        rebind_actual_derived_t<std::common_type_t<T,U>> result;
        _ApplyBinaryScalar( result.Data(), Data(), inScalar, std::multiplies<>{} );
        return result;
    }

    /**
     * @brief Divides every component by inScalar.
     */
    template<_internal::Numeric U>
    auto operator/(U const& inScalar) const noexcept
    {
        rebind_actual_derived_t<std::common_type_t<T,U>> result;
        _ApplyBinaryScalar( result.Data(), Data(), inScalar, std::divides<>{} );
        return result;
    }

    /**
     * @brief Adds inOther component-wise, in place.
     *
     * Returns this vector.
     */
    template<_internal::Numeric U, typename OtherDerived>
    auto& operator+=(VecN<N, U, OtherDerived> const& inOther) noexcept 
    {
        _ApplyBinaryIter(Data(), Data(), inOther.Data(), std::plus<>{});
        return Self();
    }

    /**
     * @brief Subtracts inOther component-wise, in place.
     *
     * Returns this vector.
     */
    template<_internal::Numeric U, typename OtherDerived>
    auto& operator-=(VecN<N, U, OtherDerived> const& inOther) noexcept
    {
        _ApplyBinaryIter(Data(), Data(), inOther.Data(), std::minus<>{});
        return Self();
    }

    /**
     * @brief Multiplies component-wise by inOther, in place.
     *
     * Returns this vector.
     */
    template<_internal::Numeric U, typename OtherDerived>
    auto& operator*=(VecN<N, U, OtherDerived> const& inOther) noexcept 
    {
        _ApplyBinaryIter(Data(), Data(), inOther.Data(), std::multiplies<>{});
        return Self();
    }

    /**
     * @brief Divides component-wise by inOther, in place.
     *
     * Returns this vector.
     */
    template<_internal::Numeric U, typename OtherDerived>
    auto& operator/=(VecN<N, U, OtherDerived> const& inOther) noexcept 
    {
        _ApplyBinaryIter(Data(), Data(), inOther.Data(), std::divides<>{});
        return Self();
    }

    /**
     * @brief Multiplies every component by inOther, in place.
     *
     * Returns this vector.
     */
    template<_internal::Numeric U>
    auto& operator*=(U const& inOther) noexcept 
    {
        _ApplyBinaryScalar(Data(), Data(), inOther, std::multiplies<>{});
        return Self();
    }

    /**
     * @brief Divides every component by inOther, in place.
     *
     * Returns this vector.
     */
    template<_internal::Numeric U>
    auto& operator/=(U const& inOther) noexcept 
    {
        _ApplyBinaryScalar(Data(), Data(), inOther, std::divides<>{});
        return Self();
    }

    /**
     * @brief Scalar times vector; same as vec * scalar.
     */
    template<_internal::Numeric U>
    friend auto operator*(U const& scalar, actual_derived const& vec) noexcept 
    {
        return vec * scalar;
    }

    /**
     * @brief Scalar divided by vector, component-wise.
     *
     * Each component of the result is scalar / vec[i].
     */
    template<_internal::Numeric U>
    friend auto operator/(U const& scalar, actual_derived const& vec) noexcept 
    {
        rebind_actual_derived_t<std::common_type_t<T,U>> result;
        for (size_type ii = 0; ii < N; ++ii) result.Data()[ii] = scalar / vec.Data()[ii];
        return result;
    }
};

/**
 * @brief Squared Euclidean length of a vector.
 *
 * Avoids the square root, so it is the cheap choice for comparing lengths.
 * For integer vectors it can overflow T.
 */
template<std::size_t N, _internal::Numeric T, typename Derived>
T LengthSquared( VecN<N, T, Derived> const& inVector ) noexcept
{
    T result{0};
    for ( std::size_t ii = 0; ii < N; ++ii )
    {
        result += inVector[ii] * inVector[ii];
    }
    return result;
}

/**
 * @brief Euclidean length of a vector.
 *
 * Returns T for floating-point vectors and double for integer ones.
 */
template<std::size_t N, _internal::Numeric T, typename Derived>
auto Length( VecN<N, T, Derived> const& inVector ) noexcept
{
    using RetType = std::conditional_t<std::is_floating_point_v<T>, T, double>;
    return static_cast<RetType>( std::sqrt( static_cast<double>(LengthSquared( inVector )) ) );
}

/**
 * @brief Distance between two points, i.e. the length of their difference.
 *
 * Both vectors must have the same type.
 */
template<std::size_t N, _internal::Numeric T, typename Derived>
auto Distance( VecN<N, T, Derived> const& inVec1, VecN<N, T, Derived> const& inVec2 ) noexcept
{
    return Length( inVec2 - inVec1 );
}

/**
 * @brief Returns `inVector` scaled to unit length.
 *
 * Dividing by a zero length (a zero vector) yields a vector of
 * NaNs/infinities. Meant for floating-point vectors: an integer vector
 * truncates back to integers.
 */
template<std::size_t N, _internal::Numeric T, typename Derived>
Derived NormalizeVec( VecN<N, T, Derived> const& inVector ) noexcept
{
    auto l2norm = Length( inVector );
    return static_cast<Derived>( inVector / l2norm );
}

}

#define DEFINE_REBIND_TRAIT(_Class) \
    namespace _internal {\
        template<Numeric T> struct rebind_derived_trait<_Class<T>>{\
            template<Numeric U> using type = _Class<U>;\
        };\
    }
