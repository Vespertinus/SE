
#ifdef SE_IMPL

#include <ai/BlackboardCore.h>

#include <EventManager.h>
#include <GlobalTypes.h>
#include <Logging.h>

#include <cassert>

namespace SE {

void BlackboardCore::Init(const BlackboardAsset* pAsset_in, TNodeWeak hOwner_in) {

        if (!pAsset_in) {
                throw std::runtime_error("BlackboardCore::Init: null asset");
        }

        pAsset = pAsset_in;
        hOwner = hOwner_in;

        const uint32_t count = pAsset->GetKeyCount();

        vFloats.clear();  vFloats.reserve(count);
        vBools.clear();   vBools.reserve(count);
        vInts.clear();    vInts.reserve(count);
        vVecs.clear();    vVecs.reserve(count);
        vEntities.clear(); vEntities.reserve(count);

        vFloatKeys.clear();  vFloatKeys.reserve(count);
        vBoolKeys.clear();   vBoolKeys.reserve(count);
        vIntKeys.clear();    vIntKeys.reserve(count);
        vVec3Keys.clear();   vVec3Keys.reserve(count);
        vEntityKeys.clear(); vEntityKeys.reserve(count);

        for (const BlackboardAsset::SKey& k : pAsset->GetKeys()) {
                switch (k.slot.type) {
                        case EBBType::Float:
                                vFloats.push_back(k.f_default);
                                vFloatKeys.push_back(k.key);
                                break;
                        case EBBType::Bool:
                                vBools.push_back(k.b_default ? 1 : 0);
                                vBoolKeys.push_back(k.key);
                                break;
                        case EBBType::Int:
                                vInts.push_back(k.i_default);
                                vIntKeys.push_back(k.key);
                                break;
                        case EBBType::Vec3:
                                vVecs.push_back(k.v_default);
                                vVec3Keys.push_back(k.key);
                                break;
                        case EBBType::Entity:
                                vEntities.push_back(TNodeWeak{});
                                vEntityKeys.push_back(k.key);
                                break;
                }
        }
}

SSlotRef BlackboardCore::Resolve(StrID key, EBBType expected) const {
        if (!pAsset) {
                throw std::runtime_error("BlackboardCore: resolve on uninitialized core");
        }
        return pAsset->Resolve(key, expected);
}

bool BlackboardCore::TryResolve(StrID key, EBBType expected, SSlotRef& out) const {
        if (!pAsset) return false;
        return pAsset->TryResolve(key, expected, out);
}

// ---- hot path: SSlotRef ----------------------------------------------------

bool BlackboardCore::SetFloat(SSlotRef slot, float value) {
        assert(slot.type == EBBType::Float);
        if (slot.type != EBBType::Float || slot.index >= vFloats.size()) return false;

        float& f = vFloats[slot.index];
        if (f == value) return false;
        f = value;
        PostChanged(vFloatKeys[slot.index]);
        return true;
}

bool BlackboardCore::SetBool(SSlotRef slot, bool value) {
        assert(slot.type == EBBType::Bool);
        if (slot.type != EBBType::Bool || slot.index >= vBools.size()) return false;

        const uint8_t b = value ? 1 : 0;
        if (vBools[slot.index] == b) return false;
        vBools[slot.index] = b;
        PostChanged(vBoolKeys[slot.index]);
        return true;
}

bool BlackboardCore::SetInt(SSlotRef slot, int32_t value) {
        assert(slot.type == EBBType::Int);
        if (slot.type != EBBType::Int || slot.index >= vInts.size()) return false;

        if (vInts[slot.index] == value) return false;
        vInts[slot.index] = value;
        PostChanged(vIntKeys[slot.index]);
        return true;
}

bool BlackboardCore::SetVec3(SSlotRef slot, const glm::vec3& vValue) {
        assert(slot.type == EBBType::Vec3);
        if (slot.type != EBBType::Vec3 || slot.index >= vVecs.size()) return false;

        const glm::vec3& v = vVecs[slot.index];
        if (v.x == vValue.x && v.y == vValue.y && v.z == vValue.z) return false;
        vVecs[slot.index] = vValue;
        PostChanged(vVec3Keys[slot.index]);
        return true;
}

bool BlackboardCore::SetEntity(SSlotRef slot, TNodeWeak hNode) {
        assert(slot.type == EBBType::Entity);
        if (slot.type != EBBType::Entity || slot.index >= vEntities.size()) return false;

        TNodeWeak& h = vEntities[slot.index];
        const bool same = !h.owner_before(hNode) && !hNode.owner_before(h);
        if (same) return false;
        h = std::move(hNode);
        PostChanged(vEntityKeys[slot.index]);
        return true;
}

float BlackboardCore::GetFloat(SSlotRef slot) const {
        assert(slot.type == EBBType::Float);
        return (slot.type == EBBType::Float && slot.index < vFloats.size())
                ? vFloats[slot.index] : 0.f;
}

bool BlackboardCore::GetBool(SSlotRef slot) const {
        assert(slot.type == EBBType::Bool);
        return (slot.type == EBBType::Bool && slot.index < vBools.size())
                ? vBools[slot.index] != 0 : false;
}

int32_t BlackboardCore::GetInt(SSlotRef slot) const {
        assert(slot.type == EBBType::Int);
        return (slot.type == EBBType::Int && slot.index < vInts.size())
                ? vInts[slot.index] : 0;
}

glm::vec3 BlackboardCore::GetVec3(SSlotRef slot) const {
        assert(slot.type == EBBType::Vec3);
        return (slot.type == EBBType::Vec3 && slot.index < vVecs.size())
                ? vVecs[slot.index] : glm::vec3{};
}

BlackboardCore::TNodeWeak BlackboardCore::GetEntity(SSlotRef slot) const {
        assert(slot.type == EBBType::Entity);
        return (slot.type == EBBType::Entity && slot.index < vEntities.size())
                ? vEntities[slot.index] : TNodeWeak{};
}

// ---- convenience: StrID ----------------------------------------------------

bool BlackboardCore::SetFloat(StrID key, float value) {
        return SetFloat(Resolve(key, EBBType::Float), value);
}

bool BlackboardCore::SetBool(StrID key, bool value) {
        return SetBool(Resolve(key, EBBType::Bool), value);
}

bool BlackboardCore::SetInt(StrID key, int32_t value) {
        return SetInt(Resolve(key, EBBType::Int), value);
}

bool BlackboardCore::SetVec3(StrID key, const glm::vec3& vValue) {
        return SetVec3(Resolve(key, EBBType::Vec3), vValue);
}

bool BlackboardCore::SetEntity(StrID key, TNodeWeak hNode) {
        return SetEntity(Resolve(key, EBBType::Entity), std::move(hNode));
}

float BlackboardCore::GetFloat(StrID key) const {
        return GetFloat(Resolve(key, EBBType::Float));
}

bool BlackboardCore::GetBool(StrID key) const {
        return GetBool(Resolve(key, EBBType::Bool));
}

int32_t BlackboardCore::GetInt(StrID key) const {
        return GetInt(Resolve(key, EBBType::Int));
}

glm::vec3 BlackboardCore::GetVec3(StrID key) const {
        return GetVec3(Resolve(key, EBBType::Vec3));
}

BlackboardCore::TNodeWeak BlackboardCore::GetEntity(StrID key) const {
        return GetEntity(Resolve(key, EBBType::Entity));
}

bool BlackboardCore::TryGetLocation(SSlotRef slot, glm::vec3& vOut) const {
        switch (slot.type) {
                case EBBType::Vec3:
                        if (slot.index >= vVecs.size()) return false;
                        vOut = vVecs[slot.index];
                        return true;
                case EBBType::Entity: {
                        if (slot.index >= vEntities.size()) return false;
                        TSceneTree::TSceneNodeExact* pNode = vEntities[slot.index].lock().get();
                        if (!pNode) return false;
                        vOut = pNode->GetTransform().GetWorldPos();
                        return true;
                }
                default:
                        return false;
        }
}

void BlackboardCore::ResetToDefaults() {

        if (!pAsset) return;

        for (const BlackboardAsset::SKey& k : pAsset->GetKeys()) {
                switch (k.slot.type) {
                        case EBBType::Float:  vFloats[k.slot.index]   = k.f_default; break;
                        case EBBType::Bool:   vBools[k.slot.index]    = k.b_default ? 1 : 0; break;
                        case EBBType::Int:    vInts[k.slot.index]     = k.i_default; break;
                        case EBBType::Vec3:   vVecs[k.slot.index]     = k.v_default; break;
                        case EBBType::Entity: vEntities[k.slot.index] = TNodeWeak{}; break;
                }
        }
}

void BlackboardCore::DeInit() {
        pAsset = nullptr;
        hOwner = TNodeWeak{};

        vFloats.clear();  vFloats.shrink_to_fit();
        vBools.clear();   vBools.shrink_to_fit();
        vInts.clear();    vInts.shrink_to_fit();
        vVecs.clear();    vVecs.shrink_to_fit();
        vEntities.clear(); vEntities.shrink_to_fit();

        vFloatKeys.clear();  vFloatKeys.shrink_to_fit();
        vBoolKeys.clear();   vBoolKeys.shrink_to_fit();
        vIntKeys.clear();    vIntKeys.shrink_to_fit();
        vVec3Keys.clear();   vVec3Keys.shrink_to_fit();
        vEntityKeys.clear(); vEntityKeys.shrink_to_fit();
}

BlackboardCore::SSnapshot BlackboardCore::Snapshot() const {
        SSnapshot out;
        out.vFloats   = vFloats;
        out.vBools    = vBools;
        out.vInts     = vInts;
        out.vVecs     = vVecs;
        out.vEntities = vEntities;   // weak_ptr copies — refcount-safe by construction
        return out;
}

void BlackboardCore::Restore(const SSnapshot& oSnapshot) {

        // Same-schema contract: a snapshot from another asset is an authoring bug.
        if (oSnapshot.vFloats.size()   != vFloats.size()   ||
            oSnapshot.vBools.size()    != vBools.size()    ||
            oSnapshot.vInts.size()     != vInts.size()     ||
            oSnapshot.vVecs.size()     != vVecs.size()     ||
            oSnapshot.vEntities.size() != vEntities.size()) {
                throw std::runtime_error(
                                fmt::format("BlackboardCore::Restore: snapshot shape mismatch "
                                            "(floats {}/{}, bools {}/{}, ints {}/{}, vec3s {}/{}, entities {}/{})",
                                            oSnapshot.vFloats.size(),   vFloats.size(),
                                            oSnapshot.vBools.size(),    vBools.size(),
                                            oSnapshot.vInts.size(),     vInts.size(),
                                            oSnapshot.vVecs.size(),     vVecs.size(),
                                            oSnapshot.vEntities.size(), vEntities.size()));
        }

        // Bulk state surgery: no change events (watchers poll; they re-read).
        vFloats   = oSnapshot.vFloats;
        vBools    = oSnapshot.vBools;
        vInts     = oSnapshot.vInts;
        vVecs     = oSnapshot.vVecs;
        vEntities = oSnapshot.vEntities;
}

std::string BlackboardCore::Str() const {

        if (!pAsset) return "BlackboardCore[uninitialized]";

        std::string out = fmt::format("BlackboardCore[{}]", pAsset->Name());
        for (const BlackboardAsset::SKey& k : pAsset->GetKeys()) {
                switch (k.slot.type) {
                        case EBBType::Float:  out += fmt::format(" {}={}", k.s_name, vFloats[k.slot.index]); break;
                        case EBBType::Bool:   out += fmt::format(" {}={}", k.s_name, vBools[k.slot.index] != 0); break;
                        case EBBType::Int:    out += fmt::format(" {}={}", k.s_name, vInts[k.slot.index]); break;
                        case EBBType::Vec3: {
                                const glm::vec3& v = vVecs[k.slot.index];
                                out += fmt::format(" {}=({},{},{})", k.s_name, v.x, v.y, v.z);
                                break;
                        }
                        case EBBType::Entity: out += fmt::format(" {}={}", k.s_name, !vEntities[k.slot.index].expired()); break;
                }
        }
        return out;
}

void BlackboardCore::PostChanged(StrID key) const {
        if (bSuppressEvents) return;
        GetSystem<EventManager>().QueueEvent(EBlackboardChanged{ key, hOwner });
}

} // namespace SE

#endif // SE_IMPL
