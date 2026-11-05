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

    /**
     * @brief True when this holds an error (a non-zero code).
     */
    [[nodiscard]] explicit operator bool() const noexcept
    {
        return static_cast<bool>( s_Code );
    }

    /**
     * @brief Human-readable "message (Detail: ...)" text for logs.
     */
    inline str::String String() const noexcept 
    {
        return s_Code.message() + " (Detail: " + s_Detail + ")";
    }
};

/**
 * @brief Streams ei.String(), which lets LOG_ERROR print an ErrorInfo directly.
 */
inline std::ostream& operator<<( std::ostream& oss, ErrorInfo const& ei )
{
    oss << ei.String();
    return oss;
}

/**
 * @brief std::error_category for an error enum, with messages from ToErrorString.
 *
 * Instantiated by REGISTER_ASGE_ERROR; not used directly.
 */
template<typename EnumT>
class GenericErrorCategory final : public std::error_category
{
    char const* m_Name; // The name of the error category
public:
    /**
     * @brief Creates a category named inName (must outlive the category).
     */
    explicit GenericErrorCategory( char const* inName ) noexcept
    : m_Name( inName )
    {}

    /**
     * @brief The category name given at construction.
     */
    [[nodiscard]] char const* name() const noexcept override
    {
        return m_Name;
    }

    /**
     * @brief Message for inEv, found by calling ToErrorString on the enum value.
     */
    [[nodiscard]] std::string message(int inEv) const override
    {
        return ToErrorString(static_cast<EnumT>(inEv));
    }
};

/**
 * @brief Shared implementation of Result<T> and BoolResult (CRTP).
 *
 * Holds either a T or an ErrorInfo in a std::variant. Value() and Error()
 * assert that the result is in the matching state.
 */
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
    std::variant<T, error_info> m_Storage; // The value (index 0) or the error (index 1)

    /**
     * @brief Builds a success result holding inValue.
     */
    ResultBase( std::in_place_index_t<0>, T inValue )
        : m_Storage( std::in_place_index<0>, std::move(inValue) )
    {}

    /**
     * @brief Builds an error result holding inError.
     */
    ResultBase( std::in_place_index_t<1>, error_info inError )
        : m_Storage( std::in_place_index<1>, std::move(inError) )
    {}

public:
    using value_type = T;

    /**
     * @brief Creates a success result holding inValue.
     */
    [[nodiscard]] static Derived Ok( T inValue ) noexcept
    {
        return Derived( std::in_place_index<0>, std::move(inValue) );
    }

    /**
     * @brief Creates an error result from a prebuilt ErrorInfo.
     */
    [[nodiscard]] static Derived Err( error_info inError ) noexcept
    {
        return Derived( std::in_place_index<1>, std::move(inError) );
    }

    /**
     * @brief Creates an error result from a code, with no detail text.
     */
    [[nodiscard]] static Derived Err( std::error_code code )
    {
        return Derived( std::in_place_index<1>, error_info{ std::move(code), {} } );
    }

    /**
     * @brief Creates an error result from a code plus detail text (e.g. a file path).
     */
    [[nodiscard]] static Derived Err( std::error_code code, std::string detail )
    {
        return Derived( std::in_place_index<1>, error_info{ std::move(code), std::move(detail) } );
    }

    /**
     * @brief True if this holds a value rather than an error.
     */
    [[nodiscard]] bool IsOk() const noexcept { return m_Storage.index() == 0; }

    /**
     * @brief Same as IsOk(), so a result can be tested with `if ( result )`.
     */
    [[nodiscard]] explicit operator bool() const noexcept { return IsOk(); }

    /**
     * @brief The stored value.
     *
     * Asserts IsOk() in debug builds; reading an error result's value is
     * undefined (std::bad_variant_access when asserts are off).
     */
    [[nodiscard]] T const& Value() const&
    {
        ASGE_ASSERT(IsOk(), "Result::Value() called on an error result");
        return std::get<0>(m_Storage);
    }

    /**
     * @brief Moves the stored value out of a temporary result.
     *
     * Same precondition as the const& overload.
     */
    [[nodiscard]] T&& Value() &&
    {
        ASGE_ASSERT(IsOk(), "Result::Value() called on an error result");
        return std::get<0>(std::move(m_Storage));
    }

    /**
     * @brief The stored ErrorInfo.
     *
     * Asserts that the result is an error; check IsOk() first.
     */
    [[nodiscard]] error_info const& Error() const
    {
        ASGE_ASSERT(!IsOk(), "Result::Error() called on a success result");
        return std::get<1>(m_Storage);
    }

    /**
     * @brief The error code, or an empty (falsy) code for a success result.
     *
     * Safe to call in either state, unlike Error().
     */
    [[nodiscard]] std::error_code Code() const noexcept
    {
        return IsOk() ? std::error_code{} : std::get<1>(m_Storage).s_Code;
    }

    /**
     * @brief Converts another Result into this type, keeping its error if it has one.
     *
     * On success the other's value is converted with T's constructor; on
     * failure the same ErrorInfo is carried over. Lets a function return a
     * `Result<U>` where a `Result<T>` is expected.
     */
    template<typename Other>
    requires is_result_v<std::decay_t<Other>> &&
    std::is_constructible_v<T, typename std::decay_t<Other>::value_type>
    static Derived From( Other&& other ) noexcept
    {
        if ( !other ) return Derived::Err( other.Error() );
        return Derived::Ok( T( std::forward<Other>(other).Value() ) );
    }

    /**
     * @brief Logs the stored error with LOG_ERROR; does nothing for a success result.
     */
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
 * @brief A value-or-error type, for interop with std::error_code and its categories.
 *
 * Holds either a T (success) or an ErrorInfo (failure), including custom
 * domains registered via REGISTER_ASGE_ERROR. T must be movable. Build one
 * with Ok() or Err(), test it with IsOk() or `if ( result )`.
 */
template<typename T>
class Result final : public errors::_internal::ResultBase<T, Result<T>>
{
    friend class errors::_internal::ResultBase<T, Result<T>>;
    using Base = errors::_internal::ResultBase<T, Result<T>>;

protected:
    using Base::Base;
};

/**
 * @brief Result specialised for success/failure with no payload.
 *
 * Like Result<bool>, with a no-argument Ok() for the success case.
 */
class BoolResult final : public errors::_internal::ResultBase<bool, BoolResult>
{
    friend class errors::_internal::ResultBase<bool, BoolResult>;
    using Base = errors::_internal::ResultBase<bool, BoolResult>;

protected:
    using Base::Base;
public:
    /**
     * @brief Creates a success result (the stored bool is true).
     */
    [[nodiscard]] static BoolResult Ok() noexcept 
    { return BoolResult(std::in_place_index<0>, true); } 
};

}

/**
 * @brief Wraps the current errno value in a generic_category error code.
 *
 * Read errno right after the failing call, since any later call may
 * overwrite it.
 */
[[nodiscard]] inline std::error_code MakeErrorFromErrno() noexcept
{
  return { errno, std::generic_category() };
}

/**
 * @brief Logs an error code and optional detail text through LOG_ERROR.
 *
 * Convenience for call sites that have no Result to call LogError() on.
 */
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

/**
 * @brief Human-readable message for a FileWatcherError value.
 *
 * Looked up by the error category REGISTER_ASGE_ERROR creates.
 */
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

/**
 * @brief Human-readable message for a ConfError value.
 *
 * Looked up by the error category REGISTER_ASGE_ERROR creates.
 */
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

/**
 * @brief Human-readable message for a ImageError value.
 *
 * Looked up by the error category REGISTER_ASGE_ERROR creates.
 */
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

/**
 * @brief Human-readable message for a FontError value.
 *
 * Looked up by the error category REGISTER_ASGE_ERROR creates.
 */
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
    NoMoreComponentsAvailable,
    EntityNotAttachedToComponent,
    InvalidComponent,
    ResourceNotSet,
};

/**
 * @brief Human-readable message for a EcsError value.
 *
 * Looked up by the error category REGISTER_ASGE_ERROR creates.
 */
inline str::String ToErrorString(EcsError e) noexcept
{
    switch (e)
    {
    case EcsError::NoMoreEntityAvailable: return "no more available entity slots";
    case EcsError::EntityIsNotAlive: return "entity is no more alive";
    case EcsError::NoMoreComponentsAvailable: return "no more available component slots";
    case EcsError::EntityNotAttachedToComponent: return "entity does not have component of this type";
    case EcsError::InvalidComponent: return "invalid component";
    case EcsError::ResourceNotSet: return "resource not set";
    }
    return "unknown ecs error";
}

// ---------------------------------------------------------------------------------------------
// VIRTUAL FILE SYSTEM ERRORS
// ---------------------------------------------------------------------------------------------

enum class VfsError
{
    NotMounted = 1,
    AlreadyMounted,
    EmptyMounts,
};

/**
 * @brief Human-readable message for a VfsError value.
 *
 * Looked up by the error category REGISTER_ASGE_ERROR creates.
 */
inline str::String ToErrorString(VfsError e) noexcept
{
    switch (e)
    {
    case VfsError::NotMounted: return "mount not found in vtable";
    case VfsError::AlreadyMounted: return "input path already mounted to the same virtual root";
    case VfsError::EmptyMounts: return "no mounts yet defined";
    }
    return "unknown vfs error";
}

// ---------------------------------------------------------------------------------------------
// AUDIO ERRORS
// ---------------------------------------------------------------------------------------------

enum class AudioError
{
    InvalidFormat = 1,
    DecodeFailed,
    SubsystemInitFailed,
    SubsystemNotInitialized,
    DeviceOpenFailed,
    StreamCreationFailed,
    InvalidStream,
    InvalidDevice,
    StreamDetachFailed,
};

/**
 * @brief Human-readable message for a AudioError value.
 *
 * Looked up by the error category REGISTER_ASGE_ERROR creates.
 */
inline str::String ToErrorString(AudioError e) noexcept
{
    switch (e)
    {
    case AudioError::InvalidFormat: return "currently supported audio format .wav .ogg";
    case AudioError::DecodeFailed: return "audio decode has failed";
    case AudioError::SubsystemInitFailed: return "audio SDL subsystem initialization failed";
    case AudioError::SubsystemNotInitialized: return "SDL audio subsystem not initialized";
    case AudioError::DeviceOpenFailed: return "SDL failed to open a new audio device";
    case AudioError::StreamCreationFailed: return "failed to create a new audio stream";
    case AudioError::InvalidStream: return "invalid or expired stream";
    case AudioError::InvalidDevice: return "invalid device";
    case AudioError::StreamDetachFailed: return "stream detaching failed";
    }
    return "unknown audio error";
}

// ---------------------------------------------------------------------------------------------
// SCENE ERRORS
// ---------------------------------------------------------------------------------------------

enum class SceneError : std::uint8_t
{
    NoActiveScene = 1,
    EmptyProject,
};

/**
 * @brief Human-readable message for a SceneError value.
 *
 * Looked up by the error category REGISTER_ASGE_ERROR creates.
 */
inline str::String ToErrorString(SceneError e) noexcept
{
    switch (e)
    {
    case SceneError::NoActiveScene: return "no scene is currently active";
    case SceneError::EmptyProject: return "project lists no scenes to load";
    }
    return "unknown scene error";
}

}

// REGISTERING ERRORS CATEGORIES TO THE ERROR DB

REGISTER_ASGE_ERROR(asge::errors::FileWatcherError, "asge.filesystem.filewatcher")
REGISTER_ASGE_ERROR(asge::errors::ConfError, "asge.configuration")
REGISTER_ASGE_ERROR(asge::errors::ImageError, "asge.media.image")
REGISTER_ASGE_ERROR(asge::errors::FontError, "asge.media.font")
REGISTER_ASGE_ERROR(asge::errors::EcsError, "asge.ecs")
REGISTER_ASGE_ERROR(asge::errors::VfsError, "asge.filesystem.vfs")
REGISTER_ASGE_ERROR(asge::errors::AudioError, "asge.media.audio")
REGISTER_ASGE_ERROR(asge::errors::SceneError, "asge.scene")

#define BEGIN_ASGE_ERROR_NS namespace asge::errors {
#define END_ASGE_ERRORS_NS }