
#ifdef SE_IMPL

// Xlib (via graphics headers in mixed TUs) does `#define Bool int` —
// keep it out of this file; generated union members use Bool as a
// scope-qualified identifier (same dance as the entity plugins).
#pragma push_macro("Bool")
#undef Bool

#include <ai/Blackboard.h>
#include <ai/BlackboardSystem.h>

#include <Blackboard_generated.h>
#include <Component_generated.h>
#include <Logging.h>
#include <GlobalTypes.h>

namespace SE {

Blackboard::Blackboard(TSceneTree::TSceneNodeExact* pNewNode, const Desc& desc)
        : pNode(pNewNode) {

        if (!desc.asset_path || desc.asset_path[0] == '\0')
                throw std::runtime_error("Blackboard: empty asset_path");

        Init(desc.asset_path, nullptr);
}

Blackboard::Blackboard(TSceneTree::TSceneNodeExact* pNewNode,
                       const FlatBuffers::Blackboard* pFB)
        : pNode(pNewNode) {

        if (!pFB || !pFB->bb())
                throw std::runtime_error("Blackboard: null FlatBuffer");

        const auto* bb = pFB->bb();

        if (bb->blackboard() && bb->name()) {
                // Inline embedded definition — name is the resource cache key
                Init(bb->name()->c_str(), bb->blackboard());
        } else if (bb->path() && bb->path()->size() > 0) {
                Init(bb->path()->c_str(), nullptr);
        } else {
                throw std::runtime_error(
                                "Blackboard: BlackboardHolder has neither path nor inline definition + name");
        }

        ApplyInitial(pFB);
}

Blackboard::~Blackboard() noexcept {
        Disable();
}

void Blackboard::Init(const char* szAssetPath,
                      const FlatBuffers::BlackboardDefinition* pInlineDef) {

        if (pInlineDef) {
                hAsset = CreateResource<BlackboardAsset>(szAssetPath, pInlineDef);
        } else {
                hAsset = CreateResource<BlackboardAsset>(szAssetPath);
        }

        const BlackboardAsset* pAsset = GetResource(hAsset);
        if (!pAsset) {
                throw std::runtime_error(
                                fmt::format("Blackboard: failed to load '{}'",
                                            szAssetPath ? szAssetPath : "<empty>"));
        }

        // The node handle is the change-event identity; the global blackboard
        // (BlackboardSystem) inits with an empty handle instead.
        oCore.Init(pAsset, pNode->GetShared());
}

const BlackboardAsset* Blackboard::GetAsset() const {
        return GetResource(hAsset);
}

void Blackboard::ApplyInitial(const FlatBuffers::Blackboard* pFB) {

        if (!pFB->initial() || pFB->initial()->size() == 0) return;

        const BlackboardAsset* pAsset = GetAsset();
        if (!pAsset) throw std::runtime_error("Blackboard: initial overrides on uninitialized core");

        // Load-time bulk surgery — no change events.
        oCore.SetEventsSuppressed(true);

        using VU = FlatBuffers::BBValueU;
        for (const auto* pInit : *pFB->initial()) {
                if (!pInit || !pInit->key()) continue;

                const StrID key(pInit->key()->str());
                const int   idx = pAsset->FindSlot(key);
                if (idx < 0) {
                        oCore.SetEventsSuppressed(false);
                        throw std::runtime_error(fmt::format(
                                        "Blackboard: initial override for unknown key '{}'",
                                        pInit->key()->str()));
                }
                const BlackboardAsset::SKey& k = pAsset->GetKeys()[static_cast<size_t>(idx)];

                bool applied = false;
                switch (pInit->value_type()) {
                        case VU::Float: {
                                const auto* p = pInit->value_as_Float();
                                applied = p && k.slot.type == EBBType::Float
                                        && oCore.SetFloat(k.slot, p->value());
                                break;
                        }
                        case VU::Bool: {
                                const auto* p = pInit->value_as_Bool();
                                applied = p && k.slot.type == EBBType::Bool
                                        && oCore.SetBool(k.slot, p->value());
                                break;
                        }
                        case VU::Int: {
                                const auto* p = pInit->value_as_Int();
                                applied = p && k.slot.type == EBBType::Int
                                        && oCore.SetInt(k.slot, p->value());
                                break;
                        }
                        case VU::Vec3: {
                                const auto* p = pInit->value_as_Vec3();
                                applied = p && k.slot.type == EBBType::Vec3
                                        && oCore.SetVec3(k.slot, glm::vec3(p->x(), p->y(), p->z()));
                                break;
                        }
                        default:
                                break;
                }

                if (!applied) {
                        oCore.SetEventsSuppressed(false);
                        throw std::runtime_error(fmt::format(
                                        "Blackboard: initial override for key '{}' has wrong type",
                                        pInit->key()->str()));
                }
        }

        oCore.SetEventsSuppressed(false);
}

void Blackboard::Enable() {
        GetSystem<BlackboardSystem>().AddComponent(this);
}

void Blackboard::Disable() {
        GetSystem<BlackboardSystem>().RemoveComponent(this);
}

} // namespace SE

#endif // SE_IMPL

#pragma pop_macro("Bool")
