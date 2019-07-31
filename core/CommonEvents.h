#ifndef COMMON_EVENTS_H
#define COMMON_EVENTS_H

namespace SE {

struct EInputUpdate {
        float last_frame_time;
};

struct EUpdate {
        float last_frame_time;
};

struct EPostUpdate {
        float last_frame_time;
};

struct EPostRenderUpdate {
        float last_frame_time;
};

struct EStartApp { ;; };

struct EFrameStart { };

//THINK StartFrame EndFrame?
//EResizeViewport
}

#endif
