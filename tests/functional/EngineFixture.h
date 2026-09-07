
#ifndef SE_TEST_ENGINE_FIXTURE_H
#define SE_TEST_ENGINE_FIXTURE_H

// ---------------------------------------------------------------------------
// EngineFixture — Tier B/C support over the REAL engine composition root.
//
// AnimGraphRuntime.tcc includes GlobalTypes.h, so any test TU that drives
// AnimGraphInstance uses the production TEngine / TResourceManager /
// TSceneTree typedefs (higher fidelity than a mock: same resource manager,
// same Config paths, same code the samples run).
//
// Systems are initialized SELECTIVELY, never via the blanket Engine::Init():
// AudioSystem's default ctor spawns an OpenAL thread, and renderer/input
// systems are meaningless without a device. Animation needs only
// Config + EventManager + FrameAllocator; TResourceManager is its own
// process-wide singleton that self-constructs on the first CreateResource.
//
// Isolation between tests: inline clip/graph data referenced by name is cached
// by TResourceManager for the process lifetime, so Tier B fixtures clear the
// animation resource pools in TearDown. Keep resource names unique per test
// anyway — reading tests should not depend on teardown order.
// ---------------------------------------------------------------------------

#include <gtest/gtest.h>

#include <Logging.h>
#include <ErrCode.h>
#include <StrID.h>
#include <ResourceHolder.h>
#include <ResourceHandle.h>
#include <Engine.h>

// The real composition root. GlobalTypes.h ends with AllImpl.tcc, which pulls
// the SE_IMPL impls of the core systems — including AnimGraphRuntime.tcc and
// EventManager.tcc via SystemsImpl.tcc. Do NOT include those .tcc files here
// directly: they have no include guards, and a second inclusion redefines
// AnimGraphInstance::kEmptyName and the AnimParamStore methods.
#include <GlobalTypes.h>

#include <AnimationAssets.h>

namespace se_test {

// One-time, idempotent engine bootstrap for the process.
inline void EnsureEngine() {
        static bool initialized = []() {
                auto& oEngine = SE::TEngine::Instance();
                oEngine.Init<SE::Config>();
                oEngine.Init<SE::EventManager>();
                oEngine.Init<SE::FrameAllocator>();
                SE::GetSystem<SE::Config>().sResourceDir = "resource/";
                return true;
        }();
        (void)initialized;
}

// Base fixture for Tier B/C suites.
class FuncTestBase : public ::testing::Test {

protected:
        void SetUp() override {
                EnsureEngine();
        }

        void TearDown() override {
                // Drop cached animation resources so tests stay isolated from
                // each other despite the process-wide resource manager.
                SE::TResourceManager::Instance().Clear<SE::AnimClip>();
                SE::TResourceManager::Instance().Clear<SE::Skeleton>();
                SE::TResourceManager::Instance().Clear<SE::AnimGraph>();
        }

        static SE::FrameAllocator& Alloc()  { return SE::GetSystem<SE::FrameAllocator>(); }
        static SE::EventManager&   Events() { return SE::GetSystem<SE::EventManager>(); }
        static SE::Config&         Cfg()    { return SE::GetSystem<SE::Config>(); }

        // Register an in-memory skeleton and return its live resource pointer.
        SE::Skeleton* AddSkeleton(const SkelPtr& pSkel) {
                flatbuffers::FlatBufferBuilder oBuilder;
                auto h = SE::CreateResource<SE::Skeleton>(
                                "test_skeleton", SerializeSkeleton(oBuilder, *pSkel));
                return SE::GetResource(h);
        }
};

} // namespace se_test

#endif // SE_TEST_ENGINE_FIXTURE_H
