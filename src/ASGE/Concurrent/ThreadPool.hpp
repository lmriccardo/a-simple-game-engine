#pragma once

#include "Thread.hpp"
#include "MPMCQueue.hpp"
#include "Context.hpp"

#include <ASGE/Utils/Functools.hpp>

namespace asge::concurrent
{

template<typename _Callable, typename... _Args>
class WorkerJob
{
private:

};

class Worker : public Thread
{
private:
    

};

}