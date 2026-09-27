// ---------------------------------------------------------------------------
// WorldDemoBTTest — the real .sebt brains over the real blackboards:
// villager patrol reads its waypoint keys, the bell key preempts patrol via
// the LowerPriority abort (investigate branch), clearing the key returns to
// patrol, and the camp guard chases player_pos while the alarm is raised.
// ---------------------------------------------------------------------------

#define SE_IMPL
#include "WorldDemoHarness.h"

#include <ai/Blackboard.h>
#include <ai/BehaviorTree.h>
#include <nav/NavAgent.h>
#include <nav/PathFollower.h>
#include <nav/NavigationSystem.h>

using namespace SE;

namespace {

// A minimal agent: NavAgent + PathFollower + Blackboard + the real .sebt.
TSceneTree::TSceneNode MakeAgent(WorldDemoTestBase & self, const char * szName,
                                 const char * szTree, const char * szEntityBB) {
        auto pNode = self.CreateNode(szName);
        pNode->CreateComponent<NavAgent>(NavAgentDesc{"default"});
        pNode->CreateComponent<PathFollower>(PathFollowerDesc{});
        pNode->CreateComponent<Blackboard>(Blackboard::Desc{szEntityBB});
        pNode->CreateComponent<BehaviorTree>(BehaviorTree::Desc{szTree, 0.05f});
        pNode->Enable();
        return pNode;
}

} // namespace

TEST_F(WorldDemoTestBase, VillagerPatrol_MovesTowardWaypoints) {
        ASSERT_TRUE(LoadDemoWorld());
        Drain(2);

        auto pVillager = MakeAgent(*this, "Villager",
                                   "resource/ai/world_demo/bt_villager_patrol.sebt",
                                   "resource/ai/world_demo/bb_villager.sebb");
        ASSERT_NE(pVillager, nullptr);

        auto * pBB = pVillager->GetComponent<Blackboard>();
        auto * pFollower = pVillager->GetComponent<PathFollower>();
        ASSERT_NE(pBB, nullptr);
        ASSERT_NE(pFollower, nullptr);

        // waypoint keys (the templates bake these as initial values)
        const glm::vec3 vA(-20.f, 2.f, 0.f);
        const glm::vec3 vB(16.f, 2.f, 0.f);
        pBB->GetCore().SetVec3(StrID("wp_a"), vA);
        pBB->GetCore().SetVec3(StrID("wp_b"), vB);

        RunSeconds(0.5f);

        // the brain wrote the first leg into the follower
        EXPECT_EQ(pFollower->GetStatus(), PathStatus::Following);
        EXPECT_LT(glm::length(pFollower->GetGoal() - vA), 1.0f);
}

TEST_F(WorldDemoTestBase, BellNoise_AbortsPatrol_InvestigatesAndReturns) {
        ASSERT_TRUE(LoadDemoWorld());
        Drain(2);

        auto pVillager = MakeAgent(*this, "Villager",
                                   "resource/ai/world_demo/bt_villager_patrol.sebt",
                                   "resource/ai/world_demo/bb_villager.sebb");
        auto * pBB = pVillager->GetComponent<Blackboard>();
        auto * pFollower = pVillager->GetComponent<PathFollower>();

        const glm::vec3 vA(-20.f, 2.f, 0.f);
        pBB->GetCore().SetVec3(StrID("wp_a"), vA);
        pBB->GetCore().SetVec3(StrID("wp_b"), glm::vec3(16.f, 2.f, 0.f));
        RunSeconds(0.5f);
        EXPECT_LT(glm::length(pFollower->GetGoal() - vA), 1.0f);

        // the bell: TriggerBinding writes noise_pos + alert on the global board
        auto & oGlobal = GetSystem<BlackboardSystem>().GetGlobal();
        oGlobal.SetVec3(StrID("noise_pos"), glm::vec3(2.f, 2.f, 6.f));
        oGlobal.SetBool(StrID("alert"), true);

        RunSeconds(0.5f);

        // LowerPriority abort: the investigate branch preempted the patrol leg
        EXPECT_LT(glm::length(pFollower->GetGoal() - glm::vec3(2.f, 2.f, 6.f)), 1.0f);

        // Harness agents have no locomotion, so park the noise on the agent to
        // let the investigate MoveTo arrive; LookAround (5 s) then CalmDown
        // clears the key on its own and patrol resumes at a waypoint.
        oGlobal.SetVec3(StrID("noise_pos"), glm::vec3(0.f, 2.f, 0.f));
        RunSeconds(7.0f);
        EXPECT_EQ(pFollower->GetStatus(), PathStatus::Following);
        const glm::vec3 vGoal = pFollower->GetGoal();
        EXPECT_TRUE(glm::length(vGoal - vA) < 1.0f
                 || glm::length(vGoal - glm::vec3(16.f, 2.f, 0.f)) < 1.0f);
}

TEST_F(WorldDemoTestBase, GuardChases_AlarmRaised_GivesUpWhenCleared) {
        ASSERT_TRUE(LoadDemoWorld());
        Drain(2);

        auto pGuard = MakeAgent(*this, "Guard",
                                "resource/ai/world_demo/bt_camp_guard.sebt",
                                "resource/ai/world_demo/bb_guard.sebb");
        auto * pBB = pGuard->GetComponent<Blackboard>();
        auto * pFollower = pGuard->GetComponent<PathFollower>();

        // idle: patrol loop between the two post waypoints
        const glm::vec3 vPostA(-79.f, 2.f, 1.f);
        const glm::vec3 vPostB(-72.f, 2.f, 0.5f);
        pBB->GetCore().SetVec3(StrID("post_a"), vPostA);
        pBB->GetCore().SetVec3(StrID("post_b"), vPostB);
        RunSeconds(0.5f);

        const glm::vec3 vGoalIdle = pFollower->GetGoal();
        EXPECT_TRUE(glm::length(vGoalIdle - vPostA) < 1.0f
                 || glm::length(vGoalIdle - vPostB) < 1.0f);

        // the camp trigger raises the alarm; the demo writes player_pos
        auto & oGlobal = GetSystem<BlackboardSystem>().GetGlobal();
        const glm::vec3 vPlayer(4.f, 2.f, 8.f);
        oGlobal.SetVec3(StrID("player_pos"), vPlayer);
        oGlobal.SetBool(StrID("alarm_raised"), true);
        RunSeconds(0.5f);

        EXPECT_LT(glm::length(pFollower->GetGoal() - vPlayer), 1.0f);

        // live chase: MoveTo retargets while Running when the player moves
        const glm::vec3 vPlayer2(10.f, 2.f, 12.f);
        oGlobal.SetVec3(StrID("player_pos"), vPlayer2);
        RunSeconds(0.5f);
        EXPECT_LT(glm::length(pFollower->GetGoal() - vPlayer2), 1.0f);

        // the pursuit persists — the guards do NOT drop back to patrol while
        // the alarm is raised (GiveUp only fires after the 10 chase legs)
        RunSeconds(3.0f);
        EXPECT_TRUE(oGlobal.GetBool(StrID("alarm_raised")));

        // Harness agents have no locomotion, so park the player on the guard:
        // each chase leg arrives instantly and drains at Recenter's 0.6 s —
        // after the 30th leg (ForceStatus keeps failed MoveTo legs from
        // breaking the loop early) GiveUp clears the alarm and patrol resumes.
        oGlobal.SetVec3(StrID("player_pos"), glm::vec3(0.f, 2.f, 0.f));
        RunSeconds(25.0f);
        EXPECT_FALSE(oGlobal.GetBool(StrID("alarm_raised")));
        const glm::vec3 vGoalBack = pFollower->GetGoal();
        EXPECT_TRUE(glm::length(vGoalBack - vPostA) < 1.5f
                 || glm::length(vGoalBack - vPostB) < 1.5f);
}
