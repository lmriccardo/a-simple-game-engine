#pragma once

#include "Thread.hpp"
#include "MPMCQueue.hpp"
#include "Context.hpp"

#include <future>
#include <ASGE/Utils/Functools.hpp>

namespace asge::concurrent
{

namespace _internal
{

struct IExecuter
{
    virtual ~IExecuter() = default;
    virtual void Execute() = 0;
};

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
        if constexpr ( std::is_void_v<return_type> ) {
            this->Call();
        } else {
            m_RetPromise.set_value( this->Call() );
        }
    }
};

// Added deduction guide for WorkerJob constructor
template<typename _Callable, typename... _Args>
WorkerJob(_Callable&&, _Args&&...) -> WorkerJob<_Callable, _Args...>;

}

class Worker : public Thread
{
private:
    

};

}