
#ifndef APP_AI_BLACKBOARD_ASSET_H
#define APP_AI_BLACKBOARD_ASSET_H 1

#include <stdexcept>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include <Logging.h>

#include <ResourceHandle.h>
#include <ResourceHolder.h>
#include <StrID.h>
#include <Blackboard_generated.h>
#include <world/ResourcePinCore.h>

namespace SE {

// Value types a blackboard slot can hold. Mirrors FlatBuffers::EBType (the .sebb
// schema); strings are deliberately absent — keys are StrIDs, values never are.
enum class EBBType : uint8_t {
        Float  = 0,
        Bool   = 1,
        Int    = 2,
        Vec3   = 3,
        Entity = 4,   // runtime-only value (TSceneNodeWeak); never authored as a default
};

// Resolved key location: `type` selects the storage array, `index` is the offset
// within it. Resolved once by the caller (BT tree load, component init); hot-path
// reads/writes are pure array ops on the ref.
struct SSlotRef {
        EBBType  type  = EBBType::Float;
        uint16_t index = 0;

        bool operator==(const SSlotRef& other) const { return type == other.type && index == other.index; }
        bool operator!=(const SSlotRef& other) const { return !(*this == other); }
};

// Shared, immutable blackboard schema built from a .sebb binary: keys + types +
// typed defaults, one table per agent class (or the world-level global).
// All data is baked from FlatBuffers at construction; the raw buffer is discarded.
// Use H<BlackboardAsset> to share across components.
class BlackboardAsset : public ResourceHolder {

public:
        struct SKey {
                StrID       key;
                std::string s_name;   // original string, debug display
                SSlotRef    slot;     // typed storage location, precomputed
                float       f_default = 0.f;
                bool        b_default = false;
                int32_t     i_default = 0;
                glm::vec3   v_default {};
        };

        // Production: loads .sebb file, builds tables, discards raw bytes.
        // Throws on any I/O, verification or validation error.
        BlackboardAsset(const std::string& sName, rid_t rid);

        // Memory mode: verifies + builds from in-memory .sebb bytes (world-declared
        // inline definitions repack to a standalone blob; tests).
        BlackboardAsset(const std::string& sName, rid_t rid, const void* pData, size_t sz);

        // Test / inline embedding: builds tables from an in-memory definition.
        BlackboardAsset(const std::string& sName, rid_t rid,
                        const FlatBuffers::BlackboardDefinition* pDef);

        const std::vector<SKey>& GetKeys() const { return vKeys; }
        uint32_t                 GetKeyCount() const { return static_cast<uint32_t>(vKeys.size()); }
        StrID                    GetId() const { return id; }

        // Linear scan over hashed keys — callers resolve once and cache the SSlotRef.
        // (Query methods are in-class inline on purpose: they carry no
        // resource-manager instantiation, so any composition — including
        // spawner-only ones without the BlackboardAsset resource — can compile
        // and link them; only the .tcc ctors/pin need the asset listed.)
        int FindSlot(StrID key) const {
                for (size_t i = 0; i < vKeys.size(); ++i) {
                        if (vKeys[i].key == key) return static_cast<int>(i);
                }
                return -1;
        }

        // Type-checked resolution: throws on unknown key or type mismatch
        // (load-time paths — BT tree load, component init).
        SSlotRef Resolve(StrID key, EBBType expected) const {
                SSlotRef out;
                if (!TryResolve(key, expected, out)) {
                        throw std::runtime_error(fmt::format(
                                        "BlackboardAsset '{}': key resolve failed (hash {})", sName,
                                        static_cast<uint64_t>(key)));
                }
                return out;
        }

        // Non-throwing variant for runtime queries.
        bool TryResolve(StrID key, EBBType expected, SSlotRef& out) const {
                const int idx = FindSlot(key);
                if (idx < 0) return false;

                const SKey& k = vKeys[static_cast<size_t>(idx)];
                if (k.slot.type != expected) return false;

                out = k.slot;
                return true;
        }

        std::string Str() const;

private:
        StrID              id;
        std::vector<SKey>  vKeys;

        void Build(const FlatBuffers::BlackboardDefinition* pDef);
};

// Declared-dependency pin (specialization lives here so its THolder is visible
// to the detection machinery; the Create body is defined in BlackboardAsset.tcc —
// an in-class body would instantiate CreateResource in every TU including this
// header, which breaks compositions whose resource typelist omits BlackboardAsset).
template <> struct TResourcePin<BlackboardAsset> {
        using THolder    = FlatBuffers::BlackboardHolder;
        using TKeyHolder = FlatBuffers::BlackboardHolderT;

        static H<BlackboardAsset> Create(const THolder* pH);
};

} // namespace SE

#endif
