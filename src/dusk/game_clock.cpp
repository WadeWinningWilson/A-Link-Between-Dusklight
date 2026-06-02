#include "dusk/game_clock.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <unordered_map>
#include <dusk/frame_interpolation.h>

namespace dusk::game_clock {

using clock = std::chrono::steady_clock;

bool s_initialized = false;
clock::time_point s_previous_sample{};
clock::time_point s_current_snapshot_time{};

std::unordered_map<uintptr_t, clock::time_point> s_interval_last_sample;

float s_sim_rate_hz = 30.0f;
clock::duration s_sim_period_duration = std::chrono::duration_cast<clock::duration>(std::chrono::duration<float>(sim_pace()));

constexpr clock::duration kAbnormalGapResetThreshold = std::chrono::milliseconds(250);
constexpr int kMaxSimTicksPerFrame = 2;

void ensure_initialized() {
    if (s_initialized) {
        return;
    }
    s_previous_sample = clock::now();
    s_current_snapshot_time = s_previous_sample;
    s_initialized = true;
}

void reset_frame_timer() {
    s_previous_sample = clock::now();
    s_current_snapshot_time = s_previous_sample - s_sim_period_duration;
}

void set_sim_rate(float hz) {
    s_sim_rate_hz = std::max(1.0f, std::min(hz, 1000.0f));
    s_sim_period_duration = std::chrono::duration_cast<clock::duration>(std::chrono::duration<float>(1.0f / s_sim_rate_hz));
    reset_frame_timer();
}

float get_sim_rate() {
    return s_sim_rate_hz;
}

MainLoopPacer advance_main_loop() {
    ensure_initialized();

    const clock::time_point now = clock::now();
    const clock::duration frame_gap = now - s_previous_sample;
    const float presentation_dt = std::chrono::duration<float>(frame_gap).count();
    s_previous_sample = now;

    MainLoopPacer out{};
    out.presentation_dt_seconds = presentation_dt;
    out.sim_pace = 1.0f / s_sim_rate_hz;

    const bool should_interpolate = dusk::getSettings().game.enableFrameInterpolation.getValue() !=
                                        dusk::FrameInterpMode::Off &&
                                    !dusk::getTransientSettings().skipFrameRateLimit;
    out.is_interpolating = should_interpolate;

    if (!should_interpolate) {
        s_current_snapshot_time = now;
        out.sim_ticks_to_run = 1;
        return out;
    }

    if (frame_gap > kAbnormalGapResetThreshold) {
        s_current_snapshot_time = now - s_sim_period_duration;
        out.sim_ticks_to_run = 0;
        return out;
    }

    int sim_ticks_to_run = 0;
    clock::time_point projected_snapshot_time = s_current_snapshot_time;
    const clock::time_point render_time = now - s_sim_period_duration;
    while (sim_ticks_to_run < kMaxSimTicksPerFrame && projected_snapshot_time < render_time) {
        projected_snapshot_time += s_sim_period_duration;
        sim_ticks_to_run++;
    }
    out.sim_ticks_to_run = sim_ticks_to_run;
    return out;
}

void commit_sim_tick() {
    ensure_initialized();
    s_current_snapshot_time += s_sim_period_duration;
}

float sample_interpolation_step() {
    ensure_initialized();
    const float step =
        std::chrono::duration<float>(clock::now() - s_current_snapshot_time).count() / (1.0f / s_sim_rate_hz);
    return std::clamp(step, 0.0f, 1.0f);
}

float consume_interval(const void* consumer) {
    ensure_initialized();
    const uintptr_t key = reinterpret_cast<uintptr_t>(consumer);
    const clock::time_point now = clock::now();

    float dt = ui_initial_dt();
    const auto it = s_interval_last_sample.find(key);
    if (it != s_interval_last_sample.end()) {
        dt = std::chrono::duration<float>(now - it->second).count();
        dt = std::min(dt, ui_maximum_dt());
    }
    s_interval_last_sample[key] = now;
    return dt;
}

}  // namespace dusk::game_clock
