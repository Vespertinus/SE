
#ifdef SE_IMPL

#include <world/SpatialIndex.h>

#include <Logging.h>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace SE {

// ---------------------------------------------------------------------------
// tag registry
// ---------------------------------------------------------------------------

uint32_t SpatialIndex::TagBit(StrID tag) {

        for (uint32_t i = 0; i < vTagNames.size(); ++i) {
                if (vTagNames[i] == tag) { return i; }
        }

        if (vTagNames.size() >= kMaxTags) {
                log_e("SpatialIndex: tag registry exhausted ({} tags) — tag {:x} refused",
                      kMaxTags, uint64_t(tag));
                return kMaxTags;
        }

        vTagNames.push_back(tag);
        return uint32_t(vTagNames.size() - 1);
}

uint64_t SpatialIndex::TagMask(std::initializer_list<StrID> tags) {

        uint64_t mask = 0;
        for (const StrID & tag : tags) {
                const uint32_t bit = TagBit(tag);
                if (bit < kMaxTags) { mask |= (1ull << bit); }
        }
        return mask;
}

// ---------------------------------------------------------------------------
// slots
// ---------------------------------------------------------------------------

uint32_t SpatialIndex::AllocSlot() {

        if (!vFree.empty()) {
                const uint32_t slot = vFree.back();
                vFree.pop_back();
                return slot;
        }

        vSlots.emplace_back();
        return uint32_t(vSlots.size() - 1);
}

bool SpatialIndex::Validate(const EntryHandle handle, uint32_t & out_slot) const {

        if (handle == 0) { return false; }

        const uint32_t slot = uint32_t(handle & 0xFFFFFFFFu) - 1;
        const uint32_t gen  = uint32_t(handle >> 32);

        if (slot >= vSlots.size()) { return false; }

        const SSlot & s = vSlots[slot];
        if (!s.alive || s.generation != gen) { return false; }

        out_slot = slot;
        return true;
}

SpatialIndex::SEntry SpatialIndex::ViewOf(const uint32_t slot) const {

        const SSlot & s = vSlots[slot];
        SEntry e;
        e.pos       = s.pos;
        e.radius    = s.radius;
        e.tags      = s.tags;
        e.sub_level = s.sub_level;
        e.handle    = (uint64_t(s.generation) << 32) | uint64_t(slot + 1);
        return e;
}

// ---------------------------------------------------------------------------
// cells
// ---------------------------------------------------------------------------

int32_t SpatialIndex::CellCoordOf(const float v) const {
        return int32_t(std::floor(v / oConfig.cell_size));
}

uint64_t SpatialIndex::CellKeyOf(const glm::vec3 & vPos) const {
        const int32_t cx = CellCoordOf(vPos.x);
        const int32_t cz = CellCoordOf(vPos.z);
        return (uint64_t(uint32_t(cx)) << 32) | uint64_t(uint32_t(cz));
}

void SpatialIndex::InsertToBucket(const uint32_t slot) {

        SSlot & s = vSlots[slot];
        std::vector<uint32_t> & bucket = mCells[s.cell_key];
        s.bucket_key = s.cell_key;
        s.bucket_pos = uint32_t(bucket.size());
        bucket.push_back(slot);
}

void SpatialIndex::EraseFromBucket(const uint32_t slot) {

        SSlot & s = vSlots[slot];
        if (s.bucket_pos == kNoBucket) { return; }   // pending insert — nothing physical yet

        const auto it = mCells.find(s.bucket_key);
        if (it == mCells.end()) { return; }          // defensive; cannot happen

        std::vector<uint32_t> & bucket = it->second;

        // swap-erase; the relocated occupant inherits the erased position
        const uint32_t moved = bucket.back();
        bucket[s.bucket_pos] = moved;
        bucket.pop_back();
        if (moved != slot) { vSlots[moved].bucket_pos = s.bucket_pos; }

        if (bucket.empty()) { mCells.erase(it); }

        s.bucket_pos = kNoBucket;
}

void SpatialIndex::FlushPending() {

        // dead slots first: their bucket records must go before anything can
        // reuse the slots (reuse only starts after this loop)
        for (const uint32_t slot : vDeadPending) {
                EraseFromBucket(slot);
                vFree.push_back(slot);
        }
        vDeadPending.clear();

        for (const uint32_t slot : vPendingInserts) {
                SSlot & s = vSlots[slot];
                if (s.alive && s.bucket_pos == kNoBucket) { InsertToBucket(slot); }
        }
        vPendingInserts.clear();

        for (const uint32_t slot : vPendingMoves) {
                SSlot & s = vSlots[slot];
                if (!s.alive) { s.move_pending = false; continue; }
                if (s.cell_key != s.bucket_key) {
                        EraseFromBucket(slot);
                        InsertToBucket(slot);
                }
                s.move_pending = false;
        }
        vPendingMoves.clear();
}

// ---------------------------------------------------------------------------
// mutation
// ---------------------------------------------------------------------------

SpatialIndex::EntryHandle SpatialIndex::Add(const SEntryDesc & desc) {

        if (desc.tag == StrID {}) {
                throw std::runtime_error("SpatialIndex::Add: a tag is required");
        }

        const uint32_t bit = TagBit(desc.tag);
        if (bit >= kMaxTags) { return 0; }   // registry overflow — already logged

        const uint32_t slot = AllocSlot();
        SSlot & s = vSlots[slot];
        s.pos          = desc.vPos;
        s.radius       = desc.radius;
        s.tags         = 1ull << bit;
        max_entry_radius = std::max(max_entry_radius, desc.radius);
        s.sub_level    = desc.sub_level;
        s.serial       = next_serial++;
        s.cell_key     = CellKeyOf(desc.vPos);
        s.bucket_key   = s.cell_key;
        s.bucket_pos   = kNoBucket;
        s.alive        = true;
        s.move_pending = false;
        ++alive_count;

        if (query_depth > 0) {
                vPendingInserts.push_back(slot);
        } else {
                InsertToBucket(slot);
        }

        return (uint64_t(s.generation) << 32) | uint64_t(slot + 1);
}

bool SpatialIndex::Move(const EntryHandle handle, const glm::vec3 & vPos) {

        uint32_t slot;
        if (!Validate(handle, slot)) { return false; }

        SSlot & s = vSlots[slot];
        s.pos = vPos;

        const uint64_t new_key = CellKeyOf(vPos);
        if (new_key == s.cell_key) { return true; }   // same cell — in-place update

        if (s.bucket_pos == kNoBucket) {
                // still pending its first bucket insert — just retarget
                s.cell_key = new_key;
                return true;
        }

        if (query_depth > 0) {
                s.cell_key = new_key;
                if (!s.move_pending) {
                        s.move_pending = true;
                        vPendingMoves.push_back(slot);
                }
                // the slot stays in its (now stale) physical bucket for the rest
                // of the query — buckets are a conservative pre-filter, the
                // precision test still sees the live position
                return true;
        }

        EraseFromBucket(slot);
        s.cell_key = new_key;
        InsertToBucket(slot);
        return true;
}

bool SpatialIndex::Remove(const EntryHandle handle) {

        uint32_t slot;
        if (!Validate(handle, slot)) { return false; }
        RemoveSlot(slot);
        return true;
}

void SpatialIndex::RemoveSlot(const uint32_t slot) {

        SSlot & s = vSlots[slot];

        if (query_depth > 0) {
                // never mutate a bucket mid-query: the slot stays in its physical
                // bucket as a dead record (queries skip it by alive) and the
                // physical erase + slot reuse happen at the flush
                s.alive        = false;
                s.move_pending = false;
                ++s.generation;
                --alive_count;
                vDeadPending.push_back(slot);
                return;
        }

        EraseFromBucket(slot);
        s.alive        = false;
        s.move_pending = false;
        ++s.generation;
        vFree.push_back(slot);
        --alive_count;
}

void SpatialIndex::RemoveBySubLevel(const uint32_t sub_level) {

        se_assert(query_depth == 0);   // event-context only; mutates buckets wholesale

        // collect first: Remove mutates buckets and the free list
        std::vector<uint32_t> doomed;
        for (uint32_t i = 0; i < vSlots.size(); ++i) {
                const SSlot & s = vSlots[i];
                if (s.alive && s.sub_level == sub_level) { doomed.push_back(i); }
        }
        for (const uint32_t slot : doomed) { RemoveSlot(slot); }
}

void SpatialIndex::Clear() {

        se_assert(query_depth == 0);   // event-context only; drops every bucket

        // keep generations monotonic: handles held across a Clear() must never
        // alias a post-Clear entry reusing the slot
        for (uint32_t i = 0; i < vSlots.size(); ++i) {
                SSlot & s = vSlots[i];
                if (s.alive) {
                        s.alive = false;
                        ++s.generation;
                        vFree.push_back(i);
                }
                s.move_pending = false;
                s.bucket_pos   = kNoBucket;
        }
        mCells.clear();
        vPendingInserts.clear();
        vPendingMoves.clear();
        alive_count      = 0;
        max_entry_radius = 0.f;
}

// ---------------------------------------------------------------------------
// queries
// ---------------------------------------------------------------------------

SpatialIndex::SQueryRange SpatialIndex::RangeFor(const glm::vec3 & vMin, const glm::vec3 & vMax) const {

        // entries are bucketed by their CENTER cell but their extent can reach
        // beyond it — widen the candidate range by the largest live radius
        const float pad = max_entry_radius;

        SQueryRange r;
        r.min_cx = CellCoordOf(vMin.x - pad);
        r.max_cx = CellCoordOf(vMax.x + pad);
        r.min_cz = CellCoordOf(vMin.z - pad);
        r.max_cz = CellCoordOf(vMax.z + pad);

        const int64_t nx = int64_t(r.max_cx) - int64_t(r.min_cx) + 1;
        const int64_t nz = int64_t(r.max_cz) - int64_t(r.min_cz) + 1;
        r.ok = (nx * nz) <= int64_t(oConfig.max_query_cells);
        return r;
}

template <class TFn>
void SpatialIndex::ForEachInSphere(const glm::vec3 & vPos, const float radius,
                                   const uint64_t tag_mask, TFn && fn) {

        const glm::vec3 vMin(vPos.x - radius, vPos.y - radius, vPos.z - radius);
        const glm::vec3 vMax(vPos.x + radius, vPos.y + radius, vPos.z + radius);

        const SQueryRange range = RangeFor(vMin, vMax);
        if (!range.ok) {
                ++refused_queries;
                log_e("SpatialIndex: sphere query (r {}) exceeds max_query_cells ({}) — refused",
                      radius, oConfig.max_query_cells);
                return;
        }

        SQueryGuard guard(*this);

        for (int32_t cz = range.min_cz; cz <= range.max_cz; ++cz) {
                for (int32_t cx = range.min_cx; cx <= range.max_cx; ++cx) {

                        const uint64_t key = (uint64_t(uint32_t(cx)) << 32) | uint64_t(uint32_t(cz));
                        const auto it = mCells.find(key);
                        if (it == mCells.end()) { continue; }

                        for (const uint32_t slot : it->second) {
                                const SSlot & s = vSlots[slot];
                                if (!s.alive || (s.tags & tag_mask) == 0) { continue; }

                                const glm::vec3 d   = s.pos - vPos;
                                const float     rr  = radius + s.radius;
                                const float     rr2 = rr * rr;
                                if (d.x * d.x + d.y * d.y + d.z * d.z <= rr2) { fn(ViewOf(slot)); }
                        }
                }
        }
}

template <class TFn>
void SpatialIndex::ForEachInAABB(const glm::vec3 & vMin, const glm::vec3 & vMax,
                                 const uint64_t tag_mask, TFn && fn) {

        const SQueryRange range = RangeFor(vMin, vMax);
        if (!range.ok) {
                ++refused_queries;
                log_e("SpatialIndex: AABB query exceeds max_query_cells ({}) — refused",
                      oConfig.max_query_cells);
                return;
        }

        SQueryGuard guard(*this);

        for (int32_t cz = range.min_cz; cz <= range.max_cz; ++cz) {
                for (int32_t cx = range.min_cx; cx <= range.max_cx; ++cx) {

                        const uint64_t key = (uint64_t(uint32_t(cx)) << 32) | uint64_t(uint32_t(cz));
                        const auto it = mCells.find(key);
                        if (it == mCells.end()) { continue; }

                        for (const uint32_t slot : it->second) {
                                const SSlot & s = vSlots[slot];
                                if (!s.alive || (s.tags & tag_mask) == 0) { continue; }

                                const bool overlap =
                                        (s.pos.x - s.radius <= vMax.x && s.pos.x + s.radius >= vMin.x) &&
                                        (s.pos.y - s.radius <= vMax.y && s.pos.y + s.radius >= vMin.y) &&
                                        (s.pos.z - s.radius <= vMax.z && s.pos.z + s.radius >= vMin.z);
                                if (overlap) { fn(ViewOf(slot)); }
                        }
                }
        }
}

bool SpatialIndex::Nearest(const glm::vec3 & vPos, const float max_radius,
                           const uint64_t tag_mask, SEntry & out) {

        const glm::vec3 vMin(vPos.x - max_radius, vPos.y - max_radius, vPos.z - max_radius);
        const glm::vec3 vMax(vPos.x + max_radius, vPos.y + max_radius, vPos.z + max_radius);

        const SQueryRange range = RangeFor(vMin, vMax);
        if (!range.ok) {
                ++refused_queries;
                log_e("SpatialIndex: nearest query (r {}) exceeds max_query_cells ({}) — refused",
                      max_radius, oConfig.max_query_cells);
                return false;
        }

        SQueryGuard guard(*this);

        float    best_d2     = max_radius * max_radius;
        uint32_t best_serial = 0xFFFFFFFFu;
        uint32_t best_slot   = 0xFFFFFFFFu;

        for (int32_t cz = range.min_cz; cz <= range.max_cz; ++cz) {
                for (int32_t cx = range.min_cx; cx <= range.max_cx; ++cx) {

                        const uint64_t key = (uint64_t(uint32_t(cx)) << 32) | uint64_t(uint32_t(cz));
                        const auto it = mCells.find(key);
                        if (it == mCells.end()) { continue; }

                        for (const uint32_t slot : it->second) {
                                const SSlot & s = vSlots[slot];
                                if (!s.alive || (s.tags & tag_mask) == 0) { continue; }

                                const glm::vec3 d  = s.pos - vPos;
                                const float     d2 = d.x * d.x + d.y * d.y + d.z * d.z;
                                if (d2 > best_d2) { continue; }

                                // deterministic tie-break: nearer serial (older entry) wins
                                if (d2 < best_d2 || s.serial < best_serial) {
                                        best_d2     = d2;
                                        best_serial = s.serial;
                                        best_slot   = slot;
                                }
                        }
                }
        }

        if (best_slot == 0xFFFFFFFFu) { return false; }
        out = ViewOf(best_slot);
        return true;
}

// ---------------------------------------------------------------------------
// config / stats / debug
// ---------------------------------------------------------------------------

bool SpatialIndex::SetConfig(const SConfig & oCfg) {

        if (alive_count != 0 && oCfg.cell_size != oConfig.cell_size) {
                log_e("SpatialIndex::SetConfig: cell_size change with live entries refused (Clear() first)");
                return false;
        }
        oConfig = oCfg;
        return true;
}

SpatialIndex::SStats SpatialIndex::GetStats() const {

        SStats st;
        st.entries          = alive_count;
        st.cells            = uint32_t(mCells.size());
        st.tags             = uint32_t(vTagNames.size());
        st.pending          = uint32_t(vPendingInserts.size() + vPendingMoves.size());
        st.refused_queries  = refused_queries;
        return st;
}

void SpatialIndex::DebugDump(std::vector<SDebugEntry> & out) const {

        out.clear();
        out.reserve(alive_count);
        for (uint32_t i = 0; i < vSlots.size(); ++i) {
                if (!vSlots[i].alive) { continue; }
                const SEntry e = ViewOf(i);
                out.push_back(SDebugEntry { e.pos, e.radius, e.tags, e.sub_level, e.handle });
        }
}

} // namespace SE

#endif
