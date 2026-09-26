
#ifndef APP_WORLD_ASSET_H
#define APP_WORLD_ASSET_H 1

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <glm/glm.hpp>

#include <ResourceHolder.h>
#include <StrID.h>
#include <World_generated.h>

namespace SE {

// Baked .seworld asset: streaming configuration, the sub-level table (identity =
// slot index in the table), world-level spawn definitions and the deterministic
// PRNG seed. All data is baked from FlatBuffers at construction; the raw buffer
// is discarded. Cell membership is derived from baked bounds at load — never
// computed per frame.
class WorldAsset : public ResourceHolder {

public:
        // SubLevelRef.flags bit values (misc/World.fbs enum SubLevelFlags).
        static constexpr uint32_t kFlagAlwaysLoaded = 1;
        static constexpr uint32_t kFlagManual       = 2;
        static constexpr uint32_t kFlagNoNav        = 4;
        static constexpr uint32_t kFlagNoSpawn      = 8;

        struct StreamingConfig {
                bool     enabled                   = true;   // false => everything activates at startup
                float    cell_size                 = 64.f;   // meters, grid for the runtime partition
                float    load_radius               = 96.f;
                float    unload_radius             = 128.f;
                uint32_t max_activations_per_frame = 1;
                float    main_budget_ms            = 2.f;
                uint32_t unload_frame_delay        = 60;
        };

        struct SubLevelEntry {
                std::string path;             // .sesc; the slot index is the identity
                StrID       name;             // "file.sesc#index", debug display only
                glm::vec3   bounds_min{};
                glm::vec3   bounds_max{};
                glm::vec3   translation{};    // instance placement (Level Instance model)
                glm::vec3   rotation{};       // Euler degrees
                uint32_t    flags    = 0;
                StrID       layer;
                float       weight   = 1.f;

                bool IsAlwaysLoaded() const { return (flags & kFlagAlwaysLoaded) != 0; }
                bool IsManual()       const { return (flags & kFlagManual)       != 0; }
                bool IsNoNav()        const { return (flags & kFlagNoNav)        != 0; }
        };

        // World-declared navigation one baked .senm per agent
        // class, by path or inline (the NavMeshHolder convention). Inert data to
        // apps without the navigation runtime; WorldNavBridge consumes it.
        struct NavMeshEntry {
                std::string agent_class;
                std::string path;                                  // external .senm
                std::string cache_name;                            // resource key for inline use
                std::shared_ptr<std::vector<uint8_t>> pInlineData; // repacked inline .senm bytes
        };

        // World-declared global blackboard schema by path or
        // inline (the BlackboardHolder convention, baked like NavMeshEntry).
        // Inert to apps without the AI side; BlackboardSystem consumes it.
        struct SBlackboardEntry {
                std::string path;                                  // external .sebb
                std::string cache_name;                            // resource key for inline use
                std::shared_ptr<std::vector<uint8_t>> pInlineData; // repacked inline .sebb bytes

                bool Empty() const { return path.empty() && !pInlineData; }
        };

        // Production: loads the .seworld file, bakes the tables, discards raw bytes.
        // Throws on any I/O, verification or validation error.
        WorldAsset(const std::string& sName, rid_t rid);
        // Test / inline embedding: bakes from an in-memory World table.
        WorldAsset(const std::string& sName, rid_t rid, const FlatBuffers::World* pWorld);

        StrID                            GetId()               const { return id; }
        const StreamingConfig&            GetStreaming()        const { return streaming; }
        const std::vector<SubLevelEntry>& GetSubLevels()        const { return vSubLevels; }
        const std::vector<StrID>&         GetSpawnPopulations() const { return vSpawnPopulations; }
        const glm::vec3&                  GetInitialFocus()     const { return vInitialFocus; }
        uint64_t                          GetSeed()             const { return seed; }
        /// World-level terrain grid manifest (.seterrain), empty when none
        /// (RPG/strategy terrains ride inside sub-levels as components instead).
        const std::string&                GetTerrainGridManifest() const { return sTerrainGridManifest; }
        const std::vector<NavMeshEntry>&  GetNavMeshes()        const { return vNavMeshes; }
        /// Queries without an explicit class resolve here; empty when undeclared.
        const std::string&                GetDefaultNavClass()  const { return sDefaultNavClass; }
        /// World-level (global) blackboard schema; Empty() when undeclared.
        const SBlackboardEntry&           GetGlobalBlackboard() const { return oGlobalBlackboard; }

        // Cell (cx, cy) covers [cx*cell_size, (cx+1)*cell_size) x [cy*cell_size, (cy+1)*cell_size)
        // on the XZ plane. Membership was built at load from baked bounds.
        void CellForPos(const glm::vec3& vPos, int32_t& cx, int32_t& cy) const;
        bool LevelsInCell(int32_t cx, int32_t cy, std::vector<uint32_t>& vOut) const;

        std::string Str() const;

private:
        StrID                      id;
        StreamingConfig            streaming;
        std::vector<SubLevelEntry> vSubLevels;
        std::vector<StrID>         vSpawnPopulations;
        glm::vec3                  vInitialFocus{};
        uint64_t                   seed = 0;
        std::string                sTerrainGridManifest;
        std::vector<NavMeshEntry>  vNavMeshes;
        std::string                sDefaultNavClass;
        SBlackboardEntry           oGlobalBlackboard;

        // packed cell coords (uint32(cx) << 32 | uint32(cy)) -> sub-level indices
        std::unordered_map<uint64_t, std::vector<uint32_t>> mCellMembership;

        void Build(const FlatBuffers::World* pWorld);
        void BuildCellMembership();
        static uint64_t PackCell(int32_t cx, int32_t cy) {
                return (uint64_t(uint32_t(cx)) << 32) | uint64_t(uint32_t(cy));
        }
};

} // namespace SE

#endif
