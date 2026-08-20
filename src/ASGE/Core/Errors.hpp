#pragma once

#include <system_error>
#include <string>
#include <variant>
#include <type_traits>
#include "Strings.hpp"
#include <ASGE/Core/Logger/Logger.hpp>

#ifndef ASGE_ASSERT
#include <cassert>
#define ASGE_ASSERT(cond, msg) assert((cond) && (msg))
#endif

namespace asge::errors::_internal
{

/**
 * @brief Carries a std::error_code alongside optional human-readable context.
 *
 * The @c code field is what callers should switch on / compare against
 * error enums. The @c detail field is purely for logging / diagnostics
 * (e.g. the offending file path) and should not be parsed by callers.
 */
struct ErrorInfo
{
    std::error_code s_Code;   // The actual error code
    std::string     s_Detail; // A string containig details about error

    [[nodiscard]] explicit operator bool() const noexcept
    {
        return static_cast<bool>( s_Code );
    }

    inline str::String String() const noexcept 
    {
        return s_Code.message() + " (Detail: " + s_Detail + ")";
    }
};

inline std::ostream& operator<<( std::ostream& oss, ErrorInfo const& ei )
{
    oss << ei.String();
    return oss;
}

template<typename EnumT>
class GenericErrorCategory final : public std::error_category
{
    char const* m_Name; // The name of the error category
public:
    explicit GenericErrorCategory( char const* inName ) noexcept
    : m_Name( inName )
    {}

    [[nodiscard]] char const* name() const noexcept override
    {
        return m_Name;
    }

    [[nodiscard]] std::string message(int inEv) const override
    {
        return ToErrorString(static_cast<EnumT>(inEv));
    }
};

template<typename T, typename Derived>
class ResultBase
{
    // grants Derived access to protected members below
    friend Derived;

    template<typename ResultT>
    class is_result_impl
    {
        /**
         * No need to implement those test methods since they are just for
         * signature purpose only. Those are not actually called, we only
         * need the return type to get the boolean constexpr value
         */
        template<typename U, typename D>
        static std::true_type test( ResultBase<U,D> const* );
        static std::false_type test(...);
    public:
        static constexpr bool value = decltype( test(std::declval<ResultT*>()) )::value;
    };

    template<typename ResultT>
    static constexpr bool is_result_v = is_result_impl<ResultT>::value;

protected:
    using error_info = errors::_internal::ErrorInfo;
    std::variant<T, error_info> m_Storage;

    ResultBase( std::in_place_index_t<0>, T inValue )
        : m_Storage( std::in_place_index<0>, std::move(inValue) )
    {}

    ResultBase( std::in_place_index_t<1>, error_info inError )
        : m_Storage( std::in_place_index<1>, std::move(inError) )
    {}

public:
    using value_type = T;

    [[nodiscard]] static Derived Ok( T inValue ) noexcept
    {
        return Derived( std::in_place_index<0>, std::move(inValue) );
    }

    [[nodiscard]] static Derived Err( error_info inError ) noexcept
    {
        return Derived( std::in_place_index<1>, std::move(inError) );
    }

    [[nodiscard]] static Derived Err( std::error_code code )
    {
        return Derived( std::in_place_index<1>, error_info{ std::move(code), {} } );
    }

    [[nodiscard]] static Derived Err( std::error_code code, std::string detail )
    {
        return Derived( std::in_place_index<1>, error_info{ std::move(code), std::move(detail) } );
    }

    [[nodiscard]] bool IsOk() const noexcept { return m_Storage.index() == 0; }
    [[nodiscard]] explicit operator bool() const noexcept { return IsOk(); }

    [[nodiscard]] T const& Value() const&
    {
        ASGE_ASSERT(IsOk(), "Result::Value() called on an error result");
        return std::get<0>(m_Storage);
    }

    [[nodiscard]] T&& Value() &&
    {
        ASGE_ASSERT(IsOk(), "Result::Value() called on an error result");
        return std::get<0>(std::move(m_Storage));
    }

    [[nodiscard]] error_info const& Error() const
    {
        ASGE_ASSERT(!IsOk(), "Result::Error() called on a success result");
        return std::get<1>(m_Storage);
    }

    [[nodiscard]] std::error_code Code() const noexcept
    {
        return IsOk() ? std::error_code{} : std::get<1>(m_Storage).s_Code;
    }

    template<typename Other>
    requires is_result_v<std::decay_t<Other>> && 
    std::is_constructible_v<T, typename std::decay_t<Other>::value_type>
    static Derived From( Other&& other ) noexcept
    {
        if ( !other ) return Derived::Err( other.Error() );
        return Derived::Ok( T( std::forward<Other>(other).Value() ) );
    }

    void LogError() const noexcept
    {
        if (IsOk()) return;
        LOG_ERROR( Error() );
    }
};

}

namespace asge
{

// ---------------------------------------------------------------------------
// Result<T> — value-or-error, built on ErrorInfo/std::error_code.
// ---------------------------------------------------------------------------

/**
 * @brief A value-or-error type combining a computed value with a failure, 
 *        for interop with std::error_code and its categories (including 
 *        custom domains registered via ASGE_ERROR_CODE_ENUM).
 *
 * @tparam T Value type produced on success. Must be movable.
 */
template<typename T>
class Result final : public errors::_internal::ResultBase<T, Result<T>>
{
    friend class errors::_internal::ResultBase<T, Result<T>>;
    using Base = errors::_internal::ResultBase<T, Result<T>>;

protected:
    using Base::Base;
};

class BoolResult final : public errors::_internal::ResultBase<bool, BoolResult>
{
    friend class errors::_internal::ResultBase<bool, BoolResult>;
    using Base = errors::_internal::ResultBase<bool, BoolResult>;

protected:
    using Base::Base;
public:
    [[nodiscard]] static BoolResult Ok() noexcept 
    { return BoolResult(std::in_place_index<0>, true); } 
};

}

[[nodiscard]] inline std::error_code MakeErrorFromErrno() noexcept
{
  return { errno, std::generic_category() };
}

inline void LogError( std::error_code ec, asge::str::String detail={} ) noexcept
{
    LOG_ERROR( asge::errors::_internal::ErrorInfo{ ec, detail } );
}

// ---------------------------------------------------------------------------
// REGISTER_ASGE_ERROR — registers a scoped enum as a std::error_code domain
// without hand-writing an error_category subclass each time.
//
// Requires a free function `std::string ToErrorString(EnumType)` visible at
// the point of expansion (resolved via ADL). Define the enum, ToErrorString,
// and invoke the macro all in the same namespace so ADL finds everything.
// ---------------------------------------------------------------------------
#define REGISTER_ASGE_ERROR(EnumType, CategoryLabel)                                            \
    inline std::error_category const& ErrorCategoryFor(EnumType) noexcept                       \
    {                                                                                           \
        static asge::errors::_internal::GenericErrorCategory<EnumType> instance(CategoryLabel); \
        return instance;                                                                        \
    }                                                                                           \
    inline std::error_code make_error_code(EnumType e) noexcept                                 \
    {                                                                                           \
        return {static_cast<int>(e), ErrorCategoryFor(e)};                                      \
    }                                                                                           \
    namespace std                                                                               \
    {                                                                                           \
    template<>                                                                                  \
    struct is_error_code_enum<EnumType> : true_type                                             \
    {                                                                                           \
    };                                                                                          \
    }

namespace asge::errors
{

// ---------------------------------------------------------------------------------------------
// FILE WATCHER ERRORS
// ---------------------------------------------------------------------------------------------

enum class FileWatcherError : std::uint8_t
{
    AlreadyWatched = 1,
    FailedDirRegister,
    InvalidFwHandle,
#ifndef _WIN32
    InvalidWatchDescriptor,
#endif
};

inline str::String ToErrorString(FileWatcherError e) noexcept
{
    switch (e)
    {
    case FileWatcherError::AlreadyWatched: return "path is already being watched";
    case FileWatcherError::FailedDirRegister: return "failed to register directory changes window.";
    case FileWatcherError::InvalidFwHandle: return "invalid file watcher handle";
#ifndef _WIN32
    case FileWatcherError::InvalidWatchDescriptor: return "invalid watch descriptor";
#endif
    }
    return "uknown file watcher error";
}

// ---------------------------------------------------------------------------------------------
// CONFIGURATION LOADING ERRORS
// ---------------------------------------------------------------------------------------------

enum class ConfError : std::uint8_t
{
    InvalidInputPath = 1,
    ConfigurationNotLoaded,

    // TOML ERRORS
    TomlExpectedNestedArray,
    TomlTypeMismatch,
    TomlAlreadyExistingKey,
    TomlNoSubtable,
    TomlInvalidString,
    TomlBadFormatting,
    TomlUnexpectedToken,
    TomlNoAttribute
};

inline str::String ToErrorString(ConfError e) noexcept
{
    switch (e)
    {
    case ConfError::InvalidInputPath: return "invalid configuration input path";
    case ConfError::ConfigurationNotLoaded: return "no configuration has been loaded yet";
    case ConfError::TomlExpectedNestedArray: return "expected nested array";
    case ConfError::TomlTypeMismatch: return "invalid type mismatch";
    case ConfError::TomlAlreadyExistingKey: return "key already exists";
    case ConfError::TomlNoSubtable: return "no existing subtable";
    case ConfError::TomlInvalidString: return "invalid string format";
    case ConfError::TomlBadFormatting: return "bad toml configuration formatting";
    case ConfError::TomlUnexpectedToken: return "unexpected token in toml configuration";
    case ConfError::TomlNoAttribute: return "no existing attribute";
    }
    return "uknown configuration error";
}

// ---------------------------------------------------------------------------------------------
// IMAGE LOADING ERRORS
// ---------------------------------------------------------------------------------------------

enum class ImageError
{
    DecodeFailed = 1,
};

inline str::String ToErrorString(ImageError e) noexcept
{
    switch (e)
    {
    case ImageError::DecodeFailed: return "failed to decode image data";
    }
    return "unknown image error";
}

// ---------------------------------------------------------------------------------------------
// FONT LOADING ERRORS
// ---------------------------------------------------------------------------------------------

enum class FontError
{
    InitFailed = 1, // stb_truetype could not parse the font data
    BakeFailed,     // glyph atlas baking failed (e.g. atlas too small)
    GlyphNotFound,  // requested codepoint was not baked into the atlas
    UnexistingCodepoint,
};

inline str::String ToErrorString(FontError e) noexcept
{
    switch (e)
    {
    case FontError::InitFailed: return "failed to parse the font data";
    case FontError::BakeFailed: return "failed to bake the glyph atlas";
    case FontError::GlyphNotFound: return "codepoint not found in the baked glyph range";
    case FontError::UnexistingCodepoint: return "given codepoint does not exists";
    }
    return "unknown font error";
}

// ---------------------------------------------------------------------------------------------
// ENTITY COMPONENT SYSTEM ERRORS
// ---------------------------------------------------------------------------------------------

enum class EcsError
{
    NoMoreEntityAvailable = 1,
    EntityIsNotAlive,
};

inline str::String ToErrorString(EcsError e) noexcept
{
    switch (e)
    {
    case EcsError::NoMoreEntityAvailable: return "no more available entities";
    case EcsError::EntityIsNotAlive: return "entity is no more alive";
    }
    return "unknown ecs error";
}

}

// REGISTERING ERRORS CATEGORIES TO THE ERROR DB

REGISTER_ASGE_ERROR(asge::errors::FileWatcherError, "asge.filewatcher")
REGISTER_ASGE_ERROR(asge::errors::ConfError, "asge.configuration")
REGISTER_ASGE_ERROR(asge::errors::ImageError, "asge.graphics.image")
REGISTER_ASGE_ERROR(asge::errors::FontError, "asge.graphics.font")
REGISTER_ASGE_ERROR(asge::errors::EcsError, "asge.ecs")