#include "CppBuild.hpp"

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_iostream.h>
#include <SDL3/SDL_process.h>
#include <SDL3/SDL_timer.h>
#include <SDL3/SDL_properties.h>

#include "BuildOutput.hpp"
#include "CppProject.hpp"

namespace fs = std::filesystem;

namespace
{

/** @brief Adds one line of tool output to the build output, at the level its text suggests. */
void LogToolLine( std::string const& inLine )
{
    if ( inLine.find( "error" ) != std::string::npos ) AppendBuildOutput( BuildLevel::Error, inLine );
    else if ( inLine.find( "warning" ) != std::string::npos ) AppendBuildOutput( BuildLevel::Warning, inLine );
    else AppendBuildOutput( BuildLevel::Info, inLine );
}

}

fs::path CppBuildDir( fs::path const& inProjectFile ) noexcept
{
    return CppProjectDir( inProjectFile ) / "build";
}

std::optional<fs::path> FindCppExecutable( fs::path const& inProjectFile ) noexcept
{
    auto const build = CppBuildDir( inProjectFile );
    auto const target = CppTargetName( inProjectFile );

    std::error_code ec;
    for ( auto const& candidate : { build / "Debug" / ( target + ".exe" ), build / ( target + ".exe" ),
                                    build / "Debug" / target, build / target } )
    {
        if ( fs::is_regular_file( candidate, ec ) ) return candidate;
    }
    return std::nullopt;
}

int RunProcess(
    std::vector<std::string> const& inArgs, std::function<void( std::string const& )> const& inOnLine,
    std::atomic<SDL_Process*>* inHandle ) noexcept
{
    std::vector<char const*> argv;
    for ( auto const& arg : inArgs ) argv.push_back( arg.c_str() );
    argv.push_back( nullptr );

    auto const props = SDL_CreateProperties();
    SDL_SetPointerProperty( props, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, argv.data() );
    SDL_SetNumberProperty( props, SDL_PROP_PROCESS_CREATE_STDIN_NUMBER, SDL_PROCESS_STDIO_NULL );
    SDL_SetNumberProperty( props, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, SDL_PROCESS_STDIO_APP );
    SDL_SetBooleanProperty( props, SDL_PROP_PROCESS_CREATE_STDERR_TO_STDOUT_BOOLEAN, true );
    auto* const process = SDL_CreateProcessWithProperties( props );
    SDL_DestroyProperties( props );

    if ( !process )
    {
        inOnLine( std::string( "could not start " ) + inArgs.front() + ": " + SDL_GetError() );
        return -1;
    }
    if ( inHandle ) inHandle->store( process );

    std::string pending;
    char chunk[4096];
    auto* const output = SDL_GetProcessOutput( process );
    while ( output )
    {
        auto const count = SDL_ReadIO( output, chunk, sizeof( chunk ) );
        if ( count == 0 )
        {
            if ( SDL_GetIOStatus( output ) != SDL_IO_STATUS_NOT_READY ) break;
            SDL_Delay( 10 );
            continue;
        }

        pending.append( chunk, count );
        for ( auto newline = pending.find( '\n' ); newline != std::string::npos; newline = pending.find( '\n' ) )
        {
            auto line = pending.substr( 0, newline );
            if ( !line.empty() && line.back() == '\r' ) line.pop_back();
            if ( !line.empty() ) inOnLine( line );
            pending.erase( 0, newline + 1 );
        }
    }
    if ( !pending.empty() ) inOnLine( pending );

    int exitCode = -1;
    SDL_WaitProcess( process, true, &exitCode );
    if ( inHandle ) inHandle->store( nullptr );
    SDL_DestroyProcess( process );
    return exitCode;
}

void CppBuild::Start(
    fs::path const& inProjectFile, bool inCompile, bool inRun, std::vector<std::string> inConfigureArgs )
{
    if ( m_State.load() != State::Idle )
    {
        AppendBuildOutput( BuildLevel::Warning, "A C++ build or run is already in progress" );
        return;
    }
    if ( m_Thread.joinable() ) m_Thread.join();

    m_State = inCompile ? State::Building : State::Running;
    m_Thread = std::thread( [this, inProjectFile, inCompile, inRun, configureArgs = std::move( inConfigureArgs )]
    {
        bool ok = true;
        if ( inCompile )
        {
            auto const code = CppProjectDir( inProjectFile );
            auto const build = CppBuildDir( inProjectFile );

            AppendBuildOutput( BuildLevel::Info, "Compiling " + code.string() );
            if ( !fs::exists( build / "CMakeCache.txt" ) )
            {
                std::vector<std::string> configure{ "cmake", "-S", code.string(), "-B", build.string() };
                configure.insert( configure.end(), configureArgs.begin(), configureArgs.end() );
                ok = RunProcess( configure, LogToolLine, &m_Process ) == 0;
            }
            if ( ok ) ok = RunProcess( { "cmake", "--build", build.string(), "--config", "Debug" }, LogToolLine, &m_Process ) == 0;

            if ( ok ) AppendBuildOutput( BuildLevel::Info, "Compile succeeded" );
            else AppendBuildOutput( BuildLevel::Error, "Compile failed (is cmake on the PATH, and is a 64-bit toolchain selected?)" );
        }

        if ( ok && inRun )
        {
            m_State = State::Running;
            if ( auto const exe = FindCppExecutable( inProjectFile ) )
            {
                AppendBuildOutput( BuildLevel::Info, "Running " + exe->string() );
                auto const exitCode = RunProcess( { exe->string() }, LogToolLine, &m_Process );
                AppendBuildOutput( BuildLevel::Info, "Game exited with code " + std::to_string( exitCode ) );
            }
            else
            {
                AppendBuildOutput( BuildLevel::Error, "No built game found in " + CppBuildDir( inProjectFile ).string() + " -- compile first" );
            }
        }

        m_State = State::Idle;
    } );
}

CppBuild::~CppBuild()
{
    if ( auto* const process = m_Process.load() ) SDL_KillProcess( process, true );
    if ( m_Thread.joinable() ) m_Thread.join();
}
