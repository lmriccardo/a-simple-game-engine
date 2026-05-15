#include "ThreadPool.hpp"

using namespace asge::concurrent;

void asge::concurrent::_internal::Worker::Run([[maybe_unused]]context_pointer &inCtx)
{
    // Each worker tries to pop jobs from the queue. If the queue
    // is not empty than a job must exists.
    executor_pointer currJob;
    if ( m_JobQueue.TryPop( currJob ) )
    {
        currJob->Execute();
    }
}

asge::concurrent::ThreadPool::ThreadPool(std::size_t nWorkers)
: m_Ctx( WithCancel() )
{
    m_Workers.reserve( nWorkers );
    for ( std::size_t ii = 0; ii < nWorkers; ii++ )
    {
        m_Workers.emplace_back( m_Queue, m_Ctx );
        m_Workers.back().Start();
    }
}

asge::concurrent::ThreadPool::~ThreadPool()
{
    m_Ctx->Cancel( ContextErr::Canceled ); // Cancel the thread context
    for ( auto& worker : m_Workers )
    {
        // Join each worker to end the thread pool
        worker.Join();
    }
}

context_pointer &asge::concurrent::ThreadPool::GetPoolContext() noexcept
{
    return m_Ctx;
}
