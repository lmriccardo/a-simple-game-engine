#include "Logger.hpp"

using namespace asge::logger;

std::ostream &asge::logger::operator<<(std::ostream &inOss, LogLevel inLevel) noexcept
{
    switch (inLevel)
    {
    case LogLevel::Debug:   inOss << "DEBUG";   break;
    case LogLevel::Info:    inOss << "INFO";    break;
    case LogLevel::Warning: inOss << "WARNING"; break;
    case LogLevel::Error:   inOss << "ERROR";   break;
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