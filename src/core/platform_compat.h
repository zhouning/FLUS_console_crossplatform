// Cross-platform replacement for Windows GetTickCount().
// Returns elapsed milliseconds from a monotonic clock as a double,
// matching the original signature so callers don't need to change.
#ifndef PLATFORM_COMPAT_H
#define PLATFORM_COMPAT_H

#include <chrono>

inline double GetTickCount()
{
    using clock = std::chrono::steady_clock;
    static const auto s_start = clock::now();
    auto now = clock::now();
    return static_cast<double>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now - s_start).count());
}

#endif // PLATFORM_COMPAT_H
