#pragma once

#include <atomic>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

struct SDL_Process;

/** @brief `<project folder>/code/build`: where the generated C++ project is configured and built. */
[[nodiscard]] std::filesystem::path CppBuildDir( std::filesystem::path const& inProjectFile ) noexcept;

/** @brief The built game's executable inside CppBuildDir (Debug configuration first), if it exists. */
[[nodiscard]] std::optional<std::filesystem::path> FindCppExecutable( std::filesystem::path const& inProjectFile ) noexcept;

/**
 * @brief Runs inArgs (program first, found through PATH), calling inOnLine for every line of its
 *        combined stdout/stderr as it arrives. Returns the exit code, or -1 if it could not start.
 *        If inHandle is given it holds the live process while it runs, so another thread can kill it.
 */
int RunProcess(
    std::vector<std::string> const& inArgs, std::function<void( std::string const& )> const& inOnLine,
    std::atomic<SDL_Process*>* inHandle = nullptr ) noexcept;

/**
 * @brief Builds and/or runs a project's generated C++ game on a worker thread, sending the tools'
 *        output to the editor log. One job at a time; a request made while busy is refused.
 */
class CppBuild
{
public:
    enum class State { Idle, Building, Running };

private:
    std::thread              m_Thread;
    std::atomic<State>       m_State{ State::Idle };
    std::atomic<SDL_Process*> m_Process{ nullptr };

    void Start(
        std::filesystem::path const& inProjectFile, bool inCompile, bool inRun,
        std::vector<std::string> inConfigureArgs );

public:
    CppBuild() = default;
    CppBuild( CppBuild const& ) = delete;
    CppBuild& operator=( CppBuild const& ) = delete;

    /** @brief Kills whatever is still running and waits for the worker thread. */
    ~CppBuild();

    /** @brief What the worker is doing right now. */
    [[nodiscard]] State GetState() const noexcept { return m_State.load(); }

    /**
     * @brief Configures (first time only) and builds the game in the Debug configuration.
     *        inConfigureArgs are extra `cmake -S -B` arguments (a Toolchain's), used only when
     *        the build folder does not exist yet.
     */
    void Compile( std::filesystem::path const& inProjectFile, std::vector<std::string> inConfigureArgs = {} )
    {
        Start( inProjectFile, true, false, std::move( inConfigureArgs ) );
    }

    /** @brief Launches the last built game. */
    void Run( std::filesystem::path const& inProjectFile ) { Start( inProjectFile, false, true, {} ); }

    /** @brief Compile(), then Run() only if the build succeeded. */
    void CompileAndRun( std::filesystem::path const& inProjectFile, std::vector<std::string> inConfigureArgs = {} )
    {
        Start( inProjectFile, true, true, std::move( inConfigureArgs ) );
    }
};
