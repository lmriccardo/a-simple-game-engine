#include "Logger.hpp"

using namespace asge::logger;

std::ostream &asge::logger::operator<<(std::ostream &inOss, LogLevel inLevel) noexcept
{
    switch (inLevel)
    {
    case LogLevel::DEBUG:   inOss << "DEBUG";   break;
    case LogLevel::INFO:    inOss << "INFO";    break;
    case LogLevel::WARNING: inOss << "WARNING"; break;
    case LogLevel::ERROR:   inOss << "ERROR";   break;
    default:
        break;
    }

    return inOss;
}

Logger &asge::logger::Logger::Instance()
{
    static Logger instance;
    return instance;
}

void asge::logger::Logger::SetLogLevel(LogLevel inLevel) noexcept
{
    m_Level.store(inLevel, std::memory_order_relaxed);
}