#include "Thread.hpp"

using namespace asge::concurrent;

asge::concurrent::Thread::~Thread()
{
    Cancel();
    Join();
}

bool asge::concurrent::Thread::Joinable() const noexcept
{
    return m_Thread != nullptr && !m_Daemon && m_Thread->joinable();
}

bool asge::concurrent::Thread::Daemon() const noexcept
{
    return m_Daemon;
}

bool asge::concurrent::Thread::Started() const noexcept
{
    return m_Started.load(std::memory_order_relaxed);
}

const std::string &asge::concurrent::Thread::GetName() const noexcept
{
    return m_Name;
}

std::thread::id asge::concurrent::Thread::GetThreadId() const noexcept
{
    return m_ThreadId;
}

void asge::concurrent::Thread::Join()
{
    if ( Joinable() )
    {
        m_Thread->join();
        TearDown();
    }
}

void asge::concurrent::Thread::Detach()
{
    if ( !m_Ctx->Done() && m_Thread != nullptr )
    {
        m_Thread->detach();
        m_Daemon = true;
    }
}

void asge::concurrent::Thread::Start()
{
    if ( !Started() )
    {
        m_Thread = std::make_unique<std::thread>(&Thread::_Run, this);
        m_ThreadId = m_Thread->get_id();
        m_Started.store( true, std::memory_order_relaxed );

        // Detach if the thread is a deamon
        if (Daemon()) Detach();
    }
}

void asge::concurrent::Thread::Cancel()
{
    if ( m_Thread != nullptr && !m_Ctx->Done() && Started() )
    {
        m_Ctx->Cancel( ContextErr::Canceled );
    }
}
