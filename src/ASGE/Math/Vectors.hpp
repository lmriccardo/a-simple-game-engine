#pragma once

#include <cstdint>
#include <array>
#include <concepts>
#include <type_traits>
#include <algorithm>
#include <stdexcept>
#include <initializer_list>
#include <functional>

namespace asge::math
{

namespace _internal
{

/* Concepts constraining to numeric types (not boolean types) */
template<typename T>
concept Numeric = 
    (    std::is_integral_v<std::remove_cvref_t<T>> 
      || std::is_floating_point_v<std::remove_cvref_t<T>> 
    ) && !std::is_same_v<std::remove_cvref_t<T>, bool>;
}

/* Forward Declaration */
template<std::size_t N, _internal::Numeric T, typename Derived>
class VectorN;

namespace _internal
{

template<typename T>
struct is_vector : std::false_type {};

template<std::size_t N, typename T, typename Derived>
struct is_vector<VectorN<N, T, Derived>> : std::true_type {};

template<typename T>
inline constexpr bool is_vector_v = is_vector<std::remove_cvref_t<T>>::value;

}

template<std::size_t N, _internal::Numeric T, typename Derived = void>
class VectorN
{
protected:
    T m_Vector[N]; // The actual container

    using ActualDerived = std::conditional_t<std::is_void_v<Derived>, VectorN, Derived>;

    ActualDerived& Self() { return static_cast<ActualDerived&>(*this); }
    ActualDerived const& Self() const { return static_cast<ActualDerived const&>(*this); }

    template<typename _Input, typename BinaryOp>
    void _ApplyCompound(_Input const& inOther, BinaryOp op) noexcept
    {
        for (size_type ii = 0; ii < N; ++ii)
        {
            if constexpr ( requires { inOther.Data()[ii]; } ) {
                op(m_Vector[ii], inOther.Data()[ii]);
            } else {
                op(m_Vector[ii], inOther);
            }
        }
    }

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

    VectorN() = default;

    explicit VectorN( value_type const inValue )
    {
        Fill( inValue );
    }

    VectorN( std::initializer_list<T> inList )
    {
        Zeros(); // First set all elements to zero
        std::copy_n( inList.begin(), std::min( N, inList.size() ), m_Vector );
    }

    VectorN( VectorN const& inOther ) noexcept 
    {
        std::copy(inOther.m_Vector, inOther.m_Vector + N, m_Vector);
    }

    VectorN( VectorN&& inOther ) noexcept
    {
        std::move(inOther.m_Vector, inOther.m_Vector + N, m_Vector);
    }

    VectorN& operator=( VectorN const& inOther ) noexcept
    {
        if (this != &inOther) 
        {
            std::copy(inOther.m_Vector, inOther.m_Vector + N, m_Vector);
        }

        return *this;
    }

    VectorN& operator=( VectorN&& inOther ) noexcept
    {
        if (this != &inOther) 
        {
            std::move(inOther.m_Vector, inOther.m_Vector + N, m_Vector);
        }

        return *this;
    }

    virtual ~VectorN() = default;

    /* Returns a vector with all zeros of the corresponding type */
    static VectorN Zero() noexcept
    {
        return VectorN( static_cast<value_type>(0) );
    }

    /* Fill the entire vector with the input value */
    void Fill( value_type const inValue ) noexcept
    {
        std::fill(m_Vector, m_Vector + N, inValue);
    }

    /* Fills the vector with zeros of the corresponding type */
    void Zeros() noexcept 
    {
        Fill(static_cast<value_type>(0));
    }

    /* Returns the size of the current array */
    constexpr size_type Size() const noexcept
    {
        return N;
    }

    /* Checks whether the vector contains any element */
    constexpr bool Empty() const noexcept
    {
        return Size() == 0;
    }

    /* Returns the element at given position. Throws if out of bound */
    reference operator[]( size_type inPos )
    {
        if ( inPos >= Size() )
        {
            throw std::out_of_range("Accessing an element out of range");
        }

        return m_Vector[inPos];
    }

    /* Returns a const-reference to the element at given position. Throws if OOB */
    const_reference operator[](size_type inPos) const
    {
        if ( inPos >= Size() )
        {
            throw std::out_of_range("Accessing an element out of range");
        }

        return m_Vector[inPos];
    }

    /* Returns the pointer to the internal data */
    pointer Data() noexcept
    {
        return m_Vector;
    }

    /* Returns the constant pointer to the internal data */
    const_pointer Data() const noexcept
    {
        return m_Vector;
    }

    /* Sums two input vector N */
    template<_internal::Numeric U>
    auto operator+( VectorN<N,U> const& inOther ) const noexcept
    {
        ActualDerived result = Self(); 
        result += inOther;
        return result;
    }

    template<_internal::Numeric U>
    auto operator-(VectorN<N, U> const& inOther) const noexcept 
    {
        ActualDerived result = Self(); 
        result -= inOther;
        return result;
    }

    template<_internal::Numeric U>
    auto operator*(VectorN<N, U> const& inOther) const noexcept 
    {
        ActualDerived result = Self(); 
        result *= inOther;
        return result;
    }

    template<_internal::Numeric U>
    auto operator/(VectorN<N, U> const& inOther) const noexcept 
    {
        ActualDerived result = Self(); 
        result /= inOther;
        return result;
    }

    template<_internal::Numeric U>
    auto operator*(U const& inScalar) const noexcept
    {
        ActualDerived result = Self(); 
        result *= inScalar;
        return result;
    }

    template<typename U>
    auto& operator+=(U const& inOther) noexcept 
    {
        _ApplyCompound(inOther, [](T& a, auto const& b) { a += b; });
        return Self();
    }

    template<typename U>
    auto& operator-=(U const& inOther) noexcept 
    {
        _ApplyCompound(inOther, [](T& a, auto const& b) { a -= b; });
        return Self();
    }

    template<typename U>
    auto& operator*=(U const& inOther) noexcept 
    {
        _ApplyCompound(inOther, [](T& a, auto const& b) { a *= b; });
        return Self();
    }

    template<typename U>
    auto& operator/=(U const& inOther) noexcept 
    {
        _ApplyCompound(inOther, [](T& a, auto const& b) { a /= b; });
        return Self();
    }

    template<_internal::Numeric U>
    friend auto operator*(U const& scalar, Derived const& vec) noexcept 
    {
        return vec * scalar; // Calls the member operator*
    }
};
    
}