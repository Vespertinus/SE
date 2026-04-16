
namespace SE {

void AppClock::Tick(double wall_delta) {

        const float raw = (wall_delta > kMaxRawDelta)
                ? kMaxRawDelta
                : static_cast<float>(wall_delta);

        raw_delta = raw;

        if (!paused) {
                scaled_delta  = raw * time_scale;
                total_scaled += scaled_delta;
        } else {
                scaled_delta = 0.0f;
        }

        ++frame_count;
}

void AppClock::Pause() {
        paused = true;
}

void AppClock::Resume() {
        paused = false;
}

void AppClock::SetScale(float s) {
        time_scale = s;
}

} // namespace SE
