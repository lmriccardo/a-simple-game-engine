#include "TimingProfiler.hpp"

void asge::profiling::TimingBucket::Record(std::uint64_t inNanos) noexcept
{
    s_CallCount.fetch_add( 1, std::memory_order_relaxed );
    s_TotalNanos.fetch_add( inNanos, std::memory_order_relaxed );

    // relaxed CAS loops — exact min/max under contention isn't critical
    auto currentMin = s_MinNanos.load( std::memory_order_relaxed );
    while ( inNanos < currentMin && !s_MinNanos.compare_exchange_weak( 
        currentMin, inNanos, std::memory_order_relaxed ) 
    ) {}

    auto currentMax = s_MaxNanos.load( std::memory_order_relaxed );
    while ( inNanos > currentMax && !s_MaxNanos.compare_exchange_weak( 
        currentMax, inNanos, std::memory_order_relaxed )
    ) {}
}

void asge::profiling::TimingBucket::Reset() noexcept
{
    s_CallCount.store( 0, std::memory_order_relaxed );
    s_TotalNanos.store( 0, std::memory_order_relaxed );
    s_MinNanos.store( UINT64_MAX, std::memory_order_relaxed );
    s_MaxNanos.store( 0, std::memory_order_relaxed );
}

asge::profiling::TimingProfiler &asge::profiling::TimingProfiler::Instance()
{
    static TimingProfiler profiler;
    return profiler;
}

asge::profiling::TimingBucket &asge::profiling::TimingProfiler::GetOrCreateBucket(str::StringView inName)
{
    std::lock_guard<std::mutex> lock( m_Mutex );

    // TimingBucket holds std::atomic members, so it is neither copyable nor
    // movable — try_emplace constructs it in place (default-initialized)
    // when absent, rather than emplace-ing an already-built temporary.
    auto [it, _] = m_Buckets.try_emplace( str::String(inName) );
    return it->second;
}

void asge::profiling::TimingProfiler::ResetAll()
{
    std::lock_guard<std::mutex> lock( m_Mutex );
    for ( auto& [_, bucket] : m_Buckets )
    {
        bucket.Reset();
    }
}

void asge::profiling::TimingProfiler::LogSummary() const
{
    std::lock_guard<std::mutex> lock( m_Mutex );
    for ( auto const& [name, bucket] : m_Buckets )
    {
        auto count = bucket.s_CallCount.load( std::memory_order_relaxed );
        if ( count == 0 ) continue;

        auto total = bucket.s_TotalNanos.load( std::memory_order_relaxed );
        auto min   = bucket.s_MinNanos.load( std::memory_order_relaxed );
        auto max   = bucket.s_MaxNanos.load( std::memory_order_relaxed );
        auto avg   = total / count;

        LOG_INFO( "[TimingProfiler] {} — calls: {}, avg: {}ns, min: {}ns, max: {}ns",
                    name, count, avg, min, max );
    }
}
