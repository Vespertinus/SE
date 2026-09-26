
#ifdef SE_IMPL

#ifndef APP_AI_BEHAVIORTREEASSET_TCC
#define APP_AI_BEHAVIORTREEASSET_TCC 1


#include <ai/BehaviorTreeAsset.h>
#include <ai/BehaviorTreeTasks.h>
#include <Logging.h>

#include <flatbuffers/flatbuffers.h>

#include <fstream>

namespace SE {

template <class TTasks>
TBehaviorTreeAsset<TTasks>::TBehaviorTreeAsset(const std::string& sName, rid_t rid)
        : ResourceHolder(rid, sName) {

        std::ifstream f(sName, std::ios::binary | std::ios::ate);
        if (!f.is_open()) {
                throw std::runtime_error(fmt::format("BehaviorTreeAsset: failed to open '{}'", sName));
        }

        const size_t sz = static_cast<size_t>(f.tellg());
        f.seekg(0);
        std::vector<uint8_t> raw(sz);
        f.read(reinterpret_cast<char*>(raw.data()), static_cast<std::streamsize>(sz));

        flatbuffers::Verifier verifier(raw.data(), raw.size());
        if (!SE::FlatBuffers::VerifyBehaviorTreeDefinitionBuffer(verifier)) {
                throw std::runtime_error(
                                fmt::format("BehaviorTreeAsset: FlatBuffer verify failed for '{}'", sName));
        }

        Build(SE::FlatBuffers::GetBehaviorTreeDefinition(raw.data()));
        size = static_cast<uint32_t>(sz);

        log_d("BehaviorTreeAsset: loaded '{}' ({} bytes, {} nodes)", sName, sz, vNodes.size());
}

template <class TTasks>
TBehaviorTreeAsset<TTasks>::TBehaviorTreeAsset(const std::string& sName, rid_t rid,
                                               const void* pData, size_t sz)
        : ResourceHolder(rid, sName) {

        if (!pData || sz == 0) {
                throw std::runtime_error(fmt::format("BehaviorTreeAsset: empty byte payload for '{}'", sName));
        }

        flatbuffers::Verifier verifier(static_cast<const uint8_t*>(pData), sz);
        if (!SE::FlatBuffers::VerifyBehaviorTreeDefinitionBuffer(verifier)) {
                throw std::runtime_error(
                                fmt::format("BehaviorTreeAsset: FlatBuffer verify failed for '{}'", sName));
        }

        Build(SE::FlatBuffers::GetBehaviorTreeDefinition(pData));
        size = static_cast<uint32_t>(sz);

        log_d("BehaviorTreeAsset: built from bytes '{}' ({} nodes)", sName, vNodes.size());
}

template <class TTasks>
TBehaviorTreeAsset<TTasks>::TBehaviorTreeAsset(const std::string& sName, rid_t rid,
                                               const FlatBuffers::BehaviorTreeDefinition* pDef)
        : ResourceHolder(rid, sName) {

        Build(pDef);
        log_d("BehaviorTreeAsset: built inline '{}' ({} nodes)", sName, vNodes.size());
}

template <class TTasks>
void TBehaviorTreeAsset<TTasks>::Build(const FlatBuffers::BehaviorTreeDefinition* pDef) {

        if (!pDef) {
                throw std::runtime_error(fmt::format("BehaviorTreeAsset '{}': null definition", sName));
        }
        if (!pDef->root()) {
                throw std::runtime_error(fmt::format("BehaviorTreeAsset '{}': no root node", sName));
        }

        if (pDef->id()) id = StrID(pDef->id()->str());

        EmitNode(pDef->root(), -1, 0);
}

template <class TTasks>
uint32_t TBehaviorTreeAsset<TTasks>::EmitNode(const FlatBuffers::BTNode* pNode,
                                              int32_t parent, size_t depth) {

        if (!pNode || !pNode->type()) {
                throw std::runtime_error(fmt::format("BehaviorTreeAsset '{}': null node", sName));
        }
        if (depth >= kMaxDepth) {
                throw std::runtime_error(fmt::format(
                                "BehaviorTreeAsset '{}': tree deeper than {} (cycle?)", sName, kMaxDepth));
        }

        const uint32_t self = static_cast<uint32_t>(vNodes.size());
        vNodes.emplace_back();
        SNode& node = vNodes.back();

        node.parent = parent;
        if (pNode->name() && pNode->name()->size() > 0) node.name = StrID(pNode->name()->str());

        switch (pNode->type_type()) {

                case FlatBuffers::BTNodeU::BTComposite: {
                        node.type = FlatBuffers::BTNodeU::BTComposite;
                        const auto* pComp = pNode->value_as_BTComposite();
                        if (!pComp || !pComp->children() || pComp->children()->size() == 0) {
                                throw std::runtime_error(fmt::format(
                                                "BehaviorTreeAsset '{}': composite node '{}' has no children",
                                                sName, pNode->name() ? pNode->name()->str() : "?"));
                        }
                        node.composite_kind = pComp->kind();
                        node.n_required     = static_cast<uint16_t>(
                                        pComp->n_required() < 0 ? 0 : pComp->n_required());
                        // Children are NOT contiguous in the node array (a child's
                        // own subtree interleaves) — record their indices in the
                        // flat child-index list. The slots are RESERVED first so
                        // nested subtrees' entries never land inside this range.
                        // Copy the list position out first: the recursion below
                        // reallocates vNodes, invalidating the `node` reference.
                        const int32_t  child_first = static_cast<int32_t>(vChildIndices.size());
                        const uint16_t child_count = static_cast<uint16_t>(pComp->children()->size());
                        node.child_first = child_first;
                        node.child_count = child_count;
                        vChildIndices.resize(vChildIndices.size() + child_count, -1);
                        for (uint16_t k = 0; k < child_count; ++k) {
                                vChildIndices[child_first + k] = static_cast<int32_t>(
                                                EmitNode(pComp->children()->Get(k),
                                                         static_cast<int32_t>(self), depth + 1));
                        }
                        break;
                }

                case FlatBuffers::BTNodeU::BTDecorator: {
                        node.type = FlatBuffers::BTNodeU::BTDecorator;
                        const auto* pDec = pNode->value_as_BTDecorator();
                        if (!pDec || !pDec->child()) {
                                throw std::runtime_error(fmt::format(
                                                "BehaviorTreeAsset '{}': decorator node '{}' has no child",
                                                sName, pNode->name() ? pNode->name()->str() : "?"));
                        }

                        node.decorator_kind = pDec->kind();
                        node.abort          = pDec->abort();
                        node.op             = pDec->op();
                        node.force_to       = pDec->force_to();
                        node.threshold      = pDec->threshold();
                        node.duration       = pDec->duration();
                        node.count          = pDec->count();
                        if (node.duration <= 0.f &&
                            (node.decorator_kind == FlatBuffers::EBTDecoratorKind::Cooldown ||
                             node.decorator_kind == FlatBuffers::EBTDecoratorKind::TimeLimit)) {
                                throw std::runtime_error(fmt::format(
                                                "BehaviorTreeAsset '{}': decorator node '{}' needs duration > 0",
                                                sName, pNode->name() ? pNode->name()->str() : "?"));
                        }
                        if (pDec->key() && pDec->key()->size() > 0) {
                                node.cond_key = StrID(pDec->key()->str());
                        } else if (node.decorator_kind ==
                                   FlatBuffers::EBTDecoratorKind::BlackboardCondition) {
                                throw std::runtime_error(fmt::format(
                                                "BehaviorTreeAsset '{}': BlackboardCondition node '{}' has no key",
                                                sName, pNode->name() ? pNode->name()->str() : "?"));
                        }
                        if (node.op == FlatBuffers::SMConditionOp::Triggered) {
                                throw std::runtime_error(fmt::format(
                                                "BehaviorTreeAsset '{}': decorator node '{}' uses BT-meaningless "
                                                "'Triggered' op", sName,
                                                pNode->name() ? pNode->name()->str() : "?"));
                        }

                        // Reserve the child slot AFTER the decorator itself. The
                        // assignment must precede the recursion — `node` dangles
                        // once vNodes reallocates.
                        node.decorated_child = static_cast<int32_t>(vNodes.size());
                        EmitNode(pDec->child(), static_cast<int32_t>(self), depth + 1);
                        break;
                }

                case FlatBuffers::BTNodeU::BTTask: {
                        node.type = FlatBuffers::BTNodeU::BTTask;
                        EmitTask(pNode->value_as_BTTask(), node);
                        break;
                }

                case FlatBuffers::BTNodeU::BTSubTree: {
                        const auto* pSub = pNode->value_as_BTSubTree();
                        if (!pSub || !pSub->path() || pSub->path()->size() == 0) {
                                throw std::runtime_error(fmt::format(
                                                "BehaviorTreeAsset '{}': SubTree node with no path", sName));
                        }

                        const std::string sSubPath = pSub->path()->str();
                        for (const auto& s : vSubTreeStack) {
                                if (s == sSubPath) {
                                        throw std::runtime_error(fmt::format(
                                                        "BehaviorTreeAsset '{}': SubTree cycle at '{}'",
                                                        sName, sSubPath));
                                }
                        }

                        std::ifstream f(sSubPath, std::ios::binary | std::ios::ate);
                        if (!f.is_open()) {
                                throw std::runtime_error(fmt::format(
                                                "BehaviorTreeAsset '{}': SubTree '{}' failed to open",
                                                sName, sSubPath));
                        }
                        const size_t sz = static_cast<size_t>(f.tellg());
                        f.seekg(0);
                        std::vector<uint8_t> raw(sz);
                        f.read(reinterpret_cast<char*>(raw.data()), static_cast<std::streamsize>(sz));

                        flatbuffers::Verifier verifier(raw.data(), raw.size());
                        if (!SE::FlatBuffers::VerifyBehaviorTreeDefinitionBuffer(verifier)) {
                                throw std::runtime_error(fmt::format(
                                                "BehaviorTreeAsset '{}': SubTree '{}' verify failed",
                                                sName, sSubPath));
                        }

                        // Splice: the referenced tree's root takes this node's place.
                        // Undo this call's slot reservation first — the SubTree
                        // node itself does not survive baking.
                        vNodes.pop_back();
                        vSubTreeStack.push_back(sSubPath);
                        const uint32_t emitted =
                                EmitNode(SE::FlatBuffers::GetBehaviorTreeDefinition(raw.data())->root(),
                                         parent, depth + 1);
                        vSubTreeStack.pop_back();
                        return emitted;
                }

                default:
                        throw std::runtime_error(fmt::format(
                                        "BehaviorTreeAsset '{}': node '{}' has unknown type", sName,
                                        pNode->name() ? pNode->name()->str() : "?"));
        }

        return self;
}

template <class TTasks>
void TBehaviorTreeAsset<TTasks>::EmitTask(const FlatBuffers::BTTask* pTask, SNode& node) {

        if (!pTask || !pTask->task_id()) {
                throw std::runtime_error(fmt::format("BehaviorTreeAsset '{}': task node with no task_id",
                                                     sName));
        }

        const std::string sTaskId = pTask->task_id()->str();
        const int idx = TTaskTable<TTasks>::IndexOf(StrID(sTaskId));
        if (idx < 0) {
                throw std::runtime_error(fmt::format(
                                "BehaviorTreeAsset '{}': unknown task id '{}' (not in the app's task table)",
                                sName, sTaskId));
        }
        node.task_index = static_cast<uint16_t>(idx);

        // Load-time validation of built-in task params (game tasks validate at
        // Init — plan D4).
        TTaskTable<TTasks>::At(node.task_index).fnValidate(pTask);

        EmitParams(pTask->params(), node);
}

template <class TTasks>
void TBehaviorTreeAsset<TTasks>::EmitParams(
                const flatbuffers::Vector<flatbuffers::Offset<FlatBuffers::BTParam>>* pParams,
                SNode& out) {

        if (!pParams || pParams->size() == 0) return;

        out.param_first = static_cast<int32_t>(vParams.size());
        out.param_count = static_cast<uint16_t>(pParams->size());

        for (const auto* pParam : *pParams) {
                if (!pParam || !pParam->key()) {
                        throw std::runtime_error(fmt::format("BehaviorTreeAsset '{}': param with no key",
                                                             sName));
                }

                SParam p;
                p.key = StrID(pParam->key()->str());

                using VU = FlatBuffers::BTParamValue;
                switch (pParam->value_type()) {
                        case VU::Float: {
                                const auto* v = pParam->value_as_Float();
                                p.type = EBTParamType::Float;
                                if (v) p.f = v->value();
                                break;
                        }
                        case VU::Bool: {
                                const auto* v = pParam->value_as_Bool();
                                p.type = EBTParamType::Bool;
                                if (v) p.b = v->value();
                                break;
                        }
                        case VU::Int: {
                                const auto* v = pParam->value_as_Int();
                                p.type = EBTParamType::Int;
                                if (v) p.i = v->value();
                                break;
                        }
                        case VU::BTParamString: {
                                const auto* v = pParam->value_as_BTParamString();
                                p.type = EBTParamType::String;
                                if (v && v->value()) p.s = v->value()->str();
                                break;
                        }
                        case VU::Vec3: {
                                const auto* v = pParam->value_as_Vec3();
                                p.type = EBTParamType::Vec3;
                                if (v) {
                                        p.v = glm::vec3(v->x(), v->y(), v->z());
                                }
                                break;
                        }
                        default:
                                throw std::runtime_error(fmt::format(
                                                "BehaviorTreeAsset '{}': param '{}' has unknown value type",
                                                sName, pParam->key()->str()));
                }

                vParams.push_back(std::move(p));
        }
}

} // namespace SE

#endif // SE_IMPL

#endif // APP_AI_BEHAVIORTREEASSET_TCC
