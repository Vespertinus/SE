
#ifndef APP_WORLD_SPATIAL_INDEX_H
#define APP_WORLD_SPATIAL_INDEX_H 1

#include <cstdint>
#include <initializer_list>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

#include <StrID.h>

namespace SE {

// ---------------------------------------------------------------------------
// SpatialIndex — the gameplay spatial query grid.
//
// A uniform 2D (XZ) hash grid over a central generation-stamped slot table.
// Static and dynamic entries share one registration API; "static" only means
// the owner never moves the entry and its lifetime is bound to a sub-level
// (attribution + sweep happen in SpatialIndexSystem, not here).
//
// Contracts:
//  - Main thread only. No allocation in queries or per-entry updates.
//  - Deterministic: the query scan is row-major over the covered cell range
//    (order derived from the query bounds, never from hash-map iteration);
//    in-bucket order is insertion order; Nearest ties break by ascending
//    insertion serial. Same operation sequence => same query results.
//  - Query semantics: ForEachInSphere expands by the entry radius
//    (sphere-sphere); ForEachInAABB intersects the entry's implied AABB
//    [pos - radius, pos + radius]; Nearest ranks by entry CENTER distance
//    (radius bounds the center, it does not expand). All precision-test in
//    full 3D — the 2D grid is a candidate pre-filter only.
//  - Reentrancy: query callbacks may Add, Move and Remove freely. While a
//    query is in flight NO bucket is mutated: inserts, cross-cell relocations
//    and removals all defer their physical bucket updates to the flush at the
//    end of the outermost query (dead slots stay in their bucket, skipped by
//    the alive check). A cross-cell move deferred mid-query stays findable
//    through its old bucket; an entry relocated into the range mid-iteration
//    is picked up by the NEXT query — buckets are a pre-filter, results stay
//    consistent with the layout the query started from.
//  - Entries are spheres around vPos; queries covering more than
//    max_query_cells are refused (log_e + counter), never silently clamped.
// ---------------------------------------------------------------------------

class SpatialIndex {
public:
        using EntryHandle = uint64_t;   // (generation << 32) | (slot + 1); 0 = invalid

        static constexpr size_t kMaxTags = 64;

        struct SConfig {
                float    cell_size       = 16.f;   // meters, XZ grid pitch
                uint32_t max_query_cells = 4096;   // scan-refusal guard (~1 km span at default)
        };

        struct SEntryDesc {
                glm::vec3 vPos      {};
                float     radius    = 0.f;
                StrID     tag       {};          // required — default StrID throws
                uint32_t  sub_level = 0;         // 0 = none (dynamics root / world-less)
        };

        struct SEntry {                        // read-only view handed to callbacks
                glm::vec3   pos       {};
                float       radius    = 0.f;
                uint64_t    tags      = 0;
                uint32_t    sub_level = 0;
                EntryHandle handle    = 0;
        };

        // --- tag registry -------------------------------------------------------
        // GetOrAdd bit index (0..63). When the registry is full, logs an error
        // and returns kMaxTags (never a valid bit); existing bits are untouched.
        uint32_t TagBit(StrID tag);
        /// Resolves every tag (registering unknown ones) into a query mask.
        /// Bits of refused tags are skipped.
        uint64_t TagMask(std::initializer_list<StrID> tags);
        uint32_t TagCount() const { return uint32_t(vTagNames.size()); }
        StrID    TagName(uint32_t bit) const { return bit < vTagNames.size() ? vTagNames[bit] : StrID {}; }

        // --- mutation ------------------------------------------------------
        EntryHandle Add(const SEntryDesc & desc);
        /// false on a stale (removed / cleared-over) handle; otherwise a no-op move.
        bool Move(EntryHandle handle, const glm::vec3 & vPos);
        /// Stale handle: no-op, returns false. Always safe, even mid-query
        /// (the physical bucket erase defers to the flush).
        bool Remove(EntryHandle handle);
        /// Sub-level teardown sweep: drops every entry attributed to sub_level.
        void RemoveBySubLevel(uint32_t sub_level);
        /// Drops everything. Slot generations stay monotonic, so handles held
        /// across a Clear() can never alias a post-Clear entry.
        void Clear();

        // --- queries (callback signature: void(const SEntry &)) -------------
        template <class TFn>
        void ForEachInSphere(const glm::vec3 & vPos, float radius, uint64_t tag_mask, TFn && fn);
        template <class TFn>
        void ForEachInAABB(const glm::vec3 & vMin, const glm::vec3 & vMax, uint64_t tag_mask, TFn && fn);
        bool Nearest(const glm::vec3 & vPos, float max_radius, uint64_t tag_mask, SEntry & out);

        // --- config / stats / debug -----------------------------------------
        /// Refused while entries are alive (cell_size change would invalidate
        /// bucket keys) — configure before populating.
        bool SetConfig(const SConfig & oCfg);
        const SConfig & Config() const { return oConfig; }

        struct SStats {
                uint32_t entries = 0;          // live entries
                uint32_t cells   = 0;          // occupied buckets
                uint32_t tags    = 0;
                uint32_t pending = 0;          // deferred inserts + relocations
                uint32_t refused_queries = 0;
        };
        SStats GetStats() const;

        struct SDebugEntry {
                glm::vec3   pos {};
                float       radius = 0.f;
                uint64_t    tags = 0;
                uint32_t    sub_level = 0;
                EntryHandle handle = 0;
        };
        void DebugDump(std::vector<SDebugEntry> & out) const;

private:
        static constexpr uint32_t kNoBucket = 0xFFFFFFFFu;

        struct SSlot {
                glm::vec3 pos       {};
                float     radius    = 0.f;
                uint64_t  tags      = 0;
                uint32_t  sub_level   = 0;
                uint32_t  serial      = 0;   // insertion order — Nearest tie-break
                uint64_t  cell_key    = 0;   // logical cell (deferred-relocation target)
                uint64_t  bucket_key  = 0;   // physical bucket holding bucket_pos
                uint32_t  bucket_pos  = kNoBucket;
                uint32_t  generation  = 0;   // bumped on every Remove/Clear
                bool      alive        = false;
                bool      move_pending = false;
        };

        struct SQueryRange {
                int32_t min_cx = 0, max_cx = 0;
                int32_t min_cz = 0, max_cz = 0;
                bool    ok     = false;
        };

        // While a query runs, bucket insertion (Add) and cross-cell relocation
        // (Move) defer; the outermost query applies them on scope exit.
        struct SQueryGuard {
                SpatialIndex & oIndex;
                explicit SQueryGuard(SpatialIndex & idx) : oIndex(idx) { ++oIndex.query_depth; }
                ~SQueryGuard() {
                        if (--oIndex.query_depth == 0) { oIndex.FlushPending(); }
                }
        };

        uint32_t    AllocSlot();
        int32_t     CellCoordOf(float v) const;
        uint64_t    CellKeyOf(const glm::vec3 & vPos) const;
        SEntry      ViewOf(uint32_t slot) const;
        bool        Validate(EntryHandle handle, uint32_t & out_slot) const;
        void        RemoveSlot(uint32_t slot);
        void        InsertToBucket(uint32_t slot);
        void        EraseFromBucket(uint32_t slot);
        void        FlushPending();
        /// Cell range covering [vMin, vMax] expanded by the largest live entry
        /// radius — an entry is bucketed by its CENTER cell, but its sphere/AABB
        /// extent may reach into a query whose bounds exclude that center.
        SQueryRange RangeFor(const glm::vec3 & vMin, const glm::vec3 & vMax) const;

        std::vector<SSlot>                                  vSlots;
        std::vector<uint32_t>                               vFree;
        std::unordered_map<uint64_t, std::vector<uint32_t>> mCells;
        std::vector<StrID>                                  vTagNames;

        std::vector<uint32_t> vPendingInserts;   // slot indices (bucket push deferred)
        std::vector<uint32_t> vPendingMoves;     // slot indices (relocation deferred)
        std::vector<uint32_t> vDeadPending;      // slot indices (bucket erase deferred)

        SConfig  oConfig;
        uint32_t next_serial     = 1;
        uint32_t query_depth     = 0;
        uint32_t alive_count     = 0;
        uint32_t refused_queries = 0;
        float    max_entry_radius = 0.f;   // high-water mark; reset only by Clear()
};

} // namespace SE

#endif
