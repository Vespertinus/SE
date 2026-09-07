
// ---------------------------------------------------------------------------
// Animation subsystem benchmarks (google/benchmark).
//
// One TU for the SE_IMPL engine blocks (see tests/CMakeLists.txt). Real
// assets load once via BenchAssets.h; timed loops only evaluate and reset the
// frame allocator. Run from the repo root (asset paths), e.g.:
//   ./anim_bench --benchmark_format=json --benchmark_out=anim_bench.json
//   ./anim_bench --benchmark_min_time=0.05s        (smoke)
// ---------------------------------------------------------------------------

#include <benchmark/benchmark.h>
#include <glm/glm.hpp>

#define SE_IMPL
#include <BenchAssets.h>

// NOTE: BenchAssets.h pulls GlobalTypes.h, whose AllImpl.tcc already compiles
// AnimGraphRuntime.tcc / EventManager.tcc — do not include those here twice.
#include <AnimEvaluator.h>

namespace {

using namespace se_bench;

// FrameAllocator used by all timed loops (sized above any pressure benchmark).
constexpr size_t kAllocCapacity = 16 * 1024 * 1024;

SE::FrameAllocator& BenchAlloc() {
        static SE::FrameAllocator oAlloc(kAllocCapacity);
        return oAlloc;
}

// ===========================================================================
// In-memory synthetic graphs (root node is always index 0)
// ===========================================================================

using GraphPtrB = std::unique_ptr<SE::FlatBuffers::AnimationGraphT>;

GraphPtrB MakeSingleClipGraph(ClipPtr pClip, const char* sName) {
        std::vector<SE::FlatBuffers::BlendTreeNodeT> vNodes;
        SE::FlatBuffers::ClipNodeDataT oData;
        auto pHolder = std::make_unique<SE::FlatBuffers::AnimClipHolderT>();
        pHolder->name = sName;
        pHolder->clip = std::move(pClip);
        oData.clip = std::move(pHolder);
        SE::FlatBuffers::BlendTreeNodeT oNode;
        oNode.data.Set(std::move(oData));
        vNodes.push_back(std::move(oNode));
        std::vector<SE::FlatBuffers::AnimStateT> vStates;
        SE::FlatBuffers::AnimStateT oState;
        oState.id = "s";
        oState.nodes.push_back(
                        std::make_unique<SE::FlatBuffers::BlendTreeNodeT>(std::move(vNodes[0])));
        vStates.push_back(std::move(oState));
        auto pGraph = std::make_unique<SE::FlatBuffers::AnimationGraphT>();
        pGraph->entry_state = "s";
        pGraph->states.push_back(std::make_unique<SE::FlatBuffers::AnimStateT>(std::move(vStates[0])));
        return pGraph;
}

GraphPtrB MakeBlend1DGraphB(std::vector<ClipPtr> vpClips) {
        std::vector<SE::FlatBuffers::BlendTreeNodeT> vNodes;
        vNodes.push_back(SE::FlatBuffers::BlendTreeNodeT{});   // placeholder root
        std::vector<uint16_t> vChildren;
        for (uint32_t i = 0; i < vpClips.size(); ++i) {
                SE::FlatBuffers::ClipNodeDataT oData;
                auto pHolder = std::make_unique<SE::FlatBuffers::AnimClipHolderT>();
                pHolder->name = "b1d_" + std::to_string(i);
                pHolder->clip = std::move(vpClips[i]);
                oData.clip = std::move(pHolder);
                SE::FlatBuffers::BlendTreeNodeT oNode;
                oNode.data.Set(std::move(oData));
                vNodes.push_back(std::move(oNode));
                vChildren.push_back(static_cast<uint16_t>(vNodes.size() - 1));
        }
        {
                SE::FlatBuffers::Blend1DNodeDataT oData;
                oData.parameter     = "speed";
                oData.thresholds    = {0.0f, 1.0f, 2.0f};
                oData.child_indices = vChildren;
                SE::FlatBuffers::BlendTreeNodeT oNode;
                oNode.data.Set(std::move(oData));
                vNodes[0] = std::move(oNode);
        }
        std::vector<SE::FlatBuffers::AnimStateT> vStates;
        SE::FlatBuffers::AnimStateT oStateT;
        oStateT.id = "s";
        for (auto& oNode : vNodes) {
                oStateT.nodes.push_back(
                                std::make_unique<SE::FlatBuffers::BlendTreeNodeT>(std::move(oNode)));
        }
        vStates.push_back(std::move(oStateT));
        auto pGraph = std::make_unique<SE::FlatBuffers::AnimationGraphT>();
        pGraph->entry_state = "s";
        pGraph->states.push_back(std::make_unique<SE::FlatBuffers::AnimStateT>(std::move(vStates[0])));
        auto pParam = std::make_unique<SE::FlatBuffers::AnimParamT>();
        pParam->name = "speed";
        pParam->type = SE::FlatBuffers::AnimParamType::Float;
        pParam->float_val = 0.0f;
        pGraph->params.push_back(std::move(pParam));
        return pGraph;
}

GraphPtrB MakeCrossfadeGraph(ClipPtr pClipA, ClipPtr pClipB) {
        auto makeLeaf = [](const char* sName, ClipPtr pClip) {
                SE::FlatBuffers::ClipNodeDataT oData;
                auto pHolder = std::make_unique<SE::FlatBuffers::AnimClipHolderT>();
                pHolder->name = sName;
                pHolder->clip = std::move(pClip);
                oData.clip = std::move(pHolder);
                SE::FlatBuffers::BlendTreeNodeT oNode;
                oNode.data.Set(std::move(oData));
                return oNode;
        };
        auto pGraph = std::make_unique<SE::FlatBuffers::AnimationGraphT>();
        pGraph->entry_state = "a";
        for (const char* sId : {"a", "b"}) {
                SE::FlatBuffers::AnimStateT oStateT;
                oStateT.id = sId;
                oStateT.nodes.push_back(std::make_unique<SE::FlatBuffers::BlendTreeNodeT>(
                                sId[0] == 'a' ? makeLeaf("cf_a", std::move(pClipA))
                                              : makeLeaf("cf_b", std::move(pClipB))));
                pGraph->states.push_back(std::make_unique<SE::FlatBuffers::AnimStateT>(
                                std::move(oStateT)));
        }
        SE::FlatBuffers::AnimTransitionT oTr;
        oTr.from = "a";
        oTr.to = "b";
        oTr.duration = 0.2f;
        pGraph->transitions.push_back(std::make_unique<SE::FlatBuffers::AnimTransitionT>(
                        std::move(oTr)));
        return pGraph;
}

// Serialize + build a live AnimGraph resource and Init an instance on it.
struct GraphInstance {
        flatbuffers::FlatBufferBuilder oBuilder;
        std::unique_ptr<SE::AnimGraph> pGraph;
        SE::AnimGraphInstance          inst;

        explicit GraphInstance(const GraphPtrB& pNative) {
                auto off = SE::FlatBuffers::CreateAnimationGraph(oBuilder, pNative.get());
                oBuilder.Finish(off, SE::FlatBuffers::AnimationGraphIdentifier());
                pGraph = std::make_unique<SE::AnimGraph>(
                                "bench_graph", 0,
                                SE::FlatBuffers::GetAnimationGraph(oBuilder.GetBufferPointer()));
                inst.Init(*pGraph);
        }
};

// ===========================================================================
// Curve / clip sampling primitives
// ===========================================================================

static void BM_SampleCurve_Format(benchmark::State& oState) {
        const int keys = static_cast<int>(oState.range(1));
        SE::AnimClip::CurveChannel oCh;
        switch (oState.range(0)) {
                case 0: oCh.format = SE::AnimClip::Format::ConstantF32; break;
                case 1: oCh.format = SE::AnimClip::Format::StepF32;     break;
                case 2: oCh.format = SE::AnimClip::Format::LinearF32;   break;
                default: oCh.format = SE::AnimClip::Format::HermiteF32; break;
        }
        oCh.vTimes.resize(keys);
        oCh.vValues.resize(keys);
        if (oCh.format == SE::AnimClip::Format::HermiteF32) oCh.vTangents.assign(keys, 0.5f);
        for (int i = 0; i < keys; ++i) {
                oCh.vTimes[i]  = static_cast<float>(i);
                oCh.vValues[i] = static_cast<float>(i);
        }

        float val = 0.0f;
        float t   = 0.0f;
        for (auto _ : oState) {
                t = std::fmod(t + 0.37f, static_cast<float>(keys - 1) + 0.999f);   // move the search
                SE::SampleCurve(oCh, t, val);
                benchmark::DoNotOptimize(val);
        }
        oState.counters["keys"] = keys;
}
BENCHMARK(BM_SampleCurve_Format)->Args({0, 4})->Args({0, 64})->Args({0, 512});
BENCHMARK(BM_SampleCurve_Format)->Args({1, 4})->Args({1, 64})->Args({1, 512});
BENCHMARK(BM_SampleCurve_Format)->Args({2, 4})->Args({2, 64})->Args({2, 512});
BENCHMARK(BM_SampleCurve_Format)->Args({3, 4})->Args({3, 64})->Args({3, 512});

// Real shipped clips: per-sample cost across the actual content spread.
static void BM_SampleClip_RealSeak(benchmark::State& oState) {
        BenchState& oBench = State();
        SE::AnimClip* pClip = nullptr;
        switch (oState.range(0)) {
                case 0: pClip = oBench.pIdle;   break;
                case 1: pClip = oBench.pWalk;   break;
                case 2: pClip = oBench.pSprint; break;
                default: pClip = oBench.pJump;  break;
        }
        if (!pClip || !oBench.pSkel) { oState.SkipWithError("asset missing"); return; }
        oState.counters["clip_channels"] = pClip->Channels().size();
        oState.counters["clip_dur_s"]    = pClip->Duration();

        SE::FrameAllocator& oAlloc = BenchAlloc();
        SE::LocalPose oPose = SE::AllocatePose(oBench.pSkel->BoneCount(), oAlloc);
        SE::InitBindPose(oPose, *oBench.pSkel);

        float t = 0.0f;
        for (auto _ : oState) {
                t = std::fmod(t + 0.137f, pClip->Duration());
                SE::SampleClip(*pClip, t, oPose);
                benchmark::DoNotOptimize(oPose.pPos[1]);
        }
        oAlloc.reset();
}
BENCHMARK(BM_SampleClip_RealSeak)->Arg(0)->Arg(1)->Arg(2)->Arg(3);

// Synthetic clip: 3 channels per bone — channel-count scaling axis.
static void BM_SampleClip_Synthetic(benchmark::State& oState) {
        const uint16_t bones = static_cast<uint16_t>(oState.range(0));
        BenchState& oBench = State();
        auto pClip = MakeRampClipNative(1.0f, bones);
        flatbuffers::FlatBufferBuilder oClipBuilder;
        SE::AnimClip oClip("synthetic", 0, SerializeClip(oClipBuilder, *pClip));
        oState.counters["clip_channels"] = oClip.Channels().size();

        SE::FrameAllocator& oAlloc = BenchAlloc();
        SE::LocalPose oPose = SE::AllocatePose(bones, oAlloc);
        float t = 0.0f;
        for (auto _ : oState) {
                t = std::fmod(t + 0.137f, 1.0f);
                SE::SampleClip(oClip, t, oPose);
                benchmark::DoNotOptimize(oPose.pPos[0]);
        }
        oAlloc.reset();
}
BENCHMARK(BM_SampleClip_Synthetic)->Arg(8)->Arg(32)->Arg(65)->Arg(128)->Arg(256);

// ===========================================================================
// Pose math
// ===========================================================================

static void BM_BlendPoses(benchmark::State& oState) {
        const uint32_t bones = static_cast<uint32_t>(oState.range(0));
        SE::FrameAllocator& oAlloc = BenchAlloc();
        SE::LocalPose oA = SE::AllocatePose(bones, oAlloc);
        SE::LocalPose oB = SE::AllocatePose(bones, oAlloc);
        SE::LocalPose oOut = SE::AllocatePose(bones, oAlloc);
        SE::InitBindPose(oA, *State().pSkel);
        SE::InitBindPose(oB, *State().pSkel);

        for (auto _ : oState) {
                SE::BlendPoses(oA, oB, 0.5f, oOut);
                benchmark::DoNotOptimize(oOut.pPos[0]);
        }
        oState.counters["bones"] = static_cast<double>(bones);
        oAlloc.reset();
}
BENCHMARK(BM_BlendPoses)->Arg(8)->Arg(32)->Arg(65)->Arg(128)->Arg(256)->Arg(512);

static void BM_RenormalizeRotations(benchmark::State& oState) {
        const uint32_t bones = static_cast<uint32_t>(oState.range(0));
        SE::FrameAllocator& oAlloc = BenchAlloc();
        SE::LocalPose oPose = SE::AllocatePose(bones, oAlloc);
        SE::InitBindPose(oPose, *State().pSkel);

        for (auto _ : oState) {
                SE::RenormalizeRotations(oPose);
                benchmark::DoNotOptimize(oPose.pRot[0]);
        }
        oAlloc.reset();
}
BENCHMARK(BM_RenormalizeRotations)->Arg(65)->Arg(128)->Arg(256);

static void BM_AllocatePose(benchmark::State& oState) {
        const uint32_t bones = static_cast<uint32_t>(oState.range(0));
        SE::FrameAllocator& oAlloc = BenchAlloc();
        for (auto _ : oState) {
                oAlloc.reset();
                SE::LocalPose oPose = SE::AllocatePose(bones, oAlloc);
                benchmark::DoNotOptimize(oPose.pPos);
        }
        oState.counters["high_water_kb"] = static_cast<double>(oAlloc.high_water()) / 1024.0;
        oAlloc.reset();
}
BENCHMARK(BM_AllocatePose)->Arg(65)->Arg(128);

// Frame allocator pressure of C simultaneous 65-bone crossfades per frame
// (2 state poses + 1 blended pose each) against a 4 MB budget.
static void BM_AllocPressure_Crossfade(benchmark::State& oState) {
        const int chars = static_cast<int>(oState.range(0));
        SE::FrameAllocator oFrame(4 * 1024 * 1024);   // engine default size
        const uint32_t bones = 65;

        for (auto _ : oState) {
                oFrame.reset();
                for (int c = 0; c < chars; ++c) {
                        SE::LocalPose oSrc = SE::AllocatePose(bones, oFrame);
                        SE::LocalPose oDst = SE::AllocatePose(bones, oFrame);
                        SE::LocalPose oOut = SE::AllocatePose(bones, oFrame);
                        SE::BlendPoses(oSrc, oDst, 0.5f, oOut);
                        benchmark::DoNotOptimize(oOut.pPos);
                }
        }
        oState.counters["chars"] = chars;
        oState.counters["high_water_kb"] = static_cast<double>(oFrame.high_water()) / 1024.0;
}
BENCHMARK(BM_AllocPressure_Crossfade)->Arg(1)->Arg(8)->Arg(32)->Arg(128);

// ===========================================================================
// Graph evaluation (65-bone synthetic graphs)
// ===========================================================================

static void BM_GraphEval_SingleClip(benchmark::State& oState) {
        BenchState& oBench = State();
        if (!oBench.pSkel) { oState.SkipWithError("skeleton missing"); return; }
        GraphInstance oRig(MakeSingleClipGraph(
                        MakeRampClipNative(1.0f, 65), "bench_single"));
        SE::FrameAllocator& oAlloc = BenchAlloc();
        for (auto _ : oState) {
                oAlloc.reset();
                SE::LocalPose oPose = SE::AllocatePose(65, oAlloc);
                SE::InitBindPose(oPose, *oBench.pSkel);
                oRig.inst.Update(1.0f / 60.0f);
                oRig.inst.EvaluateBlendTree(1.0f, oPose, oAlloc, *oBench.pSkel);
                SE::RenormalizeRotations(oPose);
                benchmark::DoNotOptimize(oPose.pPos[0]);
        }
}
BENCHMARK(BM_GraphEval_SingleClip);

static void BM_GraphEval_Blend1D(benchmark::State& oState) {
        BenchState& oBench = State();
        if (!oBench.pSkel) { oState.SkipWithError("skeleton missing"); return; }
        std::vector<ClipPtr> vpClips;
        vpClips.push_back(MakeRampClipNative(1.0f, 65));
        vpClips.push_back(MakeRampClipNative(1.0f, 65));
        vpClips.push_back(MakeRampClipNative(1.0f, 65));
        GraphInstance oRig(MakeBlend1DGraphB(std::move(vpClips)));
        oRig.inst.SetFloat(SE::StrID("speed"), 0.5f);
        SE::FrameAllocator& oAlloc = BenchAlloc();
        for (auto _ : oState) {
                oAlloc.reset();
                SE::LocalPose oPose = SE::AllocatePose(65, oAlloc);
                SE::InitBindPose(oPose, *oBench.pSkel);
                oRig.inst.Update(1.0f / 60.0f);
                oRig.inst.EvaluateBlendTree(1.0f, oPose, oAlloc, *oBench.pSkel);
                SE::RenormalizeRotations(oPose);
                benchmark::DoNotOptimize(oPose.pPos[0]);
        }
}
BENCHMARK(BM_GraphEval_Blend1D);

static void BM_GraphEval_Crossfade(benchmark::State& oState) {
        BenchState& oBench = State();
        if (!oBench.pSkel) { oState.SkipWithError("skeleton missing"); return; }
        GraphInstance oRig(MakeCrossfadeGraph(
                        MakeRampClipNative(1.0f, 65), MakeRampClipNative(1.0f, 65)));
        oRig.inst.SetFloat(SE::StrID("speed"), 1.0f);   // arm a -> b on first Update
        SE::FrameAllocator& oAlloc = BenchAlloc();
        for (auto _ : oState) {
                oAlloc.reset();
                SE::LocalPose oPose = SE::AllocatePose(65, oAlloc);
                SE::InitBindPose(oPose, *oBench.pSkel);
                oRig.inst.Update(1.0f / 60.0f);
                oRig.inst.EvaluateBlendTree(1.0f, oPose, oAlloc, *oBench.pSkel);
                SE::RenormalizeRotations(oPose);
                benchmark::DoNotOptimize(oPose.pPos[0]);
        }
        // NOTE: the crossfade completes after 0.2 s of simulated steps; the
        // loop then measures the steady single-state path. Re-arm is not
        // needed for the steady-state cost this benchmark reports.
}
BENCHMARK(BM_GraphEval_Crossfade);

// ===========================================================================
// Headline: real character.seag per-frame update, N concurrent characters
// ===========================================================================

static void BM_FrameUpdate_CharacterGraph(benchmark::State& oState) {
        BenchState& oBench = State();
        if (!oBench.pSkel || !oBench.hCharacterGraph.IsValid()) {
                oState.SkipWithError("real assets missing");
                return;
        }
        SE::AnimGraph* pGraph = SE::GetResource(oBench.hCharacterGraph);
        const int chars = static_cast<int>(oState.range(0));

        struct CharRig {
                SE::AnimGraphInstance inst;
                float phase = 0.0f;
        };
        std::vector<CharRig> vChars(chars);
        for (int c = 0; c < chars; ++c) {
                vChars[c].inst.Init(*pGraph);
                vChars[c].phase = static_cast<float>(c) * 0.37f;   // desync the scripts
        }

        SE::FrameAllocator& oAlloc = BenchAlloc();
        SE::LocalPose oPose = SE::AllocatePose(oBench.pSkel->BoneCount(), oAlloc);
        uint32_t frame = 0;
        uint64_t total_evals = 0;
        for (auto _ : oState) {
                oAlloc.reset();
                for (auto& oChar : vChars) {
                        // scripted per-character activity: speed ramp + jump
                        const float speed = 1.0f + std::sin(static_cast<float>(frame + oChar.phase * 60) * 0.1f);
                        oChar.inst.SetFloat(SE::StrID("speed"), speed);
                        oChar.inst.Update(1.0f / 60.0f);
                        if ((frame % 240) == 0) {
                                oChar.inst.SetTrigger(SE::StrID("jump"));
                        }
                        oChar.inst.EvaluateBlendTree(1.0f, oPose, oAlloc, *oBench.pSkel);
                        SE::RenormalizeRotations(oPose);
                        ++total_evals;
                        benchmark::DoNotOptimize(oPose.pPos[0]);
                }
                ++frame;
        }
        oState.counters["chars"] = chars;
        // derived chars-at-16.6ms is computed from the reported ns/iteration:
        // chars_at_16_6ms = 16.6e6 / time_per_frame_ns
        oAlloc.reset();
}
BENCHMARK(BM_FrameUpdate_CharacterGraph)->Arg(1)->Arg(4)->Arg(16)->Arg(64);

// ===========================================================================
// Decode path canary
// ===========================================================================

static void BM_ResourceLoad_RealSeak(benchmark::State& oState) {
        BenchState& oBench = State();
        (void)oBench;
        const char* sPath = "resource/animation/ual1_Walk_Loop.seak";
        auto h = SE::CreateResource<SE::AnimClip>(sPath);
        for (auto _ : oState) {
                // Drop the cached resource outside the timed window so every
                // measured iteration covers open + verify + decode + insert.
                oState.PauseTiming();
                SE::DestroyResource(h);
                SE::TResourceManager::Instance().ProcessDeferred();
                oState.ResumeTiming();
                h = SE::CreateResource<SE::AnimClip>(sPath);
                benchmark::DoNotOptimize(h);
        }
        SE::DestroyResource(h);
        SE::TResourceManager::Instance().ProcessDeferred();
}
BENCHMARK(BM_ResourceLoad_RealSeak);

} // namespace
