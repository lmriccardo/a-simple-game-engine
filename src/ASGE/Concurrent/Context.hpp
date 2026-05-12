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

/* Converts the Context Error enum into a string representation */
std::string CtxErrStr( ContextErr inErr ) noexcept;

namespace _internal
{

/* Cancel state used for the context to close */
struct CancelState
{
    std::atomic_bool                s_CancelDone{false};
    mutable std::mutex              s_Mtx;
    mutable std::condition_variable s_Cv;

    void Cancel( ContextErr inErr ) noexcept;
    ContextErr Err() const noexcept;
private:
    std::atomic<ContextErr> s_Error{ContextErr::Nil};
};

struct IContext
{
    using pointer = std::shared_ptr<IContext>;

    virtual ~IContext() = default;
    virtual bool Done() const noexcept = 0;
    virtual void Wait() const = 0;
    virtual ContextErr Err() const noexcept = 0;
    virtual void AddChild( pointer const& ) noexcept = 0;
    virtual void Cancel( ContextErr ) = 0;
};

struct ContextWithValue {};

}

using context_pointer = _internal::IContext::pointer;

/**
 * An empty context that is never canceled. This is useful as top-level
 * context for cancellable contexts.
 */
class EmptyContext final : public _internal::IContext 
{
    friend context_pointer Background() noexcept;

    // Keeps the constructor private since it can only be
    // constructed by the Context class using static method
    EmptyContext() = default;

    // Keeps Cancel private since the empty context cannot be canceled
    inline void Cancel( ContextErr ) override {}
public:
    inline bool Done() const noexcept { return false; }
    inline void Wait() const {}
    inline ContextErr Err() const noexcept { return ContextErr::Nil; }
    inline void AddChild( pointer const& ) noexcept {}

    EmptyContext( EmptyContext const& ) = default;
    EmptyContext( EmptyContext&& ) = default;

    EmptyContext& operator=( EmptyContext const& ) = default;
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

    mutable std::mutex     m_Mtx; // Mutex for safely interacts with children;
    _internal::CancelState m_State; // Cancellation state for context lifetime;
    children               m_Children; // Children of the current context

    context_pointer m_Parent{nullptr}; // Parent of the context (if any)

    explicit Context( context_pointer inParent );

    void Attach();
private:
    void AddChild( context_pointer const& inChild ) noexcept override;
    void Cancel( ContextErr inErr ) override;
public:
    using pointer = std::shared_ptr<Context>;

    Context( Context const& )            = delete;
    Context& operator=( Context const& ) = delete;
    Context( Context&& )                 = delete;
    Context& operator=( Context&& )      = delete;

    /* Checks if the current context has finished or not */
    bool Done() const noexcept;

    /* Wait until the context is not canceled */
    void Wait() const;

    /* Returns the canceling error */
    ContextErr Err() const noexcept;

    // Cancel the current context
    void Cancel();
private:
    friend pointer WithCancel( context_pointer );
};

template<typename T>
class ContextWithValue;

/**
 * @brief Derives a child context that carries an immutable typed value.
 *
 * Creates a new ContextWithValue<T> as a child of the given parent, attaching
 * it to the context tree. If the parent is already cancelled at the time of
 * this call, the child is cancelled immediately with the same error. Otherwise
 * the child is registered with the parent and will be cancelled automatically
 * when the parent is.
 * 
 * @tparam T        Type of the value to carry. Must be move-constructible.
 * @param inValue   The value to attach to the context.
 * @param inParent  The parent context (nullptr by default).
 */
template<typename T>
ContextWithValue<T>::pointer WithValue( T inValue, context_pointer inParent = nullptr );

/**
 * @brief A context that carries an immutable typed value alongside cancellation.
 *
 * ContextWithValue extends Context with a single value of type T that is set
 * at construction and never changes. It inherits the full cancellation and tree
 * propagation behaviour of Context — cancelling it behaves identically to
 * cancelling a plain Context.
 * 
 * Created exclusively through WithValue(), never constructed directly.
 *
 * @tparam T  Type of the value carried by this context. Must be move-constructible.
 */
template<typename T>
class ContextWithValue final : public Context
{
private:
    T m_Value; // The value held by the context ( immutable by definition )

    explicit ContextWithValue( context_pointer inParent, T inValue )
    : Context( inParent ), m_Value( inValue )
    {}
public:
    using value_type      = T;
    using const_reference = T const&;
    using pointer         = std::shared_ptr<ContextWithValue<T>>;

    const_reference Value() const noexcept { return m_Value; }

    friend pointer WithValue<>( T, context_pointer );
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
 * The returned context is a child of the given parent. If the parent
 * is already cancelled at the time of this call, the child is cancelled
 * immediately with the same error. Otherwise, the child is registered
 * with the parent and will be cancelled automatically when the parent is.
 *
 * If no parent is provided, Background() is used as the parent.
 *
 * @param inParent  The parent context. Defaults to Background() if nullptr.
 * @return          A shared_ptr<Context> exposing Cancel() directly.
 */
Context::pointer WithCancel( context_pointer inParent = nullptr );

/**
 * @brief Derives a child context that carries an immutable typed value.
 *
 * Creates a new ContextWithValue<T> as a child of the given parent, attaching
 * it to the context tree. If the parent is already cancelled at the time of
 * this call, the child is cancelled immediately with the same error. Otherwise
 * the child is registered with the parent and will be cancelled automatically
 * when the parent is.
 * 
 * @tparam T        Type of the value to carry. Must be move-constructible.
 * @param inValue   The value to attach to the context.
 * @param inParent  The parent context (nullptr by default).
 */
template<typename T>
ContextWithValue<T>::pointer WithValue( T inValue, context_pointer inParent)
{
    auto ctx = std::shared_ptr<ContextWithValue<T>>(
        new ContextWithValue<T>( inParent, std::move(inValue) )
    );

    ctx->Attach();
    return ctx;
}

}