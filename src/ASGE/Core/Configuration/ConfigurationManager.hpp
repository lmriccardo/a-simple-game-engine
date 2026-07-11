#pragma once

#include <string>
#include <filesystem>
#include <atomic>
#include <optional>

#include <ASGE/Core/Filesystem/Filesystem.hpp>
#include <ASGE/Core/Errors.hpp>

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
    BoolResult ReadConfiguration( filesystem::Path const& inPath );
    void HotReloadHelper( filesystem::FileEvent const& inEvent );
public:
    ConfigurationManager( concurrent::context_pointer inCtx = nullptr );
    ~ConfigurationManager();
    
    BoolResult Load( filesystem::Path const& confPath );

    template<typename T>
    Result<T> Get( std::string const& inParamPath ) const
    {
        auto cfg = m_Configuration.load( std::memory_order_acquire );
        if constexpr ( asge::_internal::traits::is_vector_v<T> ) {
            return cfg->template GetTypedArray<T>( inParamPath );
        } else {
            auto result = cfg->template Get<T>( inParamPath );
            if (!result) return Result<T>::Err( result.Error() );
            return Result<T>::Ok(*result.Value());
        }
    }
};

}