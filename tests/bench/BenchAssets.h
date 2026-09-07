
#ifndef SE_BENCH_ASSETS_H
#define SE_BENCH_ASSETS_H

// ---------------------------------------------------------------------------
// BenchAssets — lazily initialized shared state for the animation benchmarks.
//
// Assets and engine systems are built ONCE per process (outside timed loops);
// per-iteration work only resets the frame allocator. The engine is the real
// composition root with selective system init (Config + EventManager +
// FrameAllocator), mirroring tests/functional/EngineFixture.h but without the
// GTest dependency.
// ---------------------------------------------------------------------------

#include <Logging.h>
#include <ErrCode.h>
#include <StrID.h>
#include <ResourceHolder.h>
#include <ResourceHandle.h>
#include <Engine.h>

#include <GlobalTypes.h>

#include <flatbuffers/flatbuffers.h>
#include <AnimationClip_generated.h>
#include <AnimationSkeleton_generated.h>
#include <AnimationGraph_generated.h>

#include <memory>
#include <string>
#include <vector>

namespace se_bench {

using FbClip  = SE::FlatBuffers::AnimationClipT;
using ClipPtr = std::unique_ptr<FbClip>;

// Real composition root, selective init (no GL/audio/input systems).
inline void EnsureEngine() {
        static bool initialized = []() {
                auto& oEngine = SE::TEngine::Instance();
                oEngine.Init<SE::Config>();
                oEngine.Init<SE::EventManager>();
                oEngine.Init<SE::FrameAllocator>();
                SE::GetSystem<SE::Config>().sResourceDir = "resource/";
                gLogger->set_level(spdlog::level::err);   // keep timed loops quiet
                return true;
        }();
        (void)initialized;
}

// ===========================================================================
// In-memory synthetic assets (same builders philosophy as the functional
// tests' AnimationAssets.h, trimmed to what the benchmarks need)
// ===========================================================================

inline ClipPtr MakeRampClipNative(float duration, uint16_t bone_count) {
        auto pClip = std::make_unique<FbClip>();
        pClip->duration = duration;
        pClip->looping  = true;
        for (uint16_t b = 0; b < bone_count; ++b) {
                for (uint8_t target : {uint8_t(0), uint8_t(3), uint8_t(7)}) {   // pos.x, rot.x, scl.x
                        auto pCh = std::make_unique<SE::FlatBuffers::CurveChannelT>();
                        pCh->bone_index = b;
                        pCh->target     = target;
                        pCh->format     = SE::FlatBuffers::CurveFormat::LinearF32;
                        pCh->times      = {0.0f, duration};
                        pCh->values     = {0.0f, 1.0f};
                        pClip->channels.push_back(std::move(pCh));
                }
        }
        return pClip;
}

inline const SE::FlatBuffers::AnimationClip* SerializeClip(
                flatbuffers::FlatBufferBuilder& oBuilder, const FbClip& oClip) {
        auto off = SE::FlatBuffers::CreateAnimationClip(oBuilder, &oClip);
        oBuilder.Finish(off, SE::FlatBuffers::AnimationClipIdentifier());
        return SE::FlatBuffers::GetAnimationClip(oBuilder.GetBufferPointer());
}

// ===========================================================================
// Shared heavy state — built once, valid for the process lifetime
// ===========================================================================

struct BenchState {
        // 65-joint shipped skeleton (private copy of the decoded resource)
        std::unique_ptr<SE::Skeleton> pSkel;

        // real shipped graph (8 states, 16 clips, 9 params)
        SE::H<SE::AnimGraph> hCharacterGraph;

        // real clips for per-clip sampling cost
        SE::AnimClip* pIdle   = nullptr;
        SE::AnimClip* pWalk   = nullptr;
        SE::AnimClip* pSprint = nullptr;
        SE::AnimClip* pJump   = nullptr;

        BenchState() {
                auto hSkel = SE::CreateResource<SE::Skeleton>(
                                "resource/animation/ual1_skeleton.sesk");
                if (SE::Skeleton* pLoaded = SE::GetResource(hSkel)) {
                        pSkel = std::make_unique<SE::Skeleton>(*pLoaded);
                }

                hCharacterGraph = SE::CreateResource<SE::AnimGraph>(
                                "resource/anim_graph/character.seag");

                auto load = [](const char* sPath) -> SE::AnimClip* {
                        auto h = SE::CreateResource<SE::AnimClip>(sPath);
                        return SE::GetResource(h);
                };
                pIdle   = load("resource/animation/ual1_Idle_Loop.seak");
                pWalk   = load("resource/animation/ual1_Walk_Loop.seak");
                pSprint = load("resource/animation/ual1_Sprint_Loop.seak");
                pJump   = load("resource/animation/ual1_Jump_Start.seak");
        }
};

inline BenchState& State() {
        EnsureEngine();
        static BenchState oState;
        return oState;
}

} // namespace se_bench

#endif // SE_BENCH_ASSETS_H
