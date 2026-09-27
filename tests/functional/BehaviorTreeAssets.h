
#ifndef SE_TEST_BEHAVIOR_TREE_ASSETS_H
#define SE_TEST_BEHAVIOR_TREE_ASSETS_H

// ---------------------------------------------------------------------------
// Inline FlatBuffers object-api builders for the behavior tree suites
// (BlackboardAssets.h convention). Builders never touch the filesystem except
// WriteBT; SubTree tests write .sebt scratch files themselves.
// ---------------------------------------------------------------------------

#include <flatbuffers/flatbuffers.h>
#include <BehaviorTree_generated.h>
#include <Blackboard_generated.h>
#include <Component_generated.h>
#include <World_generated.h>

#include "BlackboardAssets.h"

#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace se_test {

using SE::FlatBuffers::BTNodeT;

// ---- params ------------------------------------------------------------------
// Union members Float/Bool/Int/Vec3 are the Common.fbs wrapper structs (the
// BlackboardAssets convention); strings use the BTParamString table.

inline SE::FlatBuffers::BTParamT ParamF(const std::string & k, float v) {
        SE::FlatBuffers::BTParamT p; p.key = k; p.value.Set(SE::FlatBuffers::Float(v)); return p;
}
inline SE::FlatBuffers::BTParamT ParamB(const std::string & k, bool v) {
        SE::FlatBuffers::BTParamT p; p.key = k; p.value.Set(SE::FlatBuffers::Bool(v)); return p;
}
inline SE::FlatBuffers::BTParamT ParamI(const std::string & k, int32_t v) {
        SE::FlatBuffers::BTParamT p; p.key = k; p.value.Set(SE::FlatBuffers::Int(v)); return p;
}
inline SE::FlatBuffers::BTParamT ParamS(const std::string & k, const std::string & v) {
        SE::FlatBuffers::BTParamT p; p.key = k;
        auto s = std::make_unique<SE::FlatBuffers::BTParamStringT>();
        s->value = v;
        p.value.Set(std::move(*s));
        return p;
}
inline SE::FlatBuffers::BTParamT ParamV(const std::string & k, const glm::vec3 & v) {
        SE::FlatBuffers::BTParamT p; p.key = k; p.value.Set(SE::FlatBuffers::Vec3(v.x, v.y, v.z)); return p;
}

// ---- nodes -------------------------------------------------------------------
// The type/value wrapper-union pair: both object-api fields are Set from the
// payload (the Spawner SpawnCondition convention); the binary keeps the tag in
// `type` and the payload in `value`.

inline BTNodeT BTTask(const std::string & sTaskId, const std::string & sName = "",
                      std::vector<SE::FlatBuffers::BTParamT> vParams = {}) {
        BTNodeT n;
        n.name = sName;
        auto t = std::make_unique<SE::FlatBuffers::BTTaskT>();
        t->task_id = sTaskId;
        for (auto & p : vParams) t->params.push_back(std::make_unique<SE::FlatBuffers::BTParamT>(p));
        // The `type` field carries only the tag at runtime; the payload lives in `value`.
        n.type.Set(std::move(*std::make_unique<SE::FlatBuffers::BTTaskT>()));
        n.value.Set(std::move(*t));
        return n;
}

inline BTNodeT BTComposite(SE::FlatBuffers::EBTCompositeKind kind,
                           std::vector<BTNodeT> vChildren,
                           const std::string & sName = "", int n_required = 0) {
        BTNodeT n;
        n.name = sName;
        auto c = std::make_unique<SE::FlatBuffers::BTCompositeT>();
        c->kind = kind;
        c->n_required = n_required;
        for (auto & ch : vChildren) c->children.push_back(std::make_unique<BTNodeT>(std::move(ch)));
        n.type.Set(std::move(*std::make_unique<SE::FlatBuffers::BTCompositeT>()));
        n.value.Set(std::move(*c));
        return n;
}

inline BTNodeT BTSequence(std::vector<BTNodeT> vChildren, const std::string & sName = "") {
        return BTComposite(SE::FlatBuffers::EBTCompositeKind::Sequence, std::move(vChildren), sName);
}
inline BTNodeT BTSelector(std::vector<BTNodeT> vChildren, const std::string & sName = "") {
        return BTComposite(SE::FlatBuffers::EBTCompositeKind::Selector, std::move(vChildren), sName);
}
inline BTNodeT BTParallel(std::vector<BTNodeT> vChildren, int n_required,
                          const std::string & sName = "") {
        return BTComposite(SE::FlatBuffers::EBTCompositeKind::Parallel, std::move(vChildren), sName,
                           n_required);
}

// Decorator with full control (the engine-owned typed fields).
inline BTNodeT BTDecorator(SE::FlatBuffers::EBTDecoratorKind kind, BTNodeT child,
                           SE::FlatBuffers::EBTAbort abort = SE::FlatBuffers::EBTAbort::None,
                           const std::string & sKey = "",
                           SE::FlatBuffers::SMConditionOp op = SE::FlatBuffers::SMConditionOp::IsTrue,
                           float threshold = 0.f, float duration = 0.f, int count = 0,
                           SE::FlatBuffers::EBTForceTo force_to = SE::FlatBuffers::EBTForceTo::Success,
                           const std::string & sName = "") {
        BTNodeT n;
        n.name = sName;
        auto d = std::make_unique<SE::FlatBuffers::BTDecoratorT>();
        d->kind = kind;
        d->abort = abort;
        d->key = sKey;
        d->op = op;
        d->threshold = threshold;
        d->duration = duration;
        d->count = count;
        d->force_to = force_to;
        d->child = std::make_unique<BTNodeT>(std::move(child));
        n.type.Set(std::move(*std::make_unique<SE::FlatBuffers::BTDecoratorT>()));
        n.value.Set(std::move(*d));
        return n;
}

// Condition decorator with the common shape.
inline BTNodeT BTCond(const std::string & sKey, SE::FlatBuffers::SMConditionOp op, float threshold,
                      BTNodeT child,
                      SE::FlatBuffers::EBTAbort abort = SE::FlatBuffers::EBTAbort::None,
                      const std::string & sName = "") {
        return BTDecorator(SE::FlatBuffers::EBTDecoratorKind::BlackboardCondition, std::move(child),
                           abort, sKey, op, threshold, 0.f, 0,
                           SE::FlatBuffers::EBTForceTo::Success, sName);
}
inline BTNodeT BTCondBool(const std::string & sKey, bool bExpect, BTNodeT child,
                          SE::FlatBuffers::EBTAbort abort = SE::FlatBuffers::EBTAbort::None,
                          const std::string & sName = "") {
        return BTCond(sKey, bExpect ? SE::FlatBuffers::SMConditionOp::IsTrue
                                    : SE::FlatBuffers::SMConditionOp::IsFalse,
                      0.f, std::move(child), abort, sName);
}
inline BTNodeT BTCooldown(float fDuration, BTNodeT child, const std::string & sName = "") {
        return BTDecorator(SE::FlatBuffers::EBTDecoratorKind::Cooldown, std::move(child),
                           SE::FlatBuffers::EBTAbort::None, "", SE::FlatBuffers::SMConditionOp::IsTrue,
                           0.f, fDuration, 0, SE::FlatBuffers::EBTForceTo::Success, sName);
}
inline BTNodeT BTTimeLimit(float fDuration, BTNodeT child, const std::string & sName = "") {
        return BTDecorator(SE::FlatBuffers::EBTDecoratorKind::TimeLimit, std::move(child),
                           SE::FlatBuffers::EBTAbort::None, "", SE::FlatBuffers::SMConditionOp::IsTrue,
                           0.f, fDuration, 0, SE::FlatBuffers::EBTForceTo::Success, sName);
}
inline BTNodeT BTLoop(int nCount, BTNodeT child, const std::string & sName = "") {
        return BTDecorator(SE::FlatBuffers::EBTDecoratorKind::Loop, std::move(child),
                           SE::FlatBuffers::EBTAbort::None, "", SE::FlatBuffers::SMConditionOp::IsTrue,
                           0.f, 0.f, nCount, SE::FlatBuffers::EBTForceTo::Success, sName);
}
inline BTNodeT BTForce(SE::FlatBuffers::EBTForceTo to, BTNodeT child, const std::string & sName = "") {
        return BTDecorator(SE::FlatBuffers::EBTDecoratorKind::ForceStatus, std::move(child),
                           SE::FlatBuffers::EBTAbort::None, "", SE::FlatBuffers::SMConditionOp::IsTrue,
                           0.f, 0.f, 0, to, sName);
}
inline BTNodeT BTSubTreeRef(const std::string & sPath) {
        BTNodeT n;
        auto s = std::make_unique<SE::FlatBuffers::BTSubTreeT>();
        s->path = sPath;
        n.type.Set(std::move(*std::make_unique<SE::FlatBuffers::BTSubTreeT>()));
        n.value.Set(std::move(*s));
        return n;
}

// ---- definition + serialization ----------------------------------------------

inline std::unique_ptr<SE::FlatBuffers::BehaviorTreeDefinitionT> MakeBTDef(const std::string & id,
                                                                           BTNodeT root) {
        auto pDef = std::make_unique<SE::FlatBuffers::BehaviorTreeDefinitionT>();
        pDef->id   = id;
        pDef->root = std::make_unique<BTNodeT>(std::move(root));
        return pDef;
}

inline std::vector<uint8_t> BakeBT(const SE::FlatBuffers::BehaviorTreeDefinitionT & oDef) {
        flatbuffers::FlatBufferBuilder oBuilder;
        auto offset = SE::FlatBuffers::BehaviorTreeDefinition::Pack(oBuilder, &oDef);
        SE::FlatBuffers::FinishBehaviorTreeDefinitionBuffer(oBuilder, offset);
        const uint8_t * p = oBuilder.GetBufferPointer();
        return std::vector<uint8_t>(p, p + oBuilder.GetSize());
}

// Writes the bytes and returns them (scratch fixture under resource/ai/gen).
inline std::vector<uint8_t> WriteBT(const std::string & sPath,
                                    const SE::FlatBuffers::BehaviorTreeDefinitionT & oDef) {
        const auto vBytes = BakeBT(oDef);
        std::ofstream f(sPath, std::ios::binary);
        f.write(reinterpret_cast<const char *>(vBytes.data()),
                static_cast<std::streamsize>(vBytes.size()));
        return vBytes;
}

// ---- holders + scene authoring -------------------------------------------------

inline std::unique_ptr<SE::FlatBuffers::BehaviorTreeHolderT> MakeBTHolderPath(const std::string & sPath) {
        auto pHolder = std::make_unique<SE::FlatBuffers::BehaviorTreeHolderT>();
        pHolder->path = sPath;
        return pHolder;
}
inline std::unique_ptr<SE::FlatBuffers::BehaviorTreeHolderT> MakeBTHolderInline(
                const SE::FlatBuffers::BehaviorTreeDefinitionT & oDef, const std::string & sName = "") {
        // ResourceManager dedups by name — default to a UNIQUE cache name per
        // call so two inline holders never collide on the same tree resource.
        static uint32_t sUnique = 0;
        const std::string sCache = sName.empty()
                ? oDef.id + "_inline_" + std::to_string(++sUnique)
                : sName;
        auto pHolder = std::make_unique<SE::FlatBuffers::BehaviorTreeHolderT>();
        pHolder->behavior_tree = std::make_unique<SE::FlatBuffers::BehaviorTreeDefinitionT>(oDef);
        pHolder->name = sCache;
        return pHolder;
}

// Node component: BehaviorTree referencing `pHolder` with a tick interval.
inline void AddBehaviorTreeComponent(SE::FlatBuffers::NodeT & oNode,
                                     std::unique_ptr<SE::FlatBuffers::BehaviorTreeHolderT> pHolder,
                                     float fTickInterval = 0.f) {
        auto pComp = std::make_unique<SE::FlatBuffers::ComponentT>();
        auto pBT   = std::make_unique<SE::FlatBuffers::BehaviorTreeT>();
        pBT->tree = std::move(pHolder);
        pBT->tick_interval = fTickInterval;
        pComp->component.Set(std::move(*pBT));
        oNode.components.push_back(std::move(pComp));
}

} // namespace se_test

#endif
