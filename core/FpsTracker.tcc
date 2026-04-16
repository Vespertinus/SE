
#include <algorithm>
#include <numeric>

namespace SE {

void FpsTracker::Update(float raw_delta) {

        vSamples[head] = raw_delta;
        head = (head + 1) % kWindowSize;
        if (head == 0) full = true;
}

float FpsTracker::Fps() const {

        const int count = full ? kWindowSize : head;
        if (count == 0) return 0.0f;

        float sum = 0.0f;
        for (int i = 0; i < count; ++i) sum += vSamples[i];
        return (sum > 0.0f) ? (static_cast<float>(count) / sum) : 0.0f;
}

float FpsTracker::FrameMs() const {

        if (head == 0 && !full) return 0.0f;
        const int last = (head - 1 + kWindowSize) % kWindowSize;
        return vSamples[last] * 1000.0f;
}

float FpsTracker::MinFrameMs() const {

        const int count = full ? kWindowSize : head;
        if (count == 0) return 0.0f;

        float min_val = vSamples[0];
        for (int i = 1; i < count; ++i)
                if (vSamples[i] < min_val) min_val = vSamples[i];
        return min_val * 1000.0f;
}

float FpsTracker::MaxFrameMs() const {

        const int count = full ? kWindowSize : head;
        if (count == 0) return 0.0f;

        float max_val = vSamples[0];
        for (int i = 1; i < count; ++i)
                if (vSamples[i] > max_val) max_val = vSamples[i];
        return max_val * 1000.0f;
}

} // namespace SE
