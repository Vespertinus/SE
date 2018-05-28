
#include <chrono>

#include <Global.h>
#include <Logging.h>
#include <Chrono.h>

namespace SE {

CalcDuration::CalcDuration() : 
        start(std::chrono::time_point_cast<ms>(clock::now())) {
        
}

uint32_t CalcDuration::Get() const {

        time_point <ms> end = std::chrono::time_point_cast<ms>(clock::now());
        return std::chrono::duration_cast<ms>(end - start).count();
}

void CalcDuration::Log(std::string_view msg) const {

       log_i("{} duration {} ms", msg, Get()); 
}

}
