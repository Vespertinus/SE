
#ifndef __CHRONO_H__
#define __CHRONO_H__ 1

namespace SE {

using clock     = std::chrono::high_resolution_clock;
using ms        = std::chrono::milliseconds;
using micro     = std::chrono::microseconds;
using sec       = std::chrono::seconds;

template<class Duration> using time_point = std::chrono::time_point<clock, Duration>;


class CalcDuration {
        time_point <ms> start;

        public:
        CalcDuration();
        void Log(std::string_view msg) const;
        uint32_t Get() const;
};

}
#endif
