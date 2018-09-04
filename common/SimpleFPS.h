
//Simple FPS for UNIX only

#ifndef __SIMPLE_FPS_H__
#define __SIMPLE_FPS_H__ 1

#include <sys/time.h>

namespace SE {

class SimpleFPS {
  
  struct timeval    time1,
                    time2;
  struct timeval  * cur_time,
                  * prev_time;
  double            fps;
  uint32_t          frame_counter;
  uint32_t          calc_delta;

  //TODO average fps
  public:
  
  SimpleFPS(const uint32_t delta = 100);
  void    Update();
  double  GetFPS() const;
};

} // namespace SE

#ifdef SE_IMPL
#include <SimpleFPS.tcc>
#endif

#endif
