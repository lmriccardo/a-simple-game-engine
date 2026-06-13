#pragma once

#include <string>
#include <filesystem>
#include <ASGE/Core/Filesystem/Filesystem.hpp>
#include "TOML_Parser.hpp"

namespace asge::config
{

class ConfigurationManager
{
private:
    using table_pointer_t = _internal::toml::table_pointer;

    filesystem::Path           m_ConfigPath;
    filesystem::FileWatcher    m_FileWatcher;
    filesystem::WatcherHandler m_WatcherConnection;
    table_pointer_t            m_Configuration{ nullptr };

    // Reads the configuration of the currently loaded path
    void ReadConfiguration( filesystem::Path const& inPath );
    void HotReloadHelper( filesystem::FileEvent const& inEvent );
public:
    ConfigurationManager( concurrent::context_pointer inCtx = nullptr );
    ~ConfigurationManager();
    
    bool Load( filesystem::Path const& confPath );
};

}