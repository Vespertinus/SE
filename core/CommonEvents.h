#ifndef COMMON_EVENTS_H
#define COMMON_EVENTS_H

#include <string>

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

struct EQuit {};          // post to request application shutdown
struct ECameraChanged {}; // fired when camera position, orientation, or projection changes

/** Fired by the animation system when an AnimEvent time is crossed during playback. */
struct EAnimEvent {
        std::string name;       ///< event name (e.g. "footstep.left")
        float       value;      ///< optional float payload from clip data
        StrID       stateName;  ///< name of the state that owns the clip
};

//THINK StartFrame EndFrame?
//EResizeViewport
}

#endif
