#pragma once

#include <memory>
#include <type_traits>
#include <concepts>
#include <map>
#include <mutex>
#include <functional>
#include <cstddef>
#include <cstdint>

#include <ASGE/Core/Functools.hpp>

namespace asge::signals
{

// Forward declaration of the Connection class
template <typename... Args> class Connection;

/**
 * Creates a callback wrapper that holds a weak reference to an object and
 * safely invokes a member function on that object if it is still alive.
 * 
 * This function accepts a member function pointer and a shared pointer to an object
 * and returns a callable lambda that, when invoked, attempts to lock the weak pointer
 * to the object. If successful, it calls the member function otherwise do nothing.
 * 
 * @tparam T The class type of the object owning the member function
 * @tparam _Callable Type of the member function pointer
 * 
 * @param func The member function pointer to invoke on the object
 * @param ptr  Shared pointer to the object instance
 */
template<typename T, typename Callable>
requires functools::_internal::MemberFunctionOf<Callable, T>
auto MakeCallback( Callable&& inFunc, std::shared_ptr<T> inPtr )
{
    std::weak_ptr<T> wptr = inPtr;
    return [ wptr, func = std::forward<Callable>(inFunc)](auto&&... args)
    {
        if ( std::shared_ptr<T> sptr = wptr.lock() )
        {
            ( sptr.get()->*func )( std::forward<decltype(args)>(args)... );
        }
    };
}

// --------------------------------------------------
// ---------- SIGNAL CLASS DECLARATION --------------
// --------------------------------------------------
/**
 * This class represents a simple Thread-safe Signal. In the signals and slot design
 * pattern a signal is represented by a function ( the actual emitted signal ) and
 * a number of connections connecting the signal with a number of slots ( callback
 * functions ) which must have the same signature of the signaling function. Valid
 * connections are created using the `Signal::Connect` method. When emitting the
 * signal, before calling all the releated slot functions, it checks for invalid
 * connections and in case they exists it disconnects them from itself, in some sense
 * performing automatic-disconnection.
 * 
 * Differently from the more generic Observer or Event Dispatching design pattern,
 * the Signal and Slot pattern heavily relies on type safety, given that the signal
 * and all connected slots must have the same function signature. This is obtained
 * via its template parameter arguments.
 * 
 * @param Args The type of input parameters of the signal
 */
template<typename ...Args>
class Signal
{
    friend class Connection<Args...>; // Used for auto-disconnection
private:
    using conn_type = Connection<Args...>;
    using slot_type = std::function<void(Args...)>;
    using lifetime_ptr = std::shared_ptr<void>;
    using lifetime_token = std::weak_ptr<void>;

    struct slot_info
    {
        slot_type s_Callback;   // The actual callback
        lifetime_token s_Token; // Manages the lifetime token
        bool s_HasToken;        // If the slot relies on a token
    };

    using conn_container = std::map<size_t, slot_info>;

    std::shared_ptr<conn_container> m_Connections; // Contains connections with the callback
    static std::mutex m_Mutex;
public:
    Signal() : m_Connections( std::make_shared<conn_container>() ) {};
        
    /**
     * Connect a slot to the current signal. The connection is managed
     * either using a lifetime token or manually.
     * 
     * @param inCallback The callback to run when emitting the signal
     * @param inLfToken The lifetime token to handle disconnection
     */
    conn_type Connect( slot_type inCallback, lifetime_ptr inLfToken = nullptr );

    /* Emits the signal with the input arguments. */
    void Emit( Args... inArgs );

    /* Returns the total number of active connections */
    std::size_t GetNofConnections() const
    {
        return m_Connections.size();
    }
};

template <typename... Args>
std::mutex Signal<Args...>::m_Mutex;

// --------------------------------------------------
// ---------- CONNECTION CLASS DECLARATION ----------
// --------------------------------------------------
/**
 * This class represents a single Connection to a Signal. Every connection must have a
 * unique identifier and a weak pointer to the connection map of the Signal class to which 
 * it is connected. Unique connection identifiers are generated incrementally when a new 
 * connection is created. The weak pointer to the Signal connection map is used to avoid 
 * dangling pointer and to ensure that operations are performed only if the object is still 
 * valid. For a single connection there are two ways of disconnecting it from the signal.
 * 
 * The first one is "manual disconnection", and involved calling the `Connection::Disconnect`
 * method manually. The other one, relies on a so-called lifetime token which is a weak
 * pointer to a generic object and it is used to automatic manage connection lifetime. That
 * is, if the weak pointer becomes invalid the connection is automatically disconnected
 * and so also destructuted. 
*/
template <typename... Args>
class Connection
{
    friend class Signal<Args...>; // For accessing the signal mutex

private:
    using signal_type = Signal<Args...>;
    using signal_slots_type = std::weak_ptr<typename signal_type::conn_container>;
    using lifetime_token = typename signal_type::lifetime_token;

    size_t            m_ConnId;
    signal_slots_type m_WeakSlots;
    bool              m_HasToken;

    /**
     * Generate a monotonic increasing connection unique identifiers
     * for connections handling.
     */
    static size_t NextConnectionId()
    {
        static size_t connection_id = 0;
        return ++connection_id;
    }

    // Private constructor only accessible by the Signal class itself
    Connection( size_t inId, signal_slots_type inSlotWptr, bool inHasToken ) 
    : m_ConnId( inId ), m_WeakSlots( inSlotWptr ), m_HasToken( inHasToken )
    {};

public:
    Connection() : m_ConnId( 0 ) {}; // Invalid connection constructor

    /* Manual disconnection from the Signal. */
    void Disconnect()
    {
        if ( auto signal_slots = m_WeakSlots.lock() )
        {
            std::lock_guard<std::mutex> _lock( signal_type::m_Mutex );
            signal_slots->erase( m_ConnId );
            m_ConnId = 0;
        }
    }

    /**
     * Returns true if the current connection is still alive. In this context
     * a connection is alive if the identifier != 0, the weak pointer to the
     * connection map is still valid and the lifetime token has not expired.
     */
    bool IsConnected() const
    {
        // First, return False if the connection id is 0 or if the weak pointer
        // to the signal slot is nullptr, meaning the Signal went out of scope
        // or freed
        if ( m_ConnId == 0 || m_WeakSlots.lock() == nullptr ) return false;

        // Finally, check for the presence of the current connection id
        // in the signal map and in case if the token has expired
        auto it = m_WeakSlots.lock()->find( m_ConnId );
        if ( it == m_WeakSlots.lock()->end() ) return false;
        return !m_HasToken || !it->second.s_Token.expired();
    }

    /* Returns the connection id */
    size_t GetConnectionId() const
    {
        return m_ConnId;
    }
};

// --------------------------------------------------
// ---------- SIGNAL CLASS DEFINITION ---------------
// --------------------------------------------------

template <typename... Args>
inline Signal<Args...>::conn_type Signal<Args...>::Connect(slot_type inCallback, lifetime_ptr inLfToken)
{
    std::lock_guard<std::mutex> _lock( m_Mutex );
    size_t conn_id = conn_type::NextConnectionId();
    (*m_Connections).insert_or_assign( conn_id, 
            slot_info{ std::move( inCallback ), inLfToken, inLfToken != nullptr }
        );

    return conn_type( conn_id, m_Connections, inLfToken != nullptr );
}

template <typename... Args>
inline void Signal<Args...>::Emit( Args... inArgs )
{
    std::vector< slot_type > slots_to_call;

    // Before calling all the callbacks, we need to handle invalid
    // connections which are still in the connection map
    {
        std::lock_guard<std::mutex> _lock( m_Mutex );
        auto conn_it = m_Connections->begin();
        for ( ; conn_it != m_Connections->end() ; )
        {
            const bool has_token = conn_it->second.s_HasToken;
            const lifetime_token lf_token = conn_it->second.s_Token;
            if ( has_token && lf_token.expired() )
            {
                // If the token has expired this means that the object
                // pointed by the weak_pointer is no longer valid.
                conn_it = m_Connections->erase( conn_it );
                continue;
            }

            slots_to_call.push_back( conn_it->second.s_Callback );
            conn_it++;
        }
    }

    // At this point, we can call the callbacks
    for ( auto& slot: slots_to_call )
    {
        slot( inArgs... );
    }
}

};