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
    std::atomic_uint64_t s_CallCount{ 0 };         // Number of recorded samples
    std::atomic_uint64_t s_TotalNanos{ 0 };        // Sum of all samples, in nanoseconds
    std::atomic_uint64_t s_MinNanos{ UINT64_MAX }; // Shortest sample, UINT64_MAX when empty
    std::atomic_uint64_t s_MaxNanos{ 0 };          // Longest sample, in nanoseconds

    /**
     * @brief Adds one sample of inNanos nanoseconds to the stats.
     *
     * Lock-free: bumps the count and total and updates min/max with relaxed
     * compare-and-swap loops. Safe to call from any thread.
     */
    void Record( std::uint64_t inNanos ) noexcept;

    /**
     * @brief Clears the stats back to their initial state.
     *
     * Each field is reset on its own, so a sample recorded at the same moment
     * may be half cleared.
     */
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
    mutable std::mutex m_Mutex;                       // Guards m_Buckets lookup and creation
    std::unordered_map<str::String, TimingBucket> m_Buckets; // Buckets by scope name

    /**
     * @brief Default constructor.
     *
     * Defaulted; members take their default values.
     */
    TimingProfiler() = default;
public:
    /**
     * @brief Returns the process-wide profiler (created on first use).
     */
    static TimingProfiler& Instance();

    /**
     * @brief Copy construction is disabled.
     */
    TimingProfiler( TimingProfiler const& )            = delete;

    /**
     * @brief Copy assignment is disabled.
     */
    TimingProfiler& operator=( TimingProfiler const& ) = delete;

    /**
     * @brief Returns a reference to the bucket for inName, creating it if absent.
     *
     * Intended to be cached at the call site (see ScopedTimer) so the lock
     * is only paid once per unique scope name, not on every call.
     */
    TimingBucket& GetOrCreateBucket( str::StringView inName );

    /**
     * @brief Zeroes every bucket's stats.
     *
     * Call once per frame if you want per-frame numbers. Buckets stay
     * registered, so references returned by GetOrCreateBucket remain valid.
     */
    void ResetAll();

    /**
     * @brief Logs each bucket's call count, average, min and max via LOG_INFO.
     *
     * Buckets with no recorded calls are skipped. Times are in nanoseconds
     * and the order is the hash map's, so it is unspecified.
     */
    void LogSummary() const;
};

}