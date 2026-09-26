
#ifndef APP_SPAWNER_POPULATION_RUNTIME_H
#define APP_SPAWNER_POPULATION_RUNTIME_H 1

// Pure quota/respawn/wave state machine (no engine includes). The Population
// component owns scene handles; the runtime owns POLICY: how many of what to
// spawn when. All times are "seconds since Arm" (AppClock-scaled by the caller).

#include <cstdint>
#include <string>
#include <vector>

namespace SE {
namespace PopulationRuntime {

struct SMemberDesc {
        std::string template_id;
        std::string geometry;                // SpawnPoint/SpawnVolume node name; "" = population origin
        float       weight          = 1.f;   // higher fills first when quota < demand (sorted at Arm)
        uint32_t    count           = 1;     // simultaneous instances
        float       initial_delay_s = 0.f;
        uint32_t    respawn_limit   = 0xFFFFFFFFu;
        bool        wave_only       = false; // dormant until FireWave (wave members)
};

struct SWaveDesc {
        float       at_time_s = -1.f;        // >= 0: fires once at t (condition-gated)
        std::string on_event;                // fires when the component matches the event id
        // Pre-hashed on_event (Arm) — OnTriggerFired compares hashes instead of
        // re-hashing the constant name per wave per event. 0 = no event.
        uint64_t    on_event_hash = 0;
        std::vector<SMemberDesc> members;    // input: this wave's members (Arm appends them
                                             // to the member table and sets the range below)
        uint32_t    first_member = 0;        // member range [first_member, first_member + member_count)
        uint32_t    member_count = 0;
};

struct SDesc {
        std::vector<SMemberDesc> members;    // base members + wave members (appended, wave_only)
        std::vector<SWaveDesc>   waves;
        uint32_t max_active     = 0;         // 0 = sum of member counts
        float    respawn_delay_s = 0.f;      // < 0: destroyed spawns never respawn
};

struct SSpawnCommand {
        uint32_t member;                     // index into the runtime's sorted member view
};

class Runtime {
public:
        /// Build internal state from desc. Members are stable-sorted by weight
        /// (desc, then authored order) — indices in commands refer to the SORTED view
        /// (query Member(i) for template/geometry). Resets everything.
        void Arm(const SDesc & desc);

        /// Advance policy. t = seconds since Arm. Emits at most a bounded batch of
        /// spawn commands per member per tick; unexecuted commands (frame budget)
        /// are simply re-emitted next tick — safe to drop.
        template <class TOut> void Tick(float t, bool conditions_ok, TOut && out);

        /// The system executed a spawn command for this member.
        void NotifySpawned(uint32_t member);

        /// A tracked spawn died (destroy, despawn sweep, cascade). Schedules a
        /// respawn when the population policy allows it.
        void NotifyDied(uint32_t member, float t);

        /// Arm an event wave (one-shot). The wave's members become active on the
        /// next Tick (condition-gated) — never spawned inside the event callback.
        bool FireWave(uint32_t wave_index, float t);

        const SMemberDesc & Member(uint32_t i) const { return mMembers[i].desc; }
        const SWaveDesc &   Wave(uint32_t i)   const { return mWaves[i].desc; }
        uint32_t MemberCount() const { return static_cast<uint32_t>(mMembers.size()); }
        uint32_t WaveCount()   const { return static_cast<uint32_t>(mWaves.size()); }

        uint32_t ActiveTotal() const;
        uint32_t MaxActive()   const { return max_active; }
        bool     WaveFired(uint32_t wave_index) const { return mWaves[wave_index].fired; }

        void Reset();

private:
        struct SMemberState {
                SMemberDesc desc;
                uint32_t authored_count  = 0;   // original count; wave members keep it while dormant
                uint32_t active          = 0;
                uint32_t pending_respawn = 0;
                uint32_t respawns_used   = 0;
                float    ready_t         = 0.f; // earliest t the next spawn may emit
        };
        struct SWaveState {
                SWaveDesc desc;
                bool      fired = false;
        };

        std::vector<SMemberState> mMembers;
        std::vector<SWaveState>   mWaves;
        uint32_t max_active = 0;
        float    respawn_delay_s = 0.f;

        static constexpr uint32_t kMaxEmitsPerMemberTick = 8;
};

} // namespace PopulationRuntime
} // namespace SE

#endif
