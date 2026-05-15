#pragma once

#include "Thread.hpp"
#include "Queues/MPMCQueue.hpp"
#include "Context.hpp"

#include <future>
#include <memory>
#include <cstdint>
#include <vector>
#include <type_traits>

#include <ASGE/Core/Functools.hpp>

#define MAX_JOB_QUEUE_SIZE 1024

namespace asge::concurrent
{

namespace _internal
{

struct IExecuter
{
    virtual ~IExecuter() = default;
    virtual void Execute() = 0;
};

using executor_pointer = std::unique_ptr<IExecuter>;
using job_queue = MPMCQueue<MAX_JOB_QUEUE_SIZE, executor_pointer>;

/**
 * @brief A type-erased, executable unit of work that optionally produces a future value.
 *
 * WorkerJob wraps a callable and its bound arguments into a concrete implementation
 * of IExecuter, allowing it to be queued and dispatched by the thread pool without
 * the pool itself needing to know the callable's type or return type.
 *
 * It inherits from functools::Callable to manage the callable and its arguments,
 * and from IExecuter to expose a uniform Execute() interface to the pool's workers.
 *
 * Internally, a std::promise is used to communicate the result back to the caller.
 * If the callable returns void, the promise is left untouched and no future value
 * is produced. Otherwise, the return value is set on the promise at execution time
 * and can be retrieved via the std::future returned by GetFuture().
 *
 * @note GetFuture() must be called before the job is submitted to the queue,
 *       since once ownership is transferred via unique_ptr the job is no longer
 *       accessible from the submitting thread.
 *
 * @tparam _Callable The type of the callable to be executed.
 * @tparam _Args     The types of the arguments bound to the callable.
 */
template<typename _Callable, typename... _Args>
class WorkerJob : public IExecuter, 
                  public functools::Callable<_Callable, _Args...>
{
private:
    using CallableBase = functools::Callable<_Callable, _Args...>;
    using return_type  = typename CallableBase::return_type;

    std::promise<return_type> m_RetPromise;
public:
    using future_type = std::future<return_type>;
    using CallableBase::CallableBase;
    using CallableBase::operator=;
    
    ~WorkerJob() override = default;

    future_type GetFuture()
    {
        return m_RetPromise.get_future();
    }

    void Execute() override
    {
        // Here we need to check if the return type is void. If it
        // is void then we do not need to set anything inside the promise.
        if constexpr ( std::is_void_v<return_type> ) {
            this->Call();
        } else {
            m_RetPromise.set_value( this->Call() );
        }
    }
};

// Added deduction guide for WorkerJob constructor
template<typename _Callable, typename... _Args>
WorkerJob(_Callable&&, _Args&&...) 
-> WorkerJob<std::decay_t<_Callable>, std::decay_t<_Args>...>;

/**
 * @brief A thread pool worker that continuously drains jobs from a shared queue.
 *
 * Worker extends Thread with a single responsibility: loop over a shared job_queue,
 * pop executor_pointer entries as they become available, and call Execute() on each.
 * It holds no ownership over the queue — only a reference — so the ThreadPool that
 * owns both the queue and the workers must ensure the queue outlives all workers.
 *
 * Each Worker is assigned a unique human-readable name ("Worker_1", "Worker_2", ...)
 * via a static counter, which aids debugging and profiling.
 *
 * Workers are move-constructible to allow storage in a std::vector, but are
 * non-copyable since they hold a mutable queue reference and manage a live thread.
 *
 * @note The static ID counter (s_Id) is not thread-safe. Workers are expected
 *       to be constructed sequentially during ThreadPool initialisation, not
 *       concurrently.
 *
 * @see ThreadPool  — owns the queue and the worker vector.
 * @see IExecuter   — the interface each dequeued job implements.
 */
class Worker : public Thread
{
private:
    inline static std::size_t s_Id{0};
    
    using job_queue_ref = job_queue&;
    job_queue_ref m_JobQueue;

    void Run(context_pointer& inCtx) override;
public:
    Worker( job_queue_ref inQueue, context_pointer inCtx ) 
    : Thread( "Worker_" + std::to_string(++s_Id), inCtx )
    , m_JobQueue( inQueue )
    {}

    Worker(Worker&&) = default;
    Worker& operator=(Worker&&) = default;

    Worker(Worker const&) = delete;
    Worker& operator=(Worker const&) = delete;

    ~Worker() override = default;
};

template<typename _Callable>
using future_or_void_t = std::conditional_t<
    std::is_void_v<functools::_internal::return_type_t<_Callable>>, void,
    std::future<functools::_internal::return_type_t<_Callable>>
>;

}

/**
 * @brief A fixed-size thread pool that executes submitted jobs concurrently.
 *
 * ThreadPool manages a collection of Worker threads and a shared lock-free MPMC
 * queue. Callers submit work via Submit(), which wraps the callable and its
 * arguments into a WorkerJob, enqueues it, and returns either a std::future<T>
 * (if the callable has a non-void return type) or nothing (if it returns void).
 *
 * The pool is intentionally non-movable and non-copyable: it owns both the queue
 * and the workers, and Workers hold a reference to the queue, so relocating the
 * pool would silently invalidate those references.
 *
 * Lifetime: the destructor is responsible for signalling workers to stop and
 * joining all threads before the queue is destroyed.
 *
 * @note The default worker count is HardwareConcurrency(), which maps to
 *       std::thread::hardware_concurrency(). Consider passing an explicit count
 *       for latency-sensitive or IO-heavy workloads.
 *
 * @see Worker      — consumes jobs from the queue.
 * @see WorkerJob   — wraps a callable into an IExecuter.
 */
class ThreadPool
{
private:
    _internal::job_queue           m_Queue;   // The job queue to submit jobs
    std::vector<_internal::Worker> m_Workers; // Vectors of all workers
    context_pointer                m_Ctx;     // The parent context of the entire pool
public:
    ThreadPool( std::size_t nWorkers = HardwareConcurrency() );

    ThreadPool( ThreadPool&& )      = delete;
    ThreadPool( ThreadPool const& ) = delete;

    ThreadPool& operator=( ThreadPool const& ) = delete;
    ThreadPool& operator=( ThreadPool&& )      = delete;

    ~ThreadPool();

    /**
     * @brief Submits a work to the Thread Pool
     * 
     * The works cames into the format of a callable with respective arguments.
     * It is then converted into a Worker Job and put inside the queue, which will
     * be eventually executed by one of the workers. If the input callable also
     * presents a return value, then a std::future<T> is returned, void otherwise.
     * 
     * @param inFunc The input function
     * @param inArgs Arguments to be passed to the function
     * 
     * @tparam _Callable The input function type
     * @tparam _Args The input variadic arguments types pack
     */
    template<typename _Callable, typename ..._Args>
    auto Submit( _Callable&& inFunc, _Args&& ...inArgs ) 
    -> _internal::future_or_void_t<_Callable>
    {
        auto jobPtr = std::make_unique<_internal::WorkerJob<_Callable, _Args...>>(
            std::forward<_Callable>( inFunc ), std::forward<_Args>(inArgs)...
        );

        // If the return type is not void then we need to return the future
        if constexpr ( !std::is_void_v<functools::_internal::return_type_t<_Callable>> )
        {
            auto retFuture = jobPtr->GetFuture();
            m_Queue.TryPush( std::move( jobPtr ) );
            return retFuture;
        } 
        else
        {
            m_Queue.TryPush( std::move( jobPtr ) );
        }
    }
    
    /* Returns the root thread pool context to create new contexts */
    context_pointer& GetPoolContext() noexcept;
};

}