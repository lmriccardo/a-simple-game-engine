#pragma once

#include <string>
#include <filesystem>
#include <atomic>
#include <optional>
#include <mutex>

#include <ASGE/Core/Filesystem/Filesystem.hpp>
#include <ASGE/Core/Errors.hpp>

#include <ASGE/Core/Formats/TOML/TOML_Parser.hpp>

namespace asge::config
{

/**
 * @brief Loads, queries, edits and saves one TOML configuration file.
 *
 * Holds the parsed document, optionally reloads it when the file changes on
 * disk (hot reload), and guards access with a mutex so reads, edits and a
 * reload swap do not race.
 */
class ConfigurationManager
{
private:
    using table_pointer_t = formats::toml::table_pointer;

    filesystem::Path             m_ConfigPath;                  // Path of the loaded configuration file
    filesystem::FileWatcher      m_FileWatcher;                 // Watches the file for hot reload
    filesystem::WatcherHandler   m_WatcherConnection;           // Connection of the current file watch
    table_pointer_t              m_Configuration{ nullptr };    // The parsed document, null until loaded
    std::atomic<bool>            m_HotReloadEnabled{ false };   // Whether to reload on file changes

    mutable std::mutex           m_ConfigMutex; // Guards Get/Set/Save against each other and a hot-reload swap

    /**
     * @brief Reads and parses inPath, replacing the current document on success.
     *
     * On a read or parse error the previous configuration is kept.
     */
    BoolResult ReadConfiguration( filesystem::Path const& inPath );

    /**
     * @brief File watch callback: reloads the whole configuration and logs any error.
     */
    void HotReloadHelper( filesystem::FileEvent const& inEvent );

    /**
     * @brief (Re)registers the file watch on inPath, replacing any existing one.
     *
     * Only Modified events trigger a reload.
     */
    BoolResult EnableHotReloadWatch( filesystem::Path const& inPath );
public:
    /**
     * @brief Creates a manager with no configuration and starts its file watcher.
     *
     * inCtx is the context the watcher thread runs under (Background() if null).
     */
    ConfigurationManager( concurrent::context_pointer inCtx = nullptr );

    /**
     * @brief Stops the file watcher.
     */
    ~ConfigurationManager();

    /**
     * @brief Loads a `.toml` file as the active configuration.
     *
     * Fails with ConfError::InvalidInputPath if confPath is not a regular file
     * with a .toml extension, or with the parse error. Loading the path that is
     * already loaded does nothing. If hot reload is enabled the file is watched.
     */
    BoolResult Load( filesystem::Path const& confPath );

    /**
     * @brief Turns hot reload on or off.
     *
     * Hot reload is opt-in. Enabling it before Load() just arms it; the watch
     * is registered once a file is loaded. Disabling drops the current watch.
     */
    BoolResult SetHotReloadEnabled( bool inEnabled );

    /**
     * @brief True while hot reload is enabled.
     */
    bool IsHotReloadEnabled() const noexcept;

    /**
     * @brief Writes the current document back to the loaded file.
     *
     * Fails with ConfError::ConfigurationNotLoaded if nothing is loaded. With
     * hot reload on, the write triggers a reload of what was just written.
     */
    BoolResult Save();

    /**
     * @brief Reads the value at a dotted key path as type T.
     *
     * T may be a scalar TOML type or a std::vector of one. Fails with
     * ConfigurationNotLoaded, TomlNoAttribute or TomlTypeMismatch. Returns a
     * copy, so it stays valid across a hot reload.
     */
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
     * @brief Overwrites the value of an already-existing key.
     *
     * Preserves its original TOML formatting. Fails if the path doesn't
     * already resolve to an existing key (no auto-vivification). The change is
     * in memory only until Save() is called.
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