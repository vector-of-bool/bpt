#pragma once

#include <neo/fwd.hpp>

#include <chrono>
#include <utility>

namespace bpt {

class stopwatch {
public:
    using clock      = std::chrono::high_resolution_clock;
    using time_point = clock::time_point;
    using duration   = time_point::duration;

private:
    time_point _start_time = clock::now();

public:
    void     reset() noexcept { _start_time = clock::now(); }
    duration elapsed() const noexcept { return clock::now() - _start_time; }

    template <typename Dur>
    Dur elapsed_as() const noexcept {
        return std::chrono::duration_cast<Dur>(elapsed());
    }

    auto elapsed_ms() const noexcept { return elapsed_as<std::chrono::milliseconds>(); }
    auto elapsed_us() const noexcept { return elapsed_as<std::chrono::microseconds>(); }
};

template <typename Duration = stopwatch::duration, typename Func>
auto timed(Func&& fn) noexcept(noexcept(NEO_FWD(fn)())) {
    stopwatch sw;
    using result_type = decltype(NEO_FWD(fn)());
    if constexpr (std::is_void_v<result_type>) {
        NEO_FWD(fn)();
        auto elapsed = sw.elapsed_as<Duration>();
        return std::pair(elapsed, nullptr);
    } else {
        decltype(auto) value   = NEO_FWD(fn)();
        auto           elapsed = sw.elapsed_as<Duration>();
        return std::pair<Duration, decltype(value)>(elapsed, ((decltype(value)&&)value));
    }
}

}  // namespace bpt