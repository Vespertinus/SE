
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#define SE_IMPL
#include <Logging.h>
#include <EventManager.h>
#include <EventManager.tcc>

#include <loki/Singleton.h>

#include <Global.h>
#include <ErrCode.h>
#include <Engine.h>

#include <WorldProcess.h>
#include <WorldProcess.tcc>

using ::testing::Exactly;

namespace SE {

template <class TSystem> TSystem & GetSystem();

using EngineBase = Engine<EventManager>;

using TEngine = typename Loki::SingletonHolder< EngineBase, Loki::CreateUsingNew, Loki::DeletableSingleton >;

template <class TSystem> TSystem & GetSystem() {

        return TEngine::Instance().Get<TSystem>();
}

}

#include <WorldProcessManager.h>
#include <WorldProcessManager.tcc>

class WorldProcessManagerTest : public ::testing::Test {

        protected:

        WorldProcessManagerTest() {

                SE::TEngine::Instance().Init();
        }

        ~WorldProcessManagerTest() noexcept {

                Loki::DeletableSingleton<SE::EngineBase>::GracefulDelete();
        };

        public:
};

class ProcessMock : public SE::WorldProcess {

        public:
        ProcessMock(allocator_type oAlloc) {};
        MOCK_METHOD1(OnUpdate, void(const float) );
};

TEST_F(WorldProcessManagerTest, ProcessManagement) {

        SE::WorldProcessManager oManager;

        auto pProcess = oManager.CreateAndLink<ProcessMock>();
        EXPECT_TRUE(pProcess);

        EXPECT_CALL(*pProcess.get(), OnUpdate(0.16f)).Times(Exactly(1));

        SE::GetSystem<SE::EventManager>().TriggerEvent(SE::EUpdate{0.16f});

        EXPECT_TRUE(pProcess->IsAlive());

        auto pProcess2 = oManager.CreateAndLink<ProcessMock>();
        EXPECT_TRUE(pProcess2);

        EXPECT_EQ(oManager.Count(), 2);

        pProcess->Abort();

        EXPECT_CALL(*pProcess2.get(), OnUpdate(0.16f)).Times(Exactly(1));

        SE::GetSystem<SE::EventManager>().TriggerEvent(SE::EUpdate{0.16f});

        EXPECT_EQ(oManager.Count(), 1);
        EXPECT_TRUE(pProcess->IsDead());

        oManager.Clean();

        EXPECT_EQ(oManager.Count(), 0);
}

TEST_F(WorldProcessManagerTest, ProcesChaining) {

        SE::WorldProcessManager oManager;

        auto pProcess  = oManager.CreateAndLink<ProcessMock>();
        auto pProcess2 = oManager.Create<ProcessMock>();
        EXPECT_TRUE(pProcess);
        EXPECT_TRUE(pProcess2);

        pProcess->SetChild(pProcess2);

        EXPECT_TRUE(pProcess->GetChild());

        EXPECT_CALL(*pProcess.get(), OnUpdate(0.16f)).Times(Exactly(1));
        SE::GetSystem<SE::EventManager>().TriggerEvent(SE::EUpdate{0.16f});

        pProcess->Abort();
        SE::GetSystem<SE::EventManager>().TriggerEvent(SE::EUpdate{0.16f});
        EXPECT_EQ(oManager.Count(), 0);


        pProcess  = oManager.CreateAndLink<ProcessMock>();
        pProcess2 = oManager.Create<ProcessMock>();
        pProcess->SetChild(pProcess2);

        EXPECT_CALL(*pProcess.get(), OnUpdate(0.16f)).Times(Exactly(1));
        SE::GetSystem<SE::EventManager>().TriggerEvent(SE::EUpdate{0.16f});

        pProcess->Succeed();

        EXPECT_CALL(*pProcess2.get(), OnUpdate(0.16f)).Times(Exactly(1));
        SE::GetSystem<SE::EventManager>().TriggerEvent(SE::EUpdate{0.16f});

        EXPECT_TRUE(pProcess->IsDead());
        EXPECT_TRUE(pProcess2->IsAlive());

        pProcess2->Pause();
        SE::GetSystem<SE::EventManager>().TriggerEvent(SE::EUpdate{0.16f});

        pProcess2->UnPause();
        EXPECT_CALL(*pProcess2.get(), OnUpdate(0.16f)).Times(Exactly(1));
        SE::GetSystem<SE::EventManager>().TriggerEvent(SE::EUpdate{0.16f});

        pProcess2->Fail();
        SE::GetSystem<SE::EventManager>().TriggerEvent(SE::EUpdate{0.16f});

        EXPECT_EQ(oManager.Count(), 0);
}

