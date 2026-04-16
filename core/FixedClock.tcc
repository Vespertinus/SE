
namespace SE {

FixedClock::FixedClock(float fixed_dt) : fixed_dt(fixed_dt) {}

bool FixedClock::Step(float game_delta) {

        if (!fed) {
                accumulator += game_delta;
                fed = true;
        }

        if (accumulator >= fixed_dt) {
                accumulator -= fixed_dt;
                alpha = accumulator / fixed_dt;
                return true;
        }

        alpha = accumulator / fixed_dt;
        fed   = false;
        return false;
}

void FixedClock::Reset() {
        accumulator = 0.0f;
        alpha       = 0.0f;
        fed         = false;
}

} // namespace SE
