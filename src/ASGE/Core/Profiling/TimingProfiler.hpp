#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <string>

#include <ASGE/Core/Time/TimeUtils.hpp>
#include <ASGE/Core/Strings.hpp>
#include <ASGE/Core/Logger/Logger.hpp>

namespace asge::profiling
{

/**
 * @brief Aggregated timing stats for a single named scope.
 *
 * All fields are updated via relaxed atomics on the hot path — exact
 * ordering between count/total/min/max isn't required, only that each
 * individual field is itself consistent.
 */
struct TimingBucket
{
    std::atomic_uint64_t s_CallCount{ 0 };
    std::atomic_uint64_t s_TotalNanos{ 0 };
    std::atomic_uint64_t s_MinNanos{ UINT64_MAX };
    std::atomic_uint64_t s_MaxNanos{ 0 };

    void Record( std::uint64_t inNanos ) noexcept;
    void Reset() noexcept;
};

/**
 * @brief Singleton registry of named timing buckets.
 *
 * Thread-safe: bucket lookup/creation is mutex-guarded, but recording
 * a sample into an already-existing bucket is lock-free (see TimingBucket::Record).
 */
class TimingProfiler
{
private:
    mutable std::mutex m_Mutex;
    std::unordered_map<str::String, TimingBucket> m_Buckets;
    
    TimingProfiler() = default;
public:
    static TimingProfiler& Instance();

    TimingProfiler( TimingProfiler const& )            = delete;
    TimingProfiler& operator=( TimingProfiler const& ) = delete;

    /**
     * @brief Returns a reference to the bucket for inName, creating it if absent.
     *
     * Intended to be cached at the call site (see ScopedTimer) so the lock
     * is only paid once per unique scope name, not on every call.
     */
    TimingBucket& GetOrCreateBucket( str::StringView inName );

    /** @brief Zeroes every bucket's stats. Call once per frame if you want per-frame numbers. */
    void ResetAll();

    // dumps current bucket stats via LOG_INFO
    void LogSummary() const;
};

}