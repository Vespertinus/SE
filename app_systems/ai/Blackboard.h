
#ifndef APP_AI_BLACKBOARD_H
#define APP_AI_BLACKBOARD_H 1

#include <stdexcept>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include <StrID.h>
#include <ResourceHandle.h>
#include <ai/BlackboardCore.h>

namespace SE::FlatBuffers { struct Blackboard; }

namespace SE {

class BlackboardSystem;

// ---------------------------------------------------------------------------
// Blackboard — per-entity AI working memory.
//
// The schema (keys + types + defaults) is the referenced .sebb asset; this
// component holds the values. Authored `initial` overrides are validated at
// load (unknown key / type mismatch = load error, never a silent null).
// Enable()/Disable() register with BlackboardSystem (registry = debugger and
// perception iteration surface); the dtor calls Disable() (lifecycle rule).
//
// Add to TCustomComponents in the application's App.h when needed — apps
// composing Blackboard must compose BlackboardSystem (its dispatch and global
// live there), and BlackboardSystem after WorldSystem (ctor dependency).
// ---------------------------------------------------------------------------
class Blackboard {

        friend class BlackboardSystem;

public:
        using TSerialized = FlatBuffers::Blackboard;
        using TNodeWeak   = BlackboardCore::TNodeWeak;
        using SSnapshot   = BlackboardCore::SSnapshot;

        struct Desc {
                const char* asset_path;
        };

        Blackboard(TSceneTree::TSceneNodeExact* pNode, const Desc& desc);
        Blackboard(TSceneTree::TSceneNodeExact* pNode, const FlatBuffers::Blackboard* pFB);
        ~Blackboard() noexcept;

        void Enable();
        void Disable();

        TSceneTree::TSceneNodeExact* Node() const { return pNode; }

        bool                   IsInitialized() const { return oCore.IsInitialized(); }
        H<BlackboardAsset>     GetAssetHandle() const { return hAsset; }
        const BlackboardAsset* GetAsset() const;   // defined in the .tcc

        SSlotRef Resolve(StrID key, EBBType expected) const { return oCore.Resolve(key, expected); }
        bool     TryResolve(StrID key, EBBType expected, SSlotRef& out) const {
                return oCore.TryResolve(key, expected, out);
        }

        // --- hot path: SSlotRef (returns whether the value changed) -----------
        bool      SetFloat (SSlotRef slot, float value)          { return oCore.SetFloat(slot, value); }
        bool      SetBool  (SSlotRef slot, bool value)           { return oCore.SetBool(slot, value); }
        bool      SetInt   (SSlotRef slot, int32_t value)        { return oCore.SetInt(slot, value); }
        bool      SetVec3  (SSlotRef slot, const glm::vec3& v)   { return oCore.SetVec3(slot, v); }
        bool      SetEntity(SSlotRef slot, TNodeWeak hNode)      { return oCore.SetEntity(slot, std::move(hNode)); }

        float     GetFloat (SSlotRef slot) const { return oCore.GetFloat(slot); }
        bool      GetBool  (SSlotRef slot) const { return oCore.GetBool(slot); }
        int32_t   GetInt   (SSlotRef slot) const { return oCore.GetInt(slot); }
        glm::vec3 GetVec3  (SSlotRef slot) const { return oCore.GetVec3(slot); }
        TNodeWeak GetEntity(SSlotRef slot) const { return oCore.GetEntity(slot); }

        // --- convenience: StrID (throws on unknown key / type mismatch) -------
        bool      SetFloat (StrID key, float value)          { return oCore.SetFloat(key, value); }
        bool      SetBool  (StrID key, bool value)           { return oCore.SetBool(key, value); }
        bool      SetInt   (StrID key, int32_t value)        { return oCore.SetInt(key, value); }
        bool      SetVec3  (StrID key, const glm::vec3& v)   { return oCore.SetVec3(key, v); }
        bool      SetEntity(StrID key, TNodeWeak hNode)      { return oCore.SetEntity(key, std::move(hNode)); }

        float     GetFloat (StrID key) const { return oCore.GetFloat(key); }
        bool      GetBool  (StrID key) const { return oCore.GetBool(key); }
        int32_t   GetInt   (StrID key) const { return oCore.GetInt(key); }
        glm::vec3 GetVec3  (StrID key) const { return oCore.GetVec3(key); }
        TNodeWeak GetEntity(StrID key) const { return oCore.GetEntity(key); }

        // Vec3 direct; Entity → node world position; false otherwise.
        bool TryGetLocation(SSlotRef slot, glm::vec3& vOut) const { return oCore.TryGetLocation(slot, vOut); }

        // save seam.
        SSnapshot Snapshot() const { return oCore.Snapshot(); }
        void      Restore(const SSnapshot& oSnapshot) { oCore.Restore(oSnapshot); }

        BlackboardCore&       GetCore()       { return oCore; }
        const BlackboardCore& GetCore() const { return oCore; }

        std::string Str() const { return oCore.Str(); }

private:
        void Init(const char* szAssetPath, const FlatBuffers::BlackboardDefinition* pInlineDef);

        void ApplyInitial(const FlatBuffers::Blackboard* pFB);

        TSceneTree::TSceneNodeExact* pNode = nullptr;
        H<BlackboardAsset>           hAsset;
        BlackboardCore               oCore;
};

} // namespace SE

#endif
