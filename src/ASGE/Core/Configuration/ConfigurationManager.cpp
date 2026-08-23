#include "ConfigurationManager.hpp"

#include <sstream>

using namespace asge::config;
namespace toml = _internal::toml;

asge::BoolResult asge::config::ConfigurationManager::ReadConfiguration( filesystem::Path const& inPath )
{
    std::lock_guard<std::mutex> lock( m_ConfigMutex );

    auto read_result = filesystem::ReadText( inPath );
    if ( !read_result ) {
        return BoolResult::Err( read_result.Error() );
    }
    
    auto result = toml::Parse( read_result.Value() );
    if (!result) {
        return BoolResult::Err( result.Error() );
    }

    m_Configuration = std::move(result.Value());
    return BoolResult::Ok();
}

void asge::config::ConfigurationManager::HotReloadHelper(filesystem::FileEvent const &inEvent)
{
    auto result = ReadConfiguration( inEvent.s_Path ); // Reload the entire configuration
    if (!result)
    {
        auto const err = result.Error();
        LOG_ERROR("Error reloading the conf: ", err.s_Code.message(), err.s_Detail);
    }
}

asge::config::ConfigurationManager::ConfigurationManager(concurrent::context_pointer inCtx)
    : m_FileWatcher(inCtx)
{
    // Just starts the filewatcher
    m_FileWatcher.Start();
}

asge::config::ConfigurationManager::~ConfigurationManager()
{
}

asge::BoolResult asge::config::ConfigurationManager::EnableHotReloadWatch(filesystem::Path const &inPath)
{
    // First we need to check that the previous connection is off
    if ( m_WatcherConnection.IsConnected() ) m_WatcherConnection.Disconnect();

    // Add a new watch to the file watcher which returns the new connection
    auto connectionResult = m_FileWatcher.AddWatch(
        inPath,
        functools::MakeCallback( &ConfigurationManager::HotReloadHelper, this),
        filesystem::FEventType::Modified
    );

    if (!connectionResult) return BoolResult::Err( connectionResult.Error() );
    m_WatcherConnection = connectionResult.Value();
    return BoolResult::Ok();
}

asge::BoolResult asge::config::ConfigurationManager::Load(filesystem::Path const &confPath)
{
    // Check if the current configuration matches with the input one
    if ( !filesystem::meta::IsEmpty( m_ConfigPath ) && m_ConfigPath == confPath )
    {
        return BoolResult::Ok();
    }

    // Check if the input path is a file and has the .toml extension
    if ( filesystem::meta::IsFile( confPath ) )
    {
        auto fileExtensionResult = filesystem::meta::GetExtension( confPath );
        if ( fileExtensionResult && fileExtensionResult.Value() == ".toml" )
        {
            m_ConfigPath = confPath;

            // Read the new configuration
            auto read_result = ReadConfiguration( confPath );
            if ( !read_result )
            {
                return BoolResult::Err( read_result.Error() );
            }

            // Hot-reload is opt-in: only register the watch if it has
            // already been requested via SetHotReloadEnabled(true)
            if ( m_HotReloadEnabled.load( std::memory_order_acquire ) )
            {
                return EnableHotReloadWatch( confPath );
            }

            return BoolResult::Ok();
        }
    }

    // Returns error if the input path is an invalid path
    auto const ec = make_error_code( errors::ConfError::InvalidInputPath );
    return BoolResult::Err( ec, str::ToUTF8( confPath.u8string() ) );
}

asge::BoolResult asge::config::ConfigurationManager::SetHotReloadEnabled(bool inEnabled)
{
    m_HotReloadEnabled.store( inEnabled, std::memory_order_release );

    if ( !inEnabled )
    {
        if ( m_WatcherConnection.IsConnected() ) m_WatcherConnection.Disconnect();
        return BoolResult::Ok();
    }

    if ( m_ConfigPath.empty() || m_WatcherConnection.IsConnected() )
    {
        return BoolResult::Ok();
    }

    return EnableHotReloadWatch( m_ConfigPath );
}

bool asge::config::ConfigurationManager::IsHotReloadEnabled() const noexcept
{
    return m_HotReloadEnabled.load( std::memory_order_acquire );
}

asge::BoolResult asge::config::ConfigurationManager::Save()
{
    std::lock_guard<std::mutex> lock( m_ConfigMutex );

    auto cfg = m_Configuration;
    if ( !cfg )
    {
        return BoolResult::Err( make_error_code( errors::ConfError::ConfigurationNotLoaded ) );
    }

    std::ostringstream oss;
    oss << *cfg; // reuses Table::operator<</PrintTable, now format-aware

    // If hot-reload is enabled, this write will itself trigger the file
    // watcher's Modified event and cause a re-parse of what was just
    // written (ReadConfiguration takes the same m_ConfigMutex, so this is
    // fully serialized) — an expected, harmless extra reload, not a bug.
    return filesystem::WriteText( m_ConfigPath, oss.str() );
}
