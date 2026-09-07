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

/** Fired after PhysicsSystem::Interpolate() and before Render: transforms written
 *  back by physics (character/rigidbody nodes, ragdoll joints) are final for this
 *  frame. Skin-matrix baking and other per-frame GPU state derivation runs here. */
struct EPreRenderUpdate {
        float last_frame_time;
};

struct EPostRenderUpdate {
        float last_frame_time;
};

struct EStartApp { ;; };

struct EFrameStart { };

struct EQuit {};              // post to request application shutdown
struct ECameraChanged {};     // fired when camera position, orientation, or projection changes
struct ECameraProjChanged {}; // fired only when the camera projection matrix changes

/** Fired by the animation system when an AnimEvent time is crossed during playback.
 *  The name uses a fixed buffer (not std::string): the fire path copies into the
 *  event queue, and a heap allocation there is exactly what this event is meant to
 *  avoid. Text is kept (rather than a bare StrID) because consumers log/display it. */
struct EAnimEvent {
        char        name[256];  ///< event name, null-terminated (e.g. "footstep.left")
        float       value;      ///< optional float payload from clip data
        StrID       name_id;    ///< pre-hashed id (fast equality checks)
        StrID       state_name;  ///< name of the state that owns the clip
};

//THINK StartFrame EndFrame?
//EResizeViewport
}

#endif
