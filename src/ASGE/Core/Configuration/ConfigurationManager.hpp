#pragma once

#include <string>
#include <filesystem>
#include <ASGE/Core/Filesystem/Filesystem.hpp>
#include <atomic>
#include <optional>
#include "TOML_Parser.hpp"

namespace asge::config
{

class ConfigurationManager
{
private:
    using table_pointer_t = _internal::toml::table_pointer;

    filesystem::Path             m_ConfigPath;
    filesystem::FileWatcher      m_FileWatcher;
    filesystem::WatcherHandler   m_WatcherConnection;
    std::atomic<table_pointer_t> m_Configuration{ nullptr };

    // Reads the configuration of the currently loaded path
    void ReadConfiguration( filesystem::Path const& inPath );
    void HotReloadHelper( filesystem::FileEvent const& inEvent );
public:
    ConfigurationManager( concurrent::context_pointer inCtx = nullptr );
    ~ConfigurationManager();
    
    bool Load( filesystem::Path const& confPath );

    template<typename T>
    std::optional<T> Get( std::string const& inParamPath ) const
    {
        auto cfg = m_Configuration.load( std::memory_order_acquire );
        if constexpr ( asge::_internal::traits::is_vector_v<T> ) {
            auto value = cfg->template GetTypedArray<T>( inParamPath );
            if ( value.empty() ) return std::nullopt;
            return value;
        } else {
            auto const* value = cfg->template Get<T>( inParamPath );
            if (!value) return std::nullopt;
            return *value;
        }
    }
};

}