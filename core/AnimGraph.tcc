
#ifdef SE_IMPL

#include <AnimGraph.h>
#include <Logging.h>

#include <AnimationGraph_generated.h>
#include <flatbuffers/flatbuffers.h>

#include <fstream>
#include <vector>

namespace SE {

// ---------------------------------------------------------------------------
// Path-based constructor — read file into vRawData, parse FlatBuffer in-place
// ---------------------------------------------------------------------------
AnimGraph::AnimGraph(const std::string& sName, rid_t rid)
        : ResourceHolder(rid, sName) {

        std::ifstream f(sName, std::ios::binary | std::ios::ate);
        if (!f.is_open()) {
                log_e("AnimGraph: failed to open '{}'", sName);
                return;
        }
        size_t sz = static_cast<size_t>(f.tellg());
        f.seekg(0);
        vRawData.resize(sz);
        f.read(reinterpret_cast<char*>(vRawData.data()), static_cast<std::streamsize>(sz));

        // Verify "SEAG" file identifier
        flatbuffers::Verifier verifier(vRawData.data(), vRawData.size());
        if (!SE::FlatBuffers::VerifyAnimationGraphBuffer(verifier)) {
                log_e("AnimGraph: FlatBuffer verify failed for '{}'", sName);
                vRawData.clear();
                return;
        }

        pFB = SE::FlatBuffers::GetAnimationGraph(vRawData.data());
        size = static_cast<uint32_t>(sz);

        log_d("AnimGraph: loaded '{}' ({} bytes)", sName, sz);
}

// ---------------------------------------------------------------------------
// Re-serialize one AnimationGraph table into a fresh builder.
// All strings/vectors are pre-created as named offsets before any table is
// started, avoiding the C++ argument-evaluation-order hazard.
// ---------------------------------------------------------------------------
static flatbuffers::Offset<SE::FlatBuffers::AnimationGraph>
RebuildAnimGraph(flatbuffers::FlatBufferBuilder& b,
                 const SE::FlatBuffers::AnimationGraph* src) {

        using namespace SE::FlatBuffers;

        // ---- params ----
        flatbuffers::Offset<flatbuffers::Vector<
                flatbuffers::Offset<AnimParam>>> params_off = 0;
        if (src->params() && src->params()->size() > 0) {
                std::vector<flatbuffers::Offset<AnimParam>> vParams;
                vParams.reserve(src->params()->size());
                for (const auto* p : *src->params()) {
                        if (!p || !p->name()) continue;
                        auto name_off = b.CreateString(p->name());
                        vParams.push_back(CreateAnimParam(b, name_off,
                                p->type(), p->float_val(), p->bool_val(), p->int_val()));
                }
                params_off = b.CreateVector(vParams);
        }

        // ---- states ----
        std::vector<flatbuffers::Offset<AnimState>> vStates;
        if (src->states()) {
                vStates.reserve(src->states()->size());
                for (const auto* st : *src->states()) {
                        if (!st) continue;

                        std::vector<flatbuffers::Offset<BlendTreeNode>> vNodes;
                        if (st->nodes()) {
                                vNodes.reserve(st->nodes()->size());
                                for (const auto* n : *st->nodes()) {
                                        if (!n) continue;
                                        flatbuffers::Offset<void> data_off = 0;
                                        BlendNodeDataU data_type = n->data_type();

                                        switch (data_type) {
                                        case BlendNodeDataU::ClipNodeData: {
                                                auto* c = n->data_as_ClipNodeData();
                                                auto path_off = c->clip_path()
                                                        ? b.CreateString(c->clip_path()) : 0;
                                                data_off = CreateClipNodeData(b, path_off,
                                                        c->playback_rate(), c->mirror()).Union();
                                                break;
                                        }
                                        case BlendNodeDataU::Blend1DNodeData: {
                                                auto* c = n->data_as_Blend1DNodeData();
                                                auto param_off  = c->parameter()
                                                        ? b.CreateString(c->parameter()) : 0;
                                                auto thresh_off = c->thresholds()
                                                        ? b.CreateVector(c->thresholds()->data(),
                                                                         c->thresholds()->size())
                                                        : flatbuffers::Offset<flatbuffers::Vector<float>>(0);
                                                auto child_off  = c->child_indices()
                                                        ? b.CreateVector(c->child_indices()->data(),
                                                                         c->child_indices()->size())
                                                        : flatbuffers::Offset<flatbuffers::Vector<uint16_t>>(0);
                                                data_off = CreateBlend1DNodeData(b, param_off,
                                                        thresh_off, child_off).Union();
                                                break;
                                        }
                                        case BlendNodeDataU::Blend2DNodeData: {
                                                auto* c = n->data_as_Blend2DNodeData();
                                                auto px_off = c->param_x()
                                                        ? b.CreateString(c->param_x()) : 0;
                                                auto py_off = c->param_y()
                                                        ? b.CreateString(c->param_y()) : 0;
                                                flatbuffers::Offset<flatbuffers::Vector<
                                                        const BlendPoint2D*>> pos_off = 0;
                                                if (c->positions() && c->positions()->size() > 0) {
                                                        std::vector<BlendPoint2D> pts;
                                                        pts.reserve(c->positions()->size());
                                                        for (uint32_t pi = 0; pi < c->positions()->size(); ++pi) {
                                                                const auto* p = c->positions()->Get(pi);
                                                                pts.emplace_back(p->x(), p->y());
                                                        }
                                                        pos_off = b.CreateVectorOfStructs(
                                                                pts.data(), pts.size());
                                                }
                                                auto child_off = c->child_indices()
                                                        ? b.CreateVector(c->child_indices()->data(),
                                                                         c->child_indices()->size())
                                                        : flatbuffers::Offset<flatbuffers::Vector<uint16_t>>(0);
                                                data_off = CreateBlend2DNodeData(b, px_off, py_off,
                                                        pos_off, child_off, c->algorithm()).Union();
                                                break;
                                        }
                                        case BlendNodeDataU::AdditiveNodeData: {
                                                auto* c = n->data_as_AdditiveNodeData();
                                                auto wparam_off = (c->weight_param()
                                                                   && c->weight_param()->size() > 0)
                                                        ? b.CreateString(c->weight_param()) : 0;
                                                data_off = CreateAdditiveNodeData(b,
                                                        c->base_index(), c->additive_index(),
                                                        c->weight(), wparam_off).Union();
                                                break;
                                        }
                                        case BlendNodeDataU::LayerNodeData: {
                                                auto* c = n->data_as_LayerNodeData();
                                                auto mask_off   = c->mask_name()
                                                        ? b.CreateString(c->mask_name()) : 0;
                                                auto wparam_off = (c->weight_param()
                                                                   && c->weight_param()->size() > 0)
                                                        ? b.CreateString(c->weight_param()) : 0;
                                                data_off = CreateLayerNodeData(b,
                                                        c->base_index(), c->layer_index(),
                                                        mask_off, c->weight(), wparam_off,
                                                        c->blend_mode()).Union();
                                                break;
                                        }
                                        default:
                                                log_w("RebuildAnimGraph: unknown BlendNodeDataU "
                                                      "variant {}, skipping node",
                                                      static_cast<int>(data_type));
                                                break;
                                        }

                                        if (!data_off.IsNull())
                                                vNodes.push_back(CreateBlendTreeNode(b, data_type, data_off));
                                }
                        }

                        auto id_off    = st->id() ? b.CreateString(st->id()) : 0;
                        auto nodes_off = b.CreateVector(vNodes);
                        vStates.push_back(CreateAnimState(b, id_off, nodes_off,
                                                          st->speed(), st->mirror()));
                }
        }
        auto states_off = b.CreateVector(vStates);

        // ---- transitions ----
        flatbuffers::Offset<flatbuffers::Vector<
                flatbuffers::Offset<AnimTransition>>> trans_off = 0;
        if (src->transitions() && src->transitions()->size() > 0) {
                std::vector<flatbuffers::Offset<AnimTransition>> vTrans;
                vTrans.reserve(src->transitions()->size());
                for (const auto* t : *src->transitions()) {
                        if (!t) continue;
                        flatbuffers::Offset<flatbuffers::Vector<
                                flatbuffers::Offset<AnimCondition>>> cond_off = 0;
                        if (t->conditions() && t->conditions()->size() > 0) {
                                std::vector<flatbuffers::Offset<AnimCondition>> vConds;
                                vConds.reserve(t->conditions()->size());
                                for (const auto* c : *t->conditions()) {
                                        if (!c || !c->parameter()) continue;
                                        auto param_off = b.CreateString(c->parameter());
                                        vConds.push_back(CreateAnimCondition(b, param_off,
                                                c->op(), c->threshold()));
                                }
                                cond_off = b.CreateVector(vConds);
                        }
                        auto from_off = t->from() ? b.CreateString(t->from()) : 0;
                        auto to_off   = t->to()   ? b.CreateString(t->to())   : 0;
                        vTrans.push_back(CreateAnimTransition(b, from_off, to_off,
                                t->duration(), t->exit_time(),
                                t->has_exit_time(), t->can_interrupt(),
                                t->mode(), cond_off));
                }
                trans_off = b.CreateVector(vTrans);
        }

        // ---- entry_state ----
        auto entry_off = src->entry_state() ? b.CreateString(src->entry_state()) : 0;

        return CreateAnimationGraph(b,
                src->schema_version(), params_off, states_off, trans_off, entry_off);
}

// ---------------------------------------------------------------------------
// Inline FlatBuffer constructor — copies the graph into vRawData so the caller
// does not need to keep its buffer alive after this constructor returns.
// ---------------------------------------------------------------------------
AnimGraph::AnimGraph(const std::string& sName, rid_t rid,
                     const SE::FlatBuffers::AnimationGraph* pSrcFB)
        : ResourceHolder(rid, sName) {

        if (!pSrcFB) {
                log_e("AnimGraph: null inline AnimationGraph pointer for '{}'", sName);
                return;
        }

        flatbuffers::FlatBufferBuilder builder(4096);
        auto graph_off = RebuildAnimGraph(builder, pSrcFB);
        builder.Finish(graph_off);

        const uint8_t* buf = builder.GetBufferPointer();
        vRawData.assign(buf, buf + builder.GetSize());

        pFB = SE::FlatBuffers::GetAnimationGraph(vRawData.data());
        size = static_cast<uint32_t>(vRawData.size());

        log_d("AnimGraph: copied inline graph '{}' ({} bytes)", sName, vRawData.size());
}

// ---------------------------------------------------------------------------
std::string AnimGraph::Str() const {
        const char* entry = (pFB && pFB->entry_state()) ? pFB->entry_state()->c_str() : "<null>";
        const size_t state_count = (pFB && pFB->states()) ? pFB->states()->size() : 0u;

        return fmt::format("AnimGraph['{}' entry='{}' states={} bytes={}]",
                        sName, 
                        entry,
                        state_count,
                        vRawData.size());
}

} // namespace SE

#endif // SE_IMPL
