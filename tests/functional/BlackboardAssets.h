
#ifndef SE_TEST_BLACKBOARD_ASSETS_H
#define SE_TEST_BLACKBOARD_ASSETS_H

// ---------------------------------------------------------------------------
// Inline FlatBuffers object-api builders for the blackboard suites
// (WorldAssets.h convention: one <Subsystem>Assets.h per subsystem). Builders
// never touch the filesystem; writers return serialized bytes so tests write
// them to scratch paths themselves.
// ---------------------------------------------------------------------------

#include <flatbuffers/flatbuffers.h>
#include <Blackboard_generated.h>
#include <Component_generated.h>
#include <World_generated.h>

#include "WorldAssets.h"

#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace se_test {

// ---- .sebb definition builders ---------------------------------------------

inline std::unique_ptr<SE::FlatBuffers::BlackboardDefinitionT> MakeBBDef(const std::string & id) {
        auto pDef = std::make_unique<SE::FlatBuffers::BlackboardDefinitionT>();
        pDef->id = id;
        return pDef;
}

inline SE::FlatBuffers::BlackboardKeyT * AddKey(SE::FlatBuffers::BlackboardDefinitionT & oDef,
                                                const std::string & sName,
                                                const SE::FlatBuffers::EBType type) {
        auto pKey = std::make_unique<SE::FlatBuffers::BlackboardKeyT>();
        pKey->name = sName;
        pKey->type = type;
        oDef.keys.push_back(std::move(pKey));
        return oDef.keys.back().get();
}

inline std::vector<uint8_t> BakeBB(const SE::FlatBuffers::BlackboardDefinitionT & oDef) {
        flatbuffers::FlatBufferBuilder oBuilder;
        auto offset = SE::FlatBuffers::BlackboardDefinition::Pack(oBuilder, &oDef);
        SE::FlatBuffers::FinishBlackboardDefinitionBuffer(oBuilder, offset);
        const uint8_t * p = oBuilder.GetBufferPointer();
        return std::vector<uint8_t>(p, p + oBuilder.GetSize());
}

// ---- holders ----------------------------------------------------------------

inline std::unique_ptr<SE::FlatBuffers::BlackboardHolderT> MakeBBHolderPath(const std::string & sPath) {
        auto pHolder = std::make_unique<SE::FlatBuffers::BlackboardHolderT>();
        pHolder->path = sPath;
        return pHolder;
}

inline std::unique_ptr<SE::FlatBuffers::BlackboardHolderT> MakeBBHolderInline(
                const SE::FlatBuffers::BlackboardDefinitionT & oDef, const std::string & sName) {
        auto pHolder = std::make_unique<SE::FlatBuffers::BlackboardHolderT>();
        pHolder->blackboard = std::make_unique<SE::FlatBuffers::BlackboardDefinitionT>(oDef);
        pHolder->name       = sName;
        return pHolder;
}

// ---- component builders (scene authoring) -----------------------------------

// Struct-in-union Set: the raw Common.fbs wrapper structs by value
// (CrowdTest.cpp's FieldOverrideT pattern).
inline SE::FlatBuffers::BlackboardInitialT MakeInitial(
                const std::string & sKey, const float value) {
        SE::FlatBuffers::BlackboardInitialT o;
        o.key = sKey;
        o.value.Set(SE::FlatBuffers::Float(value));
        return o;
}

inline SE::FlatBuffers::BlackboardInitialT MakeInitial(
                const std::string & sKey, const bool value) {
        SE::FlatBuffers::BlackboardInitialT o;
        o.key = sKey;
        o.value.Set(SE::FlatBuffers::Bool(value));
        return o;
}

inline SE::FlatBuffers::BlackboardInitialT MakeInitial(
                const std::string & sKey, const int32_t value) {
        SE::FlatBuffers::BlackboardInitialT o;
        o.key = sKey;
        o.value.Set(SE::FlatBuffers::Int(value));
        return o;
}

inline SE::FlatBuffers::BlackboardInitialT MakeInitial(
                const std::string & sKey, const glm::vec3 & v) {
        SE::FlatBuffers::BlackboardInitialT o;
        o.key = sKey;
        o.value.Set(SE::FlatBuffers::Vec3(v.x, v.y, v.z));
        return o;
}

// Node component: Blackboard referencing `pHolder` with per-instance overrides.
inline void AddBlackboardComponent(SE::FlatBuffers::NodeT & oNode,
                                   std::unique_ptr<SE::FlatBuffers::BlackboardHolderT> pHolder,
                                   std::vector<SE::FlatBuffers::BlackboardInitialT> vInitial = {}) {
        auto pComp = std::make_unique<SE::FlatBuffers::ComponentT>();
        auto pBB   = std::make_unique<SE::FlatBuffers::BlackboardT>();
        pBB->bb    = std::move(pHolder);
        for (const auto & i : vInitial) {
                pBB->initial.push_back(std::make_unique<SE::FlatBuffers::BlackboardInitialT>(i));
        }
        pComp->component.Set(std::move(*pBB));
        oNode.components.push_back(std::move(pComp));
}

// Typed authored value for a TriggerBindingDef.
struct BBValue {
        SE::FlatBuffers::BBValueU type = SE::FlatBuffers::BBValueU::NONE;
        float     f = 0.f;
        bool      b = false;
        int32_t   i = 0;
        glm::vec3 v {};
};

inline BBValue BoolValue(const bool b)  { BBValue v; v.type = SE::FlatBuffers::BBValueU::Bool;  v.b = b; return v; }
inline BBValue FloatValue(const float f) { BBValue v; v.type = SE::FlatBuffers::BBValueU::Float; v.f = f; return v; }
inline BBValue IntValue(const int32_t i) { BBValue v; v.type = SE::FlatBuffers::BBValueU::Int;   v.i = i; return v; }
inline BBValue Vec3Value(const glm::vec3 & v3) { BBValue v; v.type = SE::FlatBuffers::BBValueU::Vec3; v.v = v3; return v; }

inline void AddTriggerBindingComponent(
                SE::FlatBuffers::NodeT & oNode,
                const std::vector<SE::FlatBuffers::TriggerBindingDefT> & vDefs) {
        auto pComp = std::make_unique<SE::FlatBuffers::ComponentT>();
        auto pTB   = std::make_unique<SE::FlatBuffers::TriggerBindingT>();
        for (const auto & d : vDefs) {
                pTB->bindings.push_back(std::make_unique<SE::FlatBuffers::TriggerBindingDefT>(d));
        }
        pComp->component.Set(std::move(*pTB));
        oNode.components.push_back(std::move(pComp));
}

inline SE::FlatBuffers::TriggerBindingDefT MakeBindingDef(
                const std::string & sEvent,
                const std::string & sKey,
                const BBValue & oValue,
                const bool global = true,
                const bool write_position = false) {
        SE::FlatBuffers::TriggerBindingDefT d;
        d.on_event       = sEvent;
        d.key            = sKey;
        d.target         = global ? SE::FlatBuffers::TriggerBBTarget::Global
                                  : SE::FlatBuffers::TriggerBBTarget::Self;
        d.write_position = write_position;

        switch (oValue.type) {
                case SE::FlatBuffers::BBValueU::Float: d.value.Set(SE::FlatBuffers::Float(oValue.f));   break;
                case SE::FlatBuffers::BBValueU::Bool:  d.value.Set(SE::FlatBuffers::Bool(oValue.b));    break;
                case SE::FlatBuffers::BBValueU::Int:   d.value.Set(SE::FlatBuffers::Int(oValue.i));     break;
                case SE::FlatBuffers::BBValueU::Vec3:
                        d.value.Set(SE::FlatBuffers::Vec3(oValue.v.x, oValue.v.y, oValue.v.z));
                        break;
                default:
                        break;
        }
        return d;
}

// ---- world helpers ----------------------------------------------------------

// Attaches the global-blackboard holder to a world (Step 9 declaration).
inline void SetGlobalBlackboard(SE::FlatBuffers::WorldT & oWorld,
                                std::unique_ptr<SE::FlatBuffers::BlackboardHolderT> pHolder) {
        oWorld.global_blackboard = std::move(pHolder);
}

// ---- direct FB-table attach helpers (component ctor needs the raw table) ----

// Packs an object-api definition into a raw table pointer (borrowed from the
// builder — keep it alive through the CreateResource call). The inline-def
// asset ctor builds straight from the table, no verification round-trip.
inline const SE::FlatBuffers::BlackboardDefinition * PackBBDef(
                flatbuffers::FlatBufferBuilder & oFbb,
                const SE::FlatBuffers::BlackboardDefinitionT & oDef) {
        auto offset = SE::FlatBuffers::BlackboardDefinition::Pack(oFbb, &oDef);
        oFbb.Finish(offset);
        return flatbuffers::GetRoot<SE::FlatBuffers::BlackboardDefinition>(oFbb.GetBufferPointer());
}

// Packs a Blackboard component payload; the returned pointer is borrowed from
// the builder's buffer — keep the builder alive through CreateComponent.
inline const SE::FlatBuffers::Blackboard * PackBlackboard(
                flatbuffers::FlatBufferBuilder & oFbb,
                std::unique_ptr<SE::FlatBuffers::BlackboardHolderT> pHolder,
                const std::vector<SE::FlatBuffers::BlackboardInitialT> & vInitial = {}) {
        SE::FlatBuffers::BlackboardT oBB;
        oBB.bb      = std::move(pHolder);
        for (const auto & i : vInitial) {
                oBB.initial.push_back(std::make_unique<SE::FlatBuffers::BlackboardInitialT>(i));
        }
        auto offset = SE::FlatBuffers::Blackboard::Pack(oFbb, &oBB);
        oFbb.Finish(offset);
        return flatbuffers::GetRoot<SE::FlatBuffers::Blackboard>(oFbb.GetBufferPointer());
}

inline const SE::FlatBuffers::TriggerBinding * PackTriggerBinding(
                flatbuffers::FlatBufferBuilder & oFbb,
                const std::vector<SE::FlatBuffers::TriggerBindingDefT> & vDefs) {
        SE::FlatBuffers::TriggerBindingT oTB;
        for (const auto & d : vDefs) {
                oTB.bindings.push_back(std::make_unique<SE::FlatBuffers::TriggerBindingDefT>(d));
        }
        auto offset = SE::FlatBuffers::TriggerBinding::Pack(oFbb, &oTB);
        oFbb.Finish(offset);
        return flatbuffers::GetRoot<SE::FlatBuffers::TriggerBinding>(oFbb.GetBufferPointer());
}

// Serializes a BB definition and writes it to a scratch path (create the
// directory in the test first — the WorldAssets.h convention); returns the
// bytes too (for in-memory round-trip comparisons).
inline std::vector<uint8_t> WriteBB(const std::string & sPath,
                                    const SE::FlatBuffers::BlackboardDefinitionT & oDef) {
        const auto vBytes = BakeBB(oDef);
        if (!sPath.empty()) {
                WriteBytes(sPath, vBytes);
        }
        return vBytes;
}

} // namespace se_test

#endif // SE_TEST_BLACKBOARD_ASSETS_H
