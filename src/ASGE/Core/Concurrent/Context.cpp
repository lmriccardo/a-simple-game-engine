#include "Context.hpp"

using namespace asge::concurrent;

std::string asge::concurrent::CtxErrStr(ContextErr inErr) noexcept
{
    switch (inErr)
    {
    case ContextErr::Nil: return "NIL";
    case ContextErr::Canceled: return "CANCELED";
    case ContextErr::Deadline: return "Deadline Exceeded";
    default:
        return "Unkown context error";
    }
}

void asge::concurrent::_internal::CancelState::Cancel(ContextErr inErr) noexcept
{
    if ( !s_CancelDone.exchange( true, std::memory_order_acq_rel ) )
    {
        s_Error.store( inErr, std::memory_order_release );
        s_Cv.notify_all();
    }
}

ContextErr asge::concurrent::_internal::CancelState::Err() const noexcept
{
    return s_Error.load( std::memory_order_acquire );
}

void asge::concurrent::Context::Attach()
{
    // Automatically cancel the child if the parent is canceled
    if ( m_Parent->Err() != ContextErr::Nil )
    {
        Cancel( m_Parent->Err() );
    }
    else
    {
        // If the parent was not canceled when we can add child to it
        m_Parent->AddChild( shared_from_this() );
    }
}

asge::concurrent::Context::Context(context_pointer inParent)
    : m_Parent(inParent ? inParent : Background())
{
}

void asge::concurrent::Context::AddChild(context_pointer const &inChild) noexcept
{
    std::lock_guard<std::mutex> lock( m_Mtx );
    m_Children.push_back( inChild );
}

void asge::concurrent::Context::Cancel(ContextErr inErr)
{
    // Check if the current context has not yet been canceled
    if ( Err() != ContextErr::Nil ) return;

    // Cancel the current state of the context and all the children
    m_State.Cancel( inErr );

    // Lock to get a copy of all children
    children copyChildren;
    {
        std::scoped_lock lock( m_Mtx );
        copyChildren = m_Children;
    }

    // For each cancellable child we must cancel it. We can check if 
    // a child is cancellable by casting it to ICancellableContext
    for ( auto& children_lifetime_t: copyChildren )
    {
        if ( auto curr_child = children_lifetime_t.lock() )
        {
            curr_child->Cancel( inErr );
        }
    } 
}

bool asge::concurrent::Context::Done() const noexcept
{
    return m_State.s_CancelDone.load( std::memory_order_acquire ) || m_Parent->Done();
}

void asge::concurrent::Context::Wait() const
{
    if ( Done() ) return;
    std::unique_lock<std::mutex> lock( m_State.s_Mtx );
    m_State.s_Cv.wait( lock, [this](){ return Done(); } );
}

ContextErr asge::concurrent::Context::Err() const noexcept
{
    // In this case, if the context has been canceled, then the
    // cancel error must be returned, otherwise nil is the way.
    // The value of this variable is inherently changed also if
    // a parent context is canceled.
    return m_State.Err();
}

void asge::concurrent::Context::Cancel()
{
    Cancel( ContextErr::Canceled );
}

context_pointer asge::concurrent::Background() noexcept
{
    static context_pointer ec = std::shared_ptr<EmptyContext>( new EmptyContext() );
    return ec;
}

asge::concurrent::Context::pointer 
asge::concurrent::WithCancel(context_pointer inParent)
{
    auto ctx = std::shared_ptr<Context>( new Context( inParent ) );
    ctx->Attach();
    return ctx;
}
