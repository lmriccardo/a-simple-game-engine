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
    void _Run()
    {
        while (!m_Ctx->Done())
        {
            this->Run(m_Ctx);
        }
    }

protected:
    context_pointer              m_Ctx{nullptr};
    std::unique_ptr<std::thread> m_Thread{nullptr};

    std::thread::id m_ThreadId;
    std::string     m_Name;
    bool            m_Daemon {false};

    inline static std::uint64_t s_ThreadCounter{0};

    /**
     * @brief Implements the internal thread logic
     * 
     * This method must implement only the internal thread logic
     * inside the thread loop, which is instead managed outside
     * the particular thread implementation. Therefore, all other
     * conditions that might be return the thread execution must
     * be specified in this method. To cancel the thread, the Cancel
     * function on the context shall be called.
     */
    virtual void Run( context_pointer& ) = 0;

    // Functions executed when the thread is cancelled
    virtual void TearDown() {};
public:
    using thread_pointer = std::unique_ptr<Thread>;

    Thread( context_pointer inCtx = nullptr )
    : m_Ctx( WithCancel(inCtx) ), 
      m_Name("ASGE_Thread_" + std::to_string(s_ThreadCounter++))
    {}

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
    Thread& operator=(Thread const&) = delete;

    Thread(Thread&&) = default;
    Thread& operator=(Thread&&) = default;

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

    bool Daemon() const noexcept;
    bool Started() const noexcept;

    const std::string& GetName() const noexcept;
    std::thread::id GetThreadId() const noexcept;

    /**
     * @brief Starts and create a new thread from an input function
     * 
     * The input function can take a variadic number of input parameters,
     * but the first input parameter must always be a ThreadContext
     * reference.
     */
    template<typename _Callable, typename... _Args>
    static thread_pointer Start(bool, context_pointer, _Callable&&, _Args&& ...);

    void Join();
    void Detach();
    void Start();
    void Cancel();
};

namespace _internal
{

/**
 * This is a thread implementation for simple thread logic. It takes as input
 * a function with all parameters and just implements the run method. I would
 * like to point out that the only purpouse of this class is to use it as a
 * middle point in the static "start" method of the Thread class, therefore
 * it is suggested to not use it to create generic threads. Just use the "start"
 * method of Thread class.
 * 
 * @tparam _Callable The function type
 * @tparam _Args     Types for each function parameter
 */
template<typename _Callable, typename ..._Args>
class ThreadImpl : public Thread
{
private:
    _Callable            m_Func; // Callable function
    std::tuple<_Args...> m_Args; // Arguments to the callable

    void Run( context_pointer& inCtx ) override
    {
        std::apply( m_Func, std::tuple_cat(
            std::tie(inCtx),          // context is always first
            m_Args
        ));
    }
public:
    ThreadImpl( context_pointer inCtx, _Callable&& inFunc, _Args&&... inArgs )
    : Thread(inCtx), m_Func( std::forward<_Callable>(inFunc) ),
      m_Args( std::forward<_Args>(inArgs)... )
    {}

    // Must Cancel()+Join() here, not rely on ~Thread() -- see the
    // @warning on Thread's class doc comment.
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

/* Get the hardware concurrency of the current system */
inline std::size_t HardwareConcurrency() noexcept 
{ 
    return std::thread::hardware_concurrency(); 
}

}