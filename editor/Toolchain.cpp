#include "Toolchain.hpp"

#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <sstream>

#include <SDL3/SDL_stdinc.h>

#include "CppBuild.hpp"

namespace fs = std::filesystem;

namespace
{

/** @brief Every output line of inArgs' program, joined with newlines ("" if it could not run). */
std::string Capture( std::vector<std::string> const& inArgs, int* outExitCode = nullptr )
{
    std::string text;
    auto const code = RunProcess( inArgs, [&]( std::string const& inLine ) { text += inLine + "\n"; } );
    if ( outExitCode ) *outExitCode = code;
    return text;
}

std::string FirstLine( std::string const& inText )
{
    return inText.substr( 0, inText.find( '\n' ) );
}

#ifdef _WIN32

/** @brief Visual Studio installations that have the C++ x64 tools, as x64 toolchains. */
void ScanVisualStudio( std::vector<Toolchain>& outToolchains )
{
    char const* const programFiles = SDL_getenv( "ProgramFiles(x86)" );
    if ( !programFiles ) return;

    auto const vswhere = fs::path( programFiles ) / "Microsoft Visual Studio" / "Installer" / "vswhere.exe";
    if ( !fs::exists( vswhere ) ) return;

    auto const installs = ParseVsWhere( Capture( {
        vswhere.string(), "-all", "-products", "*", "-requires", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64" } ) );
    auto const capabilities = Capture( { "cmake", "-E", "capabilities" } );

    for ( auto const& install : installs )
    {
        auto const generator = VsGenerator( install.m_Version );
        if ( !generator ) continue;

        Toolchain toolchain;
        toolchain.m_Id = *generator + "|x64";
        toolchain.m_Name = install.m_DisplayName + " (x64)";
        toolchain.m_ConfigureArgs = { "-G", *generator, "-A", "x64" };
        if ( capabilities.find( "\"" + *generator + "\"" ) == std::string::npos )
        {
            toolchain.m_Usable = false;
            toolchain.m_Reason = "the installed CMake does not know the \"" + *generator + "\" generator";
        }
        outToolchains.push_back( std::move( toolchain ) );
    }
}

#else

/** @brief g++ and clang++ from the PATH, with Ninja if it is there and Unix Makefiles otherwise. */
void ScanCompilers( std::vector<Toolchain>& outToolchains )
{
    int ninjaExit = -1;
    Capture( { "ninja", "--version" }, &ninjaExit );
    std::string const generator = ninjaExit == 0 ? "Ninja" : "Unix Makefiles";

    struct Candidate { char const* m_Program; char const* m_Label; int m_MinMajor; };
    for ( auto const& candidate : { Candidate{ "g++", "GCC", 13 }, Candidate{ "clang++", "Clang", 16 } } )
    {
        int exitCode = -1;
        auto const line = FirstLine( Capture( { candidate.m_Program, "--version" }, &exitCode ) );
        if ( exitCode != 0 ) continue;

        auto const major = CompilerMajorVersion( line );
        Toolchain toolchain;
        toolchain.m_Id = generator + "|" + candidate.m_Program;
        toolchain.m_Name = std::string( candidate.m_Label ) + " " + ( major ? std::to_string( *major ) : "?" )
                         + " (" + candidate.m_Program + ", " + generator + ")";
        toolchain.m_ConfigureArgs = { "-G", generator, std::string( "-DCMAKE_CXX_COMPILER=" ) + candidate.m_Program,
                                      "-DCMAKE_BUILD_TYPE=Debug" };
        if ( !major || *major < candidate.m_MinMajor )
        {
            toolchain.m_Usable = false;
            toolchain.m_Reason = std::string( "ASGE needs " ) + candidate.m_Label + " " + std::to_string( candidate.m_MinMajor )
                               + " or newer (C++23)";
        }
        outToolchains.push_back( std::move( toolchain ) );
    }
}

#endif

}

std::vector<VsInstall> ParseVsWhere( std::string const& inText )
{
    std::vector<VsInstall> installs;
    std::istringstream stream( inText );
    for ( std::string line; std::getline( stream, line ); )
    {
        if ( !line.empty() && line.back() == '\r' ) line.pop_back();

        auto const colon = line.find( ": " );
        if ( colon == std::string::npos ) continue;

        auto const key = line.substr( 0, colon );
        auto const value = line.substr( colon + 2 );
        if ( key == "instanceId" ) installs.emplace_back();
        else if ( installs.empty() ) continue;
        else if ( key == "installationVersion" ) installs.back().m_Version = value;
        else if ( key == "displayName" ) installs.back().m_DisplayName = value;
    }

    std::erase_if( installs, []( VsInstall const& inInstall ) { return inInstall.m_Version.empty(); } );
    return installs;
}

std::optional<std::string> VsGenerator( std::string const& inVersion )
{
    auto const major = std::atoi( inVersion.c_str() );
    switch ( major )
    {
    case 18: return "Visual Studio 18 2026";
    case 17: return "Visual Studio 17 2022";
    case 16: return "Visual Studio 16 2019";
    default: return std::nullopt;
    }
}

std::optional<int> CompilerMajorVersion( std::string const& inVersionLine )
{
    auto start = inVersionLine.find( "version " );
    if ( start != std::string::npos ) start += 8;
    else
    {
        start = inVersionLine.find_last_of( ' ' );
        if ( start == std::string::npos ) return std::nullopt;
        ++start;
    }

    if ( start >= inVersionLine.size() || !std::isdigit( static_cast<unsigned char>( inVersionLine[start] ) ) )
    {
        return std::nullopt;
    }
    return std::atoi( inVersionLine.c_str() + start );
}

std::vector<Toolchain> ScanToolchains()
{
    std::vector<Toolchain> toolchains;
#ifdef _WIN32
    ScanVisualStudio( toolchains );
#else
    ScanCompilers( toolchains );
#endif
    return toolchains;
}
