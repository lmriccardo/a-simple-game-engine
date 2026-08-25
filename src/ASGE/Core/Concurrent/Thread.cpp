#include "Thread.hpp"
#include <ASGE/Core/Errors.hpp>

using namespace asge::concurrent;

asge::concurrent::Thread::~Thread()
{
    // A subclass reaching here still Joinable() means its own destructor
    // didn't Cancel()+Join() as required (see Thread's class doc comment)
    // -- the Cancel()+Join() below is only a best-effort fallback, since
    // by now the vtable has already reverted away from the derived Run()
    // override and the background thread can crash into it mid-dispatch.
    ASGE_ASSERT(!Joinable(),
        "Thread subclass destroyed without Cancel()+Join() in its own "
        "destructor -- see the @warning on Thread's class doc comment");

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
    return m_Thread != nullptr;
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
