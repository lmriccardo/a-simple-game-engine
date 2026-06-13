#include "ConfigurationManager.hpp"

using namespace asge::config;
namespace toml = _internal::toml;

void asge::config::ConfigurationManager::ReadConfiguration( filesystem::Path const& inPath )
{
    m_Configuration = toml::Parse( filesystem::ReadText( inPath ) );
}

void asge::config::ConfigurationManager::HotReloadHelper(filesystem::FileEvent const &inEvent)
{
    ReadConfiguration( inEvent.s_Path ); // Reload the entire configuration
}

asge::config::ConfigurationManager::ConfigurationManager(
    concurrent::context_pointer inCtx)
    : m_FileWatcher(inCtx)
{
    // Just starts the filewatcher
    m_FileWatcher.Start();
}

asge::config::ConfigurationManager::~ConfigurationManager()
{
}

bool asge::config::ConfigurationManager::Load(filesystem::Path const &confPath)
{
    // Check if the current configuration matches with the input one
    if ( !filesystem::meta::IsEmpty( m_ConfigPath ) && m_ConfigPath == confPath )
    {
        return true;
    }

    // Check if the input path is a file and has the .toml extension
    if ( filesystem::meta::IsFile( confPath ) )
    {
        auto fileExtension = filesystem::meta::GetExtension( confPath );
        if ( fileExtension && *fileExtension == ".toml" )
        {
            m_ConfigPath = confPath;
            
            // First we need to check that the previous connection is off
            if ( m_WatcherConnection.IsConnected() )
            {
                m_WatcherConnection.Disconnect();
            }

            // Add a new watch to the file watcher which returns the new connection
            m_WatcherConnection = m_FileWatcher.AddWatch(
                confPath, 
                functools::MakeCallback( &ConfigurationManager::HotReloadHelper, this),
                filesystem::FEventType::Modified
            );

            // Read the new configuration
            ReadConfiguration( confPath );

            return true;
        }
    }

    return false;
}
