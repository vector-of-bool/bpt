#pragma once

#include <chrono>
#include <utility>

namespace dds {

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
auto timed(Func&& fn) noexcept(noexcept(((Func &&)(fn))())) {
    stopwatch sw;
    using result_type = decltype(((Func &&)(fn))());
    if constexpr (std::is_void_v<result_type>) {
        ((Func &&)(fn))();
        auto elapsed = sw.elapsed_as<Duration>();
        return std::pair(elapsed, nullptr);
    } else {
        decltype(auto) value   = ((Func &&)(fn))();
        auto           elapsed = sw.elapsed_as<Duration>();
        return std::pair<Duration, decltype(value)>(elapsed, ((decltype(value)&&)value));
    }
}

}  // namespace dds