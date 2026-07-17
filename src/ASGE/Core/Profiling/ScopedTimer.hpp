#pragma once

#include "TimingProfiler.hpp"

namespace asge::profiling
{

class ScopedTimer
{
private:
    TimingBucket&         m_Bucket;
    asge::time::Timepoint m_Start;
public:
    explicit ScopedTimer( TimingBucket& inBucket )
        : m_Bucket( inBucket ), m_Start( asge::time::SteadyNow() )
    {}

    ~ScopedTimer()
    {
        auto elapsed = asge::time::NanosFrom( m_Start );
        m_Bucket.Record( static_cast<std::uint64_t>(elapsed) );
    }

    ScopedTimer( ScopedTimer const& )            = delete;
    ScopedTimer& operator=( ScopedTimer const& ) = delete;
};

}

#define ASGE_CONCAT( a, b ) a##b
#define PROFILE_SCOPE( name )                                                                   \
    static asge::profiling::TimingBucket& ASGE_CONCAT(_bucket_, __LINE__) =                     \
        asge::profiling::TimingProfiler::Instance().GetOrCreateBucket( name );                  \
    asge::profiling::ScopedTimer ASGE_CONCAT(_timer_, __LINE__)(ASGE_CONCAT(_bucket_, __LINE__))
