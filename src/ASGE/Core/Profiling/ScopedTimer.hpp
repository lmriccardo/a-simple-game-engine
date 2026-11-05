#pragma once

#include "TimingProfiler.hpp"

namespace asge::profiling
{

/**
 * @brief RAII timer that records its lifetime into a TimingBucket.
 *
 * Starts the steady clock on construction and, on destruction, records the
 * elapsed nanoseconds into the bucket. Usually created through PROFILE_SCOPE.
 */
class ScopedTimer
{
private:
    TimingBucket&         m_Bucket; // Bucket that receives the sample
    asge::time::Timepoint m_Start;  // Steady-clock time at construction
public:
    /**
     * @brief Starts timing; inBucket must outlive the timer.
     */
    explicit ScopedTimer( TimingBucket& inBucket )
        : m_Bucket( inBucket ), m_Start( asge::time::SteadyNow() )
    {}

    /**
     * @brief Stops timing and records the elapsed time into the bucket.
     */
    ~ScopedTimer()
    {
        auto elapsed = asge::time::NanosFrom( m_Start );
        m_Bucket.Record( static_cast<std::uint64_t>(elapsed) );
    }

    ScopedTimer( ScopedTimer const& )            = delete;
    ScopedTimer& operator=( ScopedTimer const& ) = delete;
};

}

// Token-pastes a and b. Operands of ## are not macro-expanded, so __LINE__
// stays literal and two PROFILE_SCOPEs in one scope would clash.
#define ASGE_CONCAT( a, b ) a##b

// Times the rest of the enclosing scope under the bucket called `name`. The
// bucket lookup is cached in a function-local static, so the profiler lock is
// paid once per call site; the name must therefore be the same on every call.
#define PROFILE_SCOPE( name )                                                                   \
    static asge::profiling::TimingBucket& ASGE_CONCAT(_bucket_, __LINE__) =                     \
        asge::profiling::TimingProfiler::Instance().GetOrCreateBucket( name );                  \
    asge::profiling::ScopedTimer ASGE_CONCAT(_timer_, __LINE__)(ASGE_CONCAT(_bucket_, __LINE__))
