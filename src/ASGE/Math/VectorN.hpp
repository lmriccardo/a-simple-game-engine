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

template<std::size_t N, _internal::Numeric T, typename Derived = void>
class VecN
{
protected:
    using actual_derived = std::conditional_t<std::is_void_v<Derived>, VecN, Derived>;

    template<_internal::Numeric U>
    using rebind_actual_derived_t = typename _internal::rebind_derived_trait<actual_derived>::template type<U>;

    actual_derived& Self() { return static_cast<actual_derived&>(*this); }
    actual_derived const& Self() const { return static_cast<actual_derived const&>(*this); }

    template<typename _OutIter, typename _InIter1, typename _InIter2, typename BinaryOp>
    static void _ApplyBinaryIter( _OutIter inOut, _InIter1 inIter1, _InIter2 inIter2, BinaryOp op )
    {
        for (size_type ii = 0; ii < N; ++ii)
        {
            *(inOut + ii) = op(*(inIter1 + ii), *(inIter2 + ii));
        }
    }

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

    VecN() = default;

    explicit VecN( value_type const inValue )
    {
        Fill( inValue );
    }

    VecN( std::initializer_list<T> inList )
    {
        Zeros(); // First set all elements to zero
        std::copy_n( inList.begin(), std::min( N, inList.size() ), m_Vector );
    }

    VecN( VecN const& inOther ) noexcept 
    {
        std::copy(inOther.m_Vector, inOther.m_Vector + N, m_Vector);
    }

    VecN( VecN&& inOther ) noexcept
    {
        std::move(inOther.m_Vector, inOther.m_Vector + N, m_Vector);
    }

    VecN& operator=( VecN const& inOther ) noexcept
    {
        if (this != &inOther) 
        {
            std::copy(inOther.m_Vector, inOther.m_Vector + N, m_Vector);
        }

        return *this;
    }

    VecN& operator=( VecN&& inOther ) noexcept
    {
        if (this != &inOther) 
        {
            std::move(inOther.m_Vector, inOther.m_Vector + N, m_Vector);
        }

        return *this;
    }

    virtual ~VecN() = default;

    /* Returns a vector with all zeros of the corresponding type */
    static VecN Zero() noexcept
    {
        return VecN( static_cast<value_type>(0) );
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
    template<_internal::Numeric U, typename OtherDerived>
    auto operator+( VecN<N,U,OtherDerived> const& inOther ) const noexcept
    {
        rebind_actual_derived_t<std::common_type_t<T,U>> result;
        _ApplyBinaryIter( result.Data(), Data(), inOther.Data(), std::plus<>{} );
        return result;
    }

    template<_internal::Numeric U, typename OtherDerived>
    auto operator-(VecN<N, U,OtherDerived> const& inOther) const noexcept 
    {
        rebind_actual_derived_t<std::common_type_t<T,U>> result;
        _ApplyBinaryIter( result.Data(), Data(), inOther.Data(), std::minus<>{} );
        return result;
    }

    template<_internal::Numeric U, typename OtherDerived>
    auto operator*(VecN<N, U,OtherDerived> const& inOther) const noexcept 
    {
        rebind_actual_derived_t<std::common_type_t<T,U>> result;
        _ApplyBinaryIter( result.Data(), Data(), inOther.Data(), std::multiplies<>{} );
        return result;
    }

    template<_internal::Numeric U, typename OtherDerived>
    auto operator/(VecN<N, U,OtherDerived> const& inOther) const noexcept 
    {
        rebind_actual_derived_t<std::common_type_t<T,U>> result;
        _ApplyBinaryIter( result.Data(), Data(), inOther.Data(), std::divides<>{} );
        return result;
    }

    template<_internal::Numeric U>
    auto operator*(U const& inScalar) const noexcept
    {
        rebind_actual_derived_t<std::common_type_t<T,U>> result;
        _ApplyBinaryScalar( result.Data(), Data(), inScalar, std::multiplies<>{} );
        return result;
    }

    template<_internal::Numeric U>
    auto operator/(U const& inScalar) const noexcept
    {
        rebind_actual_derived_t<std::common_type_t<T,U>> result;
        _ApplyBinaryScalar( result.Data(), Data(), inScalar, std::divides<>{} );
        return result;
    }

    template<_internal::Numeric U, typename OtherDerived>
    auto& operator+=(VecN<N, U, OtherDerived> const& inOther) noexcept 
    {
        _ApplyBinaryIter(Data(), Data(), inOther.Data(), std::plus<>{});
        return Self();
    }

    template<_internal::Numeric U, typename OtherDerived>
    auto& operator-=(VecN<N, U, OtherDerived> const& inOther) noexcept
    {
        _ApplyBinaryIter(Data(), Data(), inOther.Data(), std::minus<>{});
        return Self();
    }

    template<_internal::Numeric U, typename OtherDerived>
    auto& operator*=(VecN<N, U, OtherDerived> const& inOther) noexcept 
    {
        _ApplyBinaryIter(Data(), Data(), inOther.Data(), std::multiplies<>{});
        return Self();
    }

    template<_internal::Numeric U, typename OtherDerived>
    auto& operator/=(VecN<N, U, OtherDerived> const& inOther) noexcept 
    {
        _ApplyBinaryIter(Data(), Data(), inOther.Data(), std::divides<>{});
        return Self();
    }

    template<_internal::Numeric U>
    auto& operator*=(U const& inOther) noexcept 
    {
        _ApplyBinaryScalar(Data(), Data(), inOther, std::multiplies<>{});
        return Self();
    }

    template<_internal::Numeric U>
    auto& operator/=(U const& inOther) noexcept 
    {
        _ApplyBinaryScalar(Data(), Data(), inOther, std::divides<>{});
        return Self();
    }

    template<_internal::Numeric U>
    friend auto operator*(U const& scalar, actual_derived const& vec) noexcept 
    {
        return vec * scalar;
    }

    template<_internal::Numeric U>
    friend auto operator/(U const& scalar, actual_derived const& vec) noexcept 
    {
        return vec / scalar;
    }
};
    
}

#define DEFINE_REBIND_TRAIT(_Class) \
    namespace _internal {\
        template<Numeric T> struct rebind_derived_trait<_Class<T>>{\
            template<Numeric U> using type = _Class<U>;\
        };\
    }
