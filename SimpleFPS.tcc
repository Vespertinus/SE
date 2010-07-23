
namespace SE {

SimpleFPS::SimpleFPS(const uint32_t delta = 100) : cur_time(&time1), prev_time(&time2), fps(0), frame_counter(0), calc_delta(delta) { 

  gettimeofday(prev_time, NULL);
  
}



void SimpleFPS::Update() {

  ++frame_counter;


  if (frame_counter == calc_delta) {
    
    frame_counter = 0;
    gettimeofday(cur_time, NULL);
    
    double time_delta = ( (double)cur_time->tv_sec  + (double)cur_time->tv_usec  / 1000000.0f ) -
                        ( (double)prev_time->tv_sec + (double)prev_time->tv_usec / 1000000.0f );

    fps = calc_delta / time_delta;
 
    struct timeval * tmp  = prev_time;
    prev_time             = cur_time;
    cur_time              = tmp;

  }

}



double  SimpleFPS::GetFPS() const { return fps; }


} //namespace SE
