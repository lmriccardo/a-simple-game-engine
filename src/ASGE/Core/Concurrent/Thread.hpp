#pragma once

#include <string>
#include <thread>
#include <atomic>
#include <memory>
#include <cstdint>
#include <tuple>
#include <functional>

#include "Context.hpp"

namespace asge::concurrent
{

/**
 * @brief Abstract base class representing a managed, cancellable thread.
 *
 * Wraps a std::thread and provides a structured lifecycle — start, run,
 * cancel, teardown — built around a virtual Run() method that subclasses
 * override to implement their logic. The thread loop itself is managed
 * internally: Run() is called repeatedly until the context is cancelled,
 * keeping subclass implementations focused purely on per-iteration logic.
 *
 * Cancellation is cooperative. Subclasses signal intent to stop by calling
 * Cancel() on the ThreadContext passed to Run(). The loop exits cleanly
 * after the current iteration completes, then TearDown() is invoked.
 *
 * Threads can operate in two modes:
 *  - Joinable: the creating thread can wait for completion via Join().
 *  - Daemon:   the thread is detached and runs independently until cancelled.
 *
 * Each instance is assigned a unique name (defaulting to ASGE_Thread_N)
 * used for identification and debugging. Copy is disabled since a thread
 * represents a unique resource. Move is supported.
 *
 * @warning Every subclass MUST call Cancel() then Join() in its own
 * destructor before returning. ~Thread() also does this, but only as a
 * best-effort safety net — by the time a base-class destructor runs, the
 * vtable has already reverted away from the derived override, so if the
 * background thread is still mid-loop it can dispatch into Run()'s pure
 * virtual slot in Thread and crash ("pure virtual function called").
 * Joining from the most-derived destructor is the only place guaranteed
 * to run before that vtable switch happens.
 */
class Thread
{
    /**
     * @brief The thread body: calls Run() until the context is done.
     */
    void _Run()
    {
        while (!m_Ctx->Done())
        {
            this->Run(m_Ctx);
        }
    }

protected:
    context_pointer              m_Ctx{nullptr};    // Cancellation context driving the loop
    std::unique_ptr<std::thread> m_Thread{nullptr}; // The OS thread, null until started

    std::thread::id m_ThreadId;       // Id of the OS thread once started
    std::string     m_Name;           // Name used for debugging
    bool            m_Daemon {false}; // True once the thread is detached

    inline static std::uint64_t s_ThreadCounter{0}; // Counter for default names

    /**
     * @brief Implements the internal thread logic.
     *
     * Runs inside the thread loop, which is managed outside the concrete thread.
     * Every condition that should end the thread's execution must therefore be
     * handled here; to cancel the thread, call Cancel on the context.
     */
    virtual void Run( context_pointer& ) = 0;

    /**
     * @brief Hook run by Join() after the OS thread has finished.
     *
     * Does nothing by default. It runs on the joining thread, not the worker,
     * and not at all for a daemon (detached) thread.
     */
    virtual void TearDown() {};
public:
    using thread_pointer = std::unique_ptr<Thread>;

    /**
     * @brief Creates a stopped thread named ASGE_Thread_N.
     *
     * Its context is a child of inCtx (Background() if null), so cancelling
     * inCtx stops this thread too. Call Start() to launch it.
     */
    Thread( context_pointer inCtx = nullptr )
    : m_Ctx( WithCancel(inCtx) ), 
      m_Name("ASGE_Thread_" + std::to_string(s_ThreadCounter++))
    {}

    /**
     * @brief Creates a stopped thread with a custom name.
     *
     * inDaemon makes Start() detach the thread right away. Otherwise same as
     * the other constructor.
     */
    Thread(std::string const& inName,
        context_pointer inCtx=nullptr, bool inDaemon=false)
    : Thread( inCtx )
    {
        m_Name = inName;
        m_Daemon = inDaemon;
        s_ThreadCounter++;
    }

    /* We need to delete copy constructor and copy assignment */
    Thread(Thread const&) = delete;

    /**
     * @brief Copy assignment is disabled.
     */
    Thread& operator=(Thread const&) = delete;

    /**
     * @brief Move constructor.
     *
     * Defaulted; moves each member.
     */
    Thread(Thread&&) = default;

    /**
     * @brief Move assignment.
     *
     * Defaulted; assigns each member.
     */
    Thread& operator=(Thread&&) = default;

    /**
     * @brief Cancels and joins the thread as a last-resort safety net.
     *
     * Subclasses must do this themselves, see the class warning; the base
     * destructor asserts in debug builds if the thread is still joinable.
     */
    virtual ~Thread();

    /**
     * @brief Checks if the thread is joinable
     * 
     * A thread is joinable if it has started, if the internal
     * pointer is not a null pointer (i.e., if the thread has
     * been defined) and if the internal thread is joinable (
     * meaning that it is not a daemon).
     */
    bool Joinable() const noexcept;

    /**
     * @brief True once the thread is detached (a daemon).
     */
    bool Daemon() const noexcept;

    /**
     * @brief True once Start() created the OS thread.
     */
    bool Started() const noexcept;

    /**
     * @brief The thread's name, used for identification and debugging.
     */
    const std::string& GetName() const noexcept;

    /**
     * @brief The OS thread id; a default id until the thread is started.
     */
    std::thread::id GetThreadId() const noexcept;

    /**
     * @brief Starts and create a new thread from an input function
     * 
     * The function is called repeatedly until the context is cancelled. It
     * receives the thread's context_pointer as first argument, followed by the
     * given arguments. inDaemon detaches the thread right after it starts.
     * Returns the owning pointer; keep it alive for as long as the thread runs.
     */
    template<typename _Callable, typename... _Args>
    static thread_pointer Start(bool, context_pointer, _Callable&&, _Args&& ...);

    /**
     * @brief Waits for the thread to finish, then runs TearDown().
     *
     * Does nothing if the thread was never started, is detached, or was
     * already joined. It does not cancel: call Cancel() first or this blocks.
     */
    void Join();

    /**
     * @brief Detaches the running thread, marking it a daemon.
     *
     * A no-op if the thread was never started, is already detached, or has
     * already been joined. Safe to call regardless of whether the running
     * thread's work has finished -- detach() only requires the underlying
     * std::thread to still be joinable, not still executing.
     */
    void Detach();

    /**
     * @brief Launches the OS thread and starts the Run() loop.
     *
     * Does nothing if already started. A daemon thread is detached right away.
     */
    void Start();

    /**
     * @brief Asks the thread to stop by cancelling its context.
     *
     * Cooperative: the loop exits after the current Run() returns. Does
     * nothing if the thread was never started or the context is already done.
     */
    void Cancel();
};

namespace _internal
{

/**
 * @brief Thread implementation that runs a stored callable on each loop pass.
 *
 * Takes a function with all its parameters and implements Run() by calling
 * it with the thread context first. Its only purpose is to back the static
 * Thread::Start; use that instead of creating a ThreadImpl directly.
 */
template<typename _Callable, typename ..._Args>
class ThreadImpl : public Thread
{
private:
    _Callable            m_Func; // Callable function
    std::tuple<_Args...> m_Args; // Arguments to the callable

    /**
     * @brief Calls the stored function with inCtx followed by the stored arguments.
     */
    void Run( context_pointer& inCtx ) override
    {
        std::apply( m_Func, std::tuple_cat(
            std::tie(inCtx),          // context is always first
            m_Args
        ));
    }
public:
    /**
     * @brief Stores inFunc and inArgs; the thread is started by Thread::Start.
     */
    ThreadImpl( context_pointer inCtx, _Callable&& inFunc, _Args&&... inArgs )
    : Thread(inCtx), m_Func( std::forward<_Callable>(inFunc) ),
      m_Args( std::forward<_Args>(inArgs)... )
    {}

    /**
     * @brief Cancels and joins the thread.
     *
     * Must happen here, not in ~Thread(); see the @warning on Thread's class comment.
     */
    ~ThreadImpl() override { Cancel(); Join(); }
};

}

using thread_pointer = Thread::thread_pointer;

template <typename _Callable, typename... _Args>
inline thread_pointer Thread::Start(bool inDaemon,
    context_pointer inCtx, _Callable && inFunc, _Args &&...inArgs)
{
    using tImpl = _internal::ThreadImpl<_Callable, _Args...>;
    thread_pointer cThread = std::make_unique<tImpl>(
        inCtx, std::forward<_Callable>(inFunc), std::forward<_Args>(inArgs)...);
    
    cThread->Start();
    if (inDaemon) cThread->Detach();
    return cThread;
}

/**
 * @brief Get the hardware concurrency of the current system.
 *
 * Wraps std::thread::hardware_concurrency(), so it may return 0 when the
 * platform cannot tell.
 */
inline std::size_t HardwareConcurrency() noexcept 
{ 
    return std::thread::hardware_concurrency(); 
}

}