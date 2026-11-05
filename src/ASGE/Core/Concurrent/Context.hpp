#pragma once

#include <string>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <vector>

namespace asge::concurrent
{

enum class ContextErr
{
    Nil,        // Is the error returned when non of the other happened
    Canceled,   // Context canceled for some reason rather then deadline
    Deadline    // Context canceled due to its deadline passing
};

/**
 * @brief Converts the Context Error enum into a string representation.
 *
 * Gives "NIL", "CANCELED" or "Deadline Exceeded"; anything else yields
 * a generic "unknown" text. Meant for logs.
 */
std::string CtxErrStr( ContextErr inErr ) noexcept;

namespace _internal
{

/* Cancel state used for the context to close */
struct CancelState
{
    std::atomic_bool                s_CancelDone{false}; // True once cancelled
    mutable std::mutex              s_Mtx;               // Guards the wait on s_Cv
    mutable std::condition_variable s_Cv;                // Wakes waiters on cancel

    /**
     * @brief Marks the state cancelled with inErr and wakes all waiters.
     *
     * Only the first call has an effect; later calls keep the original error.
     */
    void Cancel( ContextErr inErr ) noexcept;

    /**
     * @brief The error stored by Cancel(), or ContextErr::Nil if not cancelled.
     */
    ContextErr Err() const noexcept;
private:
    std::atomic<ContextErr> s_Error{ContextErr::Nil}; // Why the state was cancelled
};

struct IContext
{
    using pointer = std::shared_ptr<IContext>;

    /**
     * @brief Virtual destructor so derived types are destroyed through the interface.
     */
    virtual ~IContext() = default;

    /**
     * @brief True once this context, or any ancestor, has been cancelled.
     */
    virtual bool Done() const noexcept = 0;

    /**
     * @brief Blocks the calling thread until the context is done.
     */
    virtual void Wait() const = 0;

    /**
     * @brief Why the context was cancelled, or ContextErr::Nil if it was not.
     */
    virtual ContextErr Err() const noexcept = 0;

    /**
     * @brief Registers a child to be cancelled together with this context.
     *
     * Children are held weakly, so registering does not keep them alive.
     */
    virtual void AddChild( pointer const& ) noexcept = 0;

    /**
     * @brief Cancels this context and propagates the error to its children.
     */
    virtual void Cancel( ContextErr ) = 0;
};

struct ContextWithValue {};

}

using context_pointer = _internal::IContext::pointer;

/**
 * @brief An empty context that is never canceled.
 *
 * Useful as the top-level context for cancellable contexts. Only
 * Background() creates it.
 */
class EmptyContext final : public _internal::IContext 
{
    friend context_pointer Background() noexcept;

    /**
     * @brief Default constructor, private.
     *
     * Only Background() creates an EmptyContext.
     */
    EmptyContext() = default;

    /**
     * @brief Does nothing: the empty context cannot be cancelled.
     *
     * Private so callers cannot try.
     */
    inline void Cancel( ContextErr ) override {}
public:
    /**
     * @brief Always false: this context is never cancelled.
     */
    inline bool Done() const noexcept { return false; }

    /**
     * @brief Returns immediately.
     *
     * Not blocking is a deliberate choice: waiting on a context that can
     * never end would hang forever.
     */
    inline void Wait() const {}

    /**
     * @brief Always ContextErr::Nil.
     */
    inline ContextErr Err() const noexcept { return ContextErr::Nil; }

    /**
     * @brief Ignores the child, since there is never a cancellation to propagate.
     */
    inline void AddChild( pointer const& ) noexcept {}

    /**
     * @brief Copy constructor.
     *
     * Defaulted; copies each member.
     */
    EmptyContext( EmptyContext const& ) = default;

    /**
     * @brief Move constructor.
     *
     * Defaulted; moves each member.
     */
    EmptyContext( EmptyContext&& ) = default;

    /**
     * @brief Copy assignment.
     *
     * Defaulted; assigns each member.
     */
    EmptyContext& operator=( EmptyContext const& ) = default;

    /**
     * @brief Move assignment.
     *
     * Defaulted; assigns each member.
     */
    EmptyContext& operator=( EmptyContext&& ) = default;
};

/**
 * @brief A cancellable context that propagates cancellation through a tree.
 *
 * Context is the core building block of the cancellation system. Each context
 * is a node in a tree — when a context is cancelled, the cancellation propagates
 * automatically to all of its children, and transitively to their children.
 *
 * A Context is never constructed directly. Instead it is created through the
 * WithCancel() factory function, which derives a child from an existing parent.
 * The root of any context tree is always the Background() context.
 *
 * Cancellation is cooperative — a context does not forcibly stop any thread.
 * Code that receives a context is expected to poll Done() or block on Wait()
 * at safe points and exit cleanly when the context is cancelled.
 *
 * Context is non-copyable and non-movable. It must always be heap-allocated
 * and shared via Context::pointer (shared_ptr<Context>), which is what the
 * factory functions return. The read-only interface (context_pointer) can be
 * handed to consumers that should observe but never cancel the context.
 */
class Context : public _internal::IContext,
                public std::enable_shared_from_this<Context>
{
protected:
    using child_t  = std::weak_ptr<_internal::IContext>;
    using children = std::vector<child_t>;

    mutable std::mutex     m_Mtx;      // Guards the children list
    _internal::CancelState m_State;    // Cancellation state for context lifetime
    children               m_Children; // Children of the current context

    context_pointer m_Parent{nullptr}; // Parent of the context (if any)

    /**
     * @brief Creates a context under inParent (Background() if null).
     *
     * Not attached to the parent yet; the factory functions call Attach()
     * once the object is owned by a shared_ptr.
     */
    explicit Context( context_pointer inParent );

    /**
     * @brief Links this context to its parent.
     *
     * If the parent is already cancelled the child is cancelled right away
     * with the same error; otherwise it is registered so a later cancel of
     * the parent reaches it. Needs shared_from_this(), so it cannot run in
     * the constructor.
     */
    void Attach();
private:
    /**
     * @brief Adds inChild (held weakly) to the list cancelled with this context.
     */
    void AddChild( context_pointer const& inChild ) noexcept override;

    /**
     * @brief Cancels this context with inErr, then every live child.
     *
     * Does nothing if already cancelled. The child list is copied under the
     * lock and cancelled outside it, so child callbacks cannot deadlock on it.
     */
    void Cancel( ContextErr inErr ) override;
public:
    using pointer = std::shared_ptr<Context>;

    /**
     * @brief Copy construction is disabled.
     */
    Context( Context const& )            = delete;

    /**
     * @brief Copy assignment is disabled.
     */
    Context& operator=( Context const& ) = delete;

    /**
     * @brief Move construction is disabled.
     */
    Context( Context&& )                 = delete;

    /**
     * @brief Move assignment is disabled.
     */
    Context& operator=( Context&& )      = delete;

    /**
     * @brief Checks if the current context has finished or not.
     *
     * True once this context or any ancestor was cancelled.
     */
    bool Done() const noexcept;

    /**
     * @brief Waits until the context is cancelled.
     *
     * Blocks on a condition variable, and returns at once if already done.
     */
    void Wait() const;

    /**
     * @brief Returns the canceling error.
     *
     * ContextErr::Nil until this context itself is cancelled.
     */
    ContextErr Err() const noexcept;

    /**
     * @brief Cancels the current context with ContextErr::Canceled.
     *
     * Propagates to all children, and is harmless if already cancelled.
     */
    void Cancel();
private:
    friend pointer WithCancel( context_pointer );
};

template<typename T>
class ContextWithValue;

template<typename T>
std::shared_ptr<ContextWithValue<T>> WithValue( T inValue, context_pointer inParent = nullptr );

/**
 * @brief A context that carries an immutable typed value alongside cancellation.
 *
 * ContextWithValue extends Context with a single value of type T that is set
 * at construction and never changes. It inherits the full cancellation and tree
 * propagation behaviour of Context — cancelling it behaves identically to
 * cancelling a plain Context.
 * 
 * Created exclusively through WithValue(), never constructed directly.
 * T is the type of the carried value and must be move-constructible.
 */
template<typename T>
class ContextWithValue final : public Context
{
private:
    T m_Value; // The value held by the context ( immutable by definition )

    /**
     * @brief Creates the context under inParent holding inValue; used by WithValue().
     */
    explicit ContextWithValue( context_pointer inParent, T inValue )
    : Context( inParent ), m_Value( inValue )
    {}
public:
    using value_type      = T;
    using const_reference = T const&;
    using pointer         = std::shared_ptr<ContextWithValue<T>>;

    /**
     * @brief The carried value; it never changes after construction.
     */
    const_reference Value() const noexcept { return m_Value; }

    friend std::shared_ptr<ContextWithValue<T>> WithValue<T>( T, context_pointer );
};

/**
 * @brief Returns the root context of the context tree.
 *
 * Background is never cancelled, has no deadline, and carries no values.
 * It is intended as the top-level parent for all derived contexts and is
 * the only context that has no parent itself.
 *
 * The same instance is returned on every call.
 */
context_pointer Background() noexcept;

/**
 * @brief Derives a child context that can be manually cancelled.
 *
 * The child is cancelled immediately, with the same error, if the parent is
 * already cancelled; otherwise it is registered with the parent and cancelled
 * along with it. A null inParent means Background(). The result is a
 * shared_ptr<Context>, which exposes Cancel() directly.
 */
Context::pointer WithCancel( context_pointer inParent = nullptr );

/**
 * @brief Derives a child context that carries an immutable typed value.
 *
 * Creates a ContextWithValue<T> as a child of inParent, with the same
 * cancellation rules as WithCancel(). T must be move-constructible, and a null
 * inParent means Background().
 */
template<typename T>
std::shared_ptr<ContextWithValue<T>> WithValue( T inValue, context_pointer inParent)
{
    auto ctx = std::shared_ptr<ContextWithValue<T>>(
        new ContextWithValue<T>( inParent, std::move(inValue) )
    );

    ctx->Attach();
    return ctx;
}

}