
#ifndef BT_APP_DUMMY_TASKS_H
#define BT_APP_DUMMY_TASKS_H 1

// ---------------------------------------------------------------------------
// bt_app game tasks — the app-supplied half of the compile-time task contract
// (stage plan §13 property 4, plan D3/D4). BTDummyTask proves the typelist
// extension + task-memory lifecycle; BTNextWaypoint is the acceptance-test's
// custom waypoint cycler (the moving_actors AIBrain "what each behavior does"
// half, re-expressed as data + one small task struct).
//
// Note the state split the design wants (plan D7): waypoint progress is
// cross-iteration state, so it lives in the BLACKBOARD (an Int key), not in
// task memory — Loop clears task memory between iterations on purpose.
// ---------------------------------------------------------------------------

#include <ai/BehaviorTreeTasks.h>

namespace SE {

// Counting dummy: every hook appends to counters tests reset between cases.
// Tick runs kTicksPerCompletion times, then succeeds. Init plants a marker in
// task memory to expose zeroing on re-entry/abort.
struct BTDummyTask : TBTTaskBase<BTDummyTask> {
        static constexpr const char* kName = "Dummy";

        inline static int n_init = 0;
        inline static int n_tick = 0;
        inline static int n_stop = 0;

        static void ResetCounters() { n_init = 0; n_tick = 0; n_stop = 0; }

        static void Init(STaskCtx& ctx) {
                ++n_init;
                int marker = 4242;
                std::memcpy(ctx.states[ctx.node_index].mem.data(), &marker, sizeof(marker));
        }

        static EBTStatus Tick(STaskCtx& ctx) {
                ++n_tick;
                SNodeState& st = ctx.states[ctx.node_index];
                if (st.counter + 1 >= kTicksPerCompletion) {
                        st.counter = 0;
                        return EBTStatus::Success;
                }
                ++st.counter;
                return EBTStatus::Running;
        }

        static void Stop(STaskCtx&) { ++n_stop; }

        static constexpr uint32_t kTicksPerCompletion = 3;
};

// Waypoint cycler (game task). Params:
//   target:String     — blackboard Vec3 key receiving the waypoint position
//   index_key:String  — blackboard Int key holding the waypoint index (in/out)
//   count:Int         — number of waypoints (wraps)
//   spacing:Float     — waypoint spacing on +X from the entity's current origin
struct BTNextWaypoint : TBTTaskBase<BTNextWaypoint> {
        static constexpr const char* kName = "NextWaypoint";

        static void Validate(const FlatBuffers::BTTask*) {
                // all params have authored defaults; nothing to reject at load
        }

        static EBTStatus Tick(STaskCtx& ctx) {
                const int    count  = std::max(1, ctx.FindParamInt("count", 1));
                const float  spacing = ctx.FindParamFloat("spacing", 2.f);
                const StrID  target_key(ctx.FindParamString("target", "target"));
                const StrID  index_key(ctx.FindParamString("index_key", "wp_index"));

                BlackboardCore* pBB = ctx.BB();
                if (!pBB) return EBTStatus::Failure;

                SSlotRef idx_slot;
                int next = 0;
                if (pBB->TryResolve(index_key, EBBType::Int, idx_slot)) {
                        next = (pBB->GetInt(idx_slot) + 1) % count;
                        pBB->SetInt(idx_slot, next);
                }

                const glm::vec3 vBase = ctx.pNode ? ctx.pNode->GetTransform().GetWorldPos()
                                                  : glm::vec3{0.f};
                if (!ctx.WriteBBVec3(target_key, vBase + glm::vec3(spacing * float(next), 0.f, 0.f))) {
                        return EBTStatus::Failure;
                }
                return EBTStatus::Success;
        }
};

// Instant terminal tasks for truth tables.
struct BTSucceedTask : TBTTaskBase<BTSucceedTask> {
        static constexpr const char* kName = "Succeed";
        static EBTStatus Tick(STaskCtx&) { return EBTStatus::Success; }
};

struct BTFailTask : TBTTaskBase<BTFailTask> {
        static constexpr const char* kName = "Fail";
        static EBTStatus Tick(STaskCtx&) { return EBTStatus::Failure; }
};

} // namespace SE

#endif
