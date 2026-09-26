
#ifdef SE_IMPL

#include <spawner/PopulationRuntime.h>

#include <algorithm>

namespace SE {
namespace PopulationRuntime {

void Runtime::Arm(const SDesc & desc) {

        Reset();

        max_active      = desc.max_active;
        respawn_delay_s = desc.respawn_delay_s;

        // Base members first, then each wave's members appended and marked wave_only.
        mMembers.reserve(desc.members.size());
        for (const auto & m : desc.members) {
                SMemberState s;
                s.desc           = m;
                s.desc.wave_only = false;
                s.authored_count = m.count;
                s.ready_t        = m.initial_delay_s;
                mMembers.push_back(s);
        }

        mWaves.reserve(desc.waves.size());
        for (const auto & w : desc.waves) {
                SWaveState s;
                s.desc              = w;
                s.desc.first_member = static_cast<uint32_t>(mMembers.size());
                s.desc.member_count = static_cast<uint32_t>(w.members.size());
                // Same hashing StrID uses (std::hash<string_view>) — the engine
                // side compares this hash directly against ETriggerFired ids.
                s.desc.on_event_hash = w.on_event.empty()
                        ? 0 : std::hash<std::string_view>{}(w.on_event);
                for (const auto & m : w.members) {
                        SMemberState ms;
                        ms.desc           = m;
                        ms.desc.wave_only = true;
                        ms.authored_count = m.count;
                        ms.desc.count     = 0;    // dormant until the wave fires
                        ms.ready_t        = m.initial_delay_s;
                        mMembers.push_back(ms);
                }
                mWaves.push_back(s);
        }

        // Weight priority: stable sort (weight desc, then authored order). Template
        // and geometry resolve through Member(i), so the reordering is invisible.
        std::stable_sort(mMembers.begin(), mMembers.end(),
                        [](const SMemberState & a, const SMemberState & b) {
                                return a.desc.weight > b.desc.weight;
                        });

        if (max_active == 0) {
                for (const auto & m : mMembers) {
                        max_active += m.authored_count;   // wave members count their authored quota
                }
                if (max_active == 0) { max_active = 1; }  // wave-only population: waves still spawn
        }
}

template <class TOut> void Runtime::Tick(float t, bool conditions_ok, TOut && out) {

        // One-shot timed waves fire (condition-gated) before member fills.
        for (auto & w : mWaves) {
                if (!w.fired && w.desc.at_time_s >= 0.f && t >= w.desc.at_time_s && conditions_ok) {
                        w.fired = true;
                        for (uint32_t i = 0; i < w.desc.member_count; ++i) {
                                mMembers[w.desc.first_member + i].desc.count =
                                                mMembers[w.desc.first_member + i].authored_count;
                                mMembers[w.desc.first_member + i].ready_t =
                                                std::max(t, mMembers[w.desc.first_member + i].ready_t);
                        }
                }
        }

        if (!conditions_ok) {
                return;
        }

        uint32_t global_active = ActiveTotal();

        for (uint32_t i = 0; i < mMembers.size(); ++i) {
                SMemberState & m = mMembers[i];

                if (m.desc.count == 0)          { continue; }  // dormant wave member
                if (m.active >= m.desc.count)   { continue; }  // quota full
                if (t < m.ready_t)              { continue; }  // delay / respawn timer not reached

                uint32_t deficit = m.desc.count - m.active;
                if (m.pending_respawn > 0) {
                        // Respawn-pending: only as many as died (a wave member mid-fill
                        // that also lost one must not over-spawn past its quota).
                        deficit = std::min(deficit, m.pending_respawn);
                }
                deficit = std::min(deficit, kMaxEmitsPerMemberTick);
                deficit = std::min(deficit, max_active - global_active);
                if (deficit == 0) { continue; }

                for (uint32_t k = 0; k < deficit; ++k) {
                        out(SSpawnCommand{ i });
                        if (m.pending_respawn > 0) { --m.pending_respawn; }
                        ++global_active;
                        // active is bumped by NotifySpawned when the system executes the
                        // command. Unexecuted commands (frame budget) simply re-emit next
                        // tick — counts stay the source of truth, nothing is queued here.
                }
        }
}

void Runtime::NotifySpawned(uint32_t member) {
        if (member >= mMembers.size()) { return; }
        ++mMembers[member].active;
}

void Runtime::NotifyDied(uint32_t member, float t) {

        if (member >= mMembers.size()) { return; }
        SMemberState & m = mMembers[member];

        if (m.active > 0) { --m.active; }

        const bool bRespawnAllowed = respawn_delay_s >= 0.f
                        && m.respawns_used < m.desc.respawn_limit;

        if (bRespawnAllowed) {
                ++m.respawns_used;
                ++m.pending_respawn;
                m.ready_t = t + respawn_delay_s;
        } else {
                // Respawn denied (disabled or budget exhausted): the death is final —
                // the member's desired count shrinks so Tick does not refill it.
                if (m.desc.count > 0) { --m.desc.count; }
        }
}

bool Runtime::FireWave(uint32_t wave_index, float t) {

        if (wave_index >= mWaves.size()) { return false; }
        SWaveState & w = mWaves[wave_index];
        if (w.fired)                     { return false; }

        w.fired = true;
        for (uint32_t i = 0; i < w.desc.member_count; ++i) {
                auto & m = mMembers[w.desc.first_member + i];
                m.desc.count = m.authored_count;
                m.ready_t    = std::max(t, m.ready_t);
        }
        return true;
}

uint32_t Runtime::ActiveTotal() const {
        uint32_t total = 0;
        for (const auto & m : mMembers) { total += m.active; }
        return total;
}

void Runtime::Reset() {
        mMembers.clear();
        mWaves.clear();
        max_active = 0;
        respawn_delay_s = 0.f;
}

} // namespace PopulationRuntime
} // namespace SE

#endif
