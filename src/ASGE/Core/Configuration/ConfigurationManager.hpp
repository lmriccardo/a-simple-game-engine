#pragma once

#include <string>
#include <filesystem>
#include <atomic>
#include <optional>
#include <mutex>

#include <ASGE/Core/Filesystem/Filesystem.hpp>
#include <ASGE/Core/Errors.hpp>

#include "TOML_Parser.hpp"

namespace asge::config
{

class ConfigurationManager
{
private:
    using table_pointer_t = toml::table_pointer;

    filesystem::Path             m_ConfigPath;
    filesystem::FileWatcher      m_FileWatcher;
    filesystem::WatcherHandler   m_WatcherConnection;
    table_pointer_t              m_Configuration{ nullptr };
    std::atomic<bool>            m_HotReloadEnabled{ false };

    // Guards Get/Set/Save against each other and against a hot-reload swap.
    // Read access stops being lock-free once Set can mutate the live tree
    // in place, so this is a deliberate trade for mutation correctness.
    mutable std::mutex           m_ConfigMutex;

    // Reads the configuration of the currently loaded path
    BoolResult ReadConfiguration( filesystem::Path const& inPath );
    void HotReloadHelper( filesystem::FileEvent const& inEvent );

    // (Re)registers the file watch on inPath, replacing any existing one
    BoolResult EnableHotReloadWatch( filesystem::Path const& inPath );
public:
    ConfigurationManager( concurrent::context_pointer inCtx = nullptr );
    ~ConfigurationManager();

    BoolResult Load( filesystem::Path const& confPath );
    BoolResult SetHotReloadEnabled( bool inEnabled );
    bool IsHotReloadEnabled() const noexcept;
    BoolResult Save();

    template<typename T>
    Result<T> Get( std::string const& inParamPath ) const
    {
        std::lock_guard<std::mutex> lock( m_ConfigMutex );

        auto cfg = m_Configuration;
        if ( !cfg )
        {
            return Result<T>::Err( make_error_code( errors::ConfError::ConfigurationNotLoaded ) );
        }

        if constexpr ( asge::_internal::traits::is_vector_v<T> ) {
            return cfg->template GetTypedArray<typename T::value_type>( inParamPath );
        } else {
            auto result = cfg->template Get<T>( inParamPath );
            if (!result) return Result<T>::Err( result.Error() );
            return Result<T>::Ok(*result.Value());
        }
    }

    /**
     * @brief Overwrites the value of an already-existing key, preserving its
     *        original TOML formatting. Fails if the path doesn't already
     *        resolve to an existing key (no auto-vivification).
     */
    template<typename T>
    BoolResult Set( std::string const& inParamPath, T inValue )
    {
        std::lock_guard<std::mutex> lock( m_ConfigMutex );

        auto cfg = m_Configuration;
        if ( !cfg )
        {
            return BoolResult::Err( make_error_code( errors::ConfError::ConfigurationNotLoaded ) );
        }

        if constexpr ( asge::_internal::traits::is_vector_v<T> ) {
            return cfg->template SetTypedArray<typename T::value_type>( inParamPath, inValue );
        } else {
            return cfg->template Set<T>( inParamPath, std::move(inValue) );
        }
    }
};

}