
#ifndef APP_AI_BEHAVIOR_TREE_ASSET_H
#define APP_AI_BEHAVIOR_TREE_ASSET_H 1

#include <stdexcept>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include <Logging.h>

#include <ResourceHandle.h>
#include <ResourceHolder.h>
#include <StrID.h>
#include <MPUtil.h>

#include <ai/BehaviorTreeCommon.h>
#include <BehaviorTree_generated.h>

namespace SE {

// ---------------------------------------------------------------------------
// TBehaviorTreeAsset<TTasks> — a .sebt baked into a pointer-free node pool.
//
// Templated on the app's task typelist so "unknown task = load error" holds at
// resource-load time (the stage plan's validation point) and so every node's
// task_index is a direct table lookup at tick time. Apps derive the asset,
// component and system from ONE task-list alias (plan D3, CrowdSystemT
// precedent).
//
// Baked layout: pre-order SNode array (parent/first_child/child_count,
// decorated_child, per-kind payload) plus a contiguous SParam blob addressed
// by per-node (param_first, param_count) runs. Structural validation throws at
// load: missing root, composite without children, decorator without child,
// unknown task id, condition without key / Triggered op, ill-typed built-in
// task params, SubTree load failure or cycle.
//
// SubTree nodes exist only at authoring time: they are expanded inline at load
// (bake-at-load, EntityTemplateSystem base-id precedent) with an expansion
// stack for cycle detection — plan D8. The runtime has no SubTree kind.
// ---------------------------------------------------------------------------
template <class TTasks>
class TBehaviorTreeAsset : public ResourceHolder {

public:
        static constexpr size_t kMaxDepth = 64;   // authoring guard, hostile data

        // Production: loads the .sebt file. Throws on any I/O/verify/validation error.
        TBehaviorTreeAsset(const std::string& sName, rid_t rid);

        // Memory mode: verifies + builds from in-memory .sebt bytes (tests, inline holders).
        TBehaviorTreeAsset(const std::string& sName, rid_t rid, const void* pData, size_t sz);

        // Test / inline embedding: builds from an in-memory definition.
        TBehaviorTreeAsset(const std::string& sName, rid_t rid,
                           const FlatBuffers::BehaviorTreeDefinition* pDef);

        const std::vector<SNode>&  GetNodes()  const { return vNodes; }
        const std::vector<SParam>& GetParams() const { return vParams; }
        const std::vector<int32_t>& GetChildIndices() const { return vChildIndices; }
        uint16_t NodeCount() const { return static_cast<uint16_t>(vNodes.size()); }
        StrID    GetId()    const { return id; }

        std::string Str() const { return fmt::format("BehaviorTreeAsset['{}' nodes={}]", sName, vNodes.size()); }

private:
        StrID              id;
        std::vector<SNode>  vNodes;
        std::vector<SParam> vParams;
        std::vector<int32_t> vChildIndices;   // composite children, per child_first/count

        std::vector<std::string> vSubTreeStack;   // expansion path stack (cycle detection)

        void Build(const FlatBuffers::BehaviorTreeDefinition* pDef);
        uint32_t EmitNode(const FlatBuffers::BTNode* pNode, int32_t parent, size_t depth);
        void EmitTask(const FlatBuffers::BTTask* pTask, SNode& out);
        void EmitParams(const flatbuffers::Vector<flatbuffers::Offset<FlatBuffers::BTParam>>* pParams,
                        SNode& out);
};

} // namespace SE

#endif
