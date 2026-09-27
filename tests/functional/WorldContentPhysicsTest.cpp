// ---------------------------------------------------------------------------
// WorldContentPhysicsTest — Tier C+. Guards the AUTHORED demo content: the
// real wd_village.sesc must carry RigidBody colliders on its buildings (the
// round-6 review found window/door wall tiles shipping without them), the
// door leaves must have their kinematic bodies, and a character driven at a
// village wall must actually be blocked. Runs against the composed scene the
// demo streams — if this regresses, players walk through houses again.
// ---------------------------------------------------------------------------

#define SE_IMPL
#include <GlobalTypes.h>

#include <SceneTree_generated.h>
#include <Component_generated.h>

#include <ai/BlackboardSystem.h>
#include <ai/Blackboard.h>

#include "PhysicsHarness.h"

#include <gtest/gtest.h>

#include <functional>
#include <fstream>
#include <cmath>
#include <string>

using namespace se_test;
using SE::RigidBody;

namespace {

// HouseA sits at (-14, -8), 4x6 m, door on the +z side — front wall at z = -5.
constexpr float kHouseX = -14.0f;

// Instantiate the composed village sub-level (flat node list under one root).
SE::TSceneTree::TSceneNode LoadVillage(SE::TSceneTree & oTree) {
        return oTree.Instantiate("resource/world/village_gen/wd_village.sesc", nullptr);
}

RigidBody * BodyOf(SE::TSceneTree::TSceneNode pRoot, const char * szName) {
        auto pNode = pRoot ? pRoot->FindChild(SE::StrID(szName), /*recursive=*/true)
                           : nullptr;
        if (!pNode) return nullptr;
        return pNode->GetComponent<RigidBody>();
}

// The scene-load path needs the singleton PhysicsSystem (EnginePhysTestBase),
// so the character is driven through it directly: create + register a node,
// then fixed-step the singleton and sync. Node Y == character Y after sync.
struct CharProbe {
        SE::CharHandle                 handle;
        SE::TSceneTree::TSceneNodeExact * pNode = nullptr;
};

CharProbe SpawnChar(SE::TSceneTree & oTree, glm::vec3 vPos,
                    float radius = 0.3f, float half_height = 0.9f) {
        static uint32_t sCharSeq = 0;
        CharProbe c;
        SE::CharacterDesc d;
        d.vInitialPosition = vPos;
        d.radius           = radius;
        d.half_height      = half_height;
        c.handle = SE::GetSystem<SE::PhysicsSystem>().CreateCharacter(d);
        EXPECT_TRUE(c.handle.IsValid());
        auto pNode = oTree.Create("content_char_" + std::to_string(++sCharSeq));
        SE::GetSystem<SE::PhysicsSystem>().RegisterCharacterNode(c.handle, pNode.get());
        c.pNode = pNode.get();
        return c;
}

void StepN(uint32_t n) {
        for (uint32_t i = 0; i < n; ++i) {
                SE::GetSystem<SE::PhysicsSystem>().StepOnce();
                SE::GetSystem<SE::PhysicsSystem>().Interpolate();
        }
}

} // namespace

// EnginePhysTestBase plus the systems the authored village components talk to
// (SpatialEntry → SpatialIndexSystem, TriggerBinding → BlackboardSystem,
// Population → SpawnerSystem). One-time inits; never shut down — process
// lifetime, like the rest of the fixture bootstrap.
class ContentPhysTestBase : public EnginePhysTestBase {
protected:
        void SetUp() override {
                EnginePhysTestBase::SetUp();
                static bool systems_constructed = []() {
                        auto & oEngine = SE::TEngine::Instance();
                        oEngine.Init<SE::WorldSystem>();   // SpatialIndexSystem depends on it
                        oEngine.Init<SE::SpatialIndexSystem>();
                        oEngine.Init<SE::BlackboardSystem>();
                        oEngine.Init<SE::EntityManager>(); // SpawnerSystem depends on it
                        oEngine.Init<SE::SpawnerSystem>();
                        return true;
                }();
                (void)systems_constructed;
        }
};

// The wall-tile variants must ALL block movement: plain and window tiles
// carry their own box; the DOOR wall is split (round 7) — the tile itself has
// no body, its side-pillar + lintel children frame the ~1.3 m opening, and
// the kinematic leaf body covers the opening while the door is closed.
TEST_F(ContentPhysTestBase, VillageContent_WallTilesCarryColliders) {
        auto pVillage = LoadVillage(*scene);
        ASSERT_TRUE(pVillage) << "wd_village.sesc failed to instantiate";

        // plain wall tile
        auto * pWall = BodyOf(pVillage, "HouseA_F_1");
        ASSERT_NE(pWall, nullptr) << "plain wall tile lost its RigidBody";
        EXPECT_TRUE(pWall->GetHandle().IsValid());

        // window wall tile (side wall, middle slot)
        auto * pWindow = BodyOf(pVillage, "HouseA_S-1_WinW1");
        ASSERT_NE(pWindow, nullptr) << "window wall tile lost its RigidBody";
        EXPECT_TRUE(pWindow->GetHandle().IsValid());

        // door wall tile: the full-tile box must stay GONE (it sealed the
        // doorway — the round-7 "can't walk through the open door" report)
        auto pDoorWall = pVillage->FindChild(SE::StrID("HouseA_F_DoorW"), /*recursive=*/true);
        ASSERT_TRUE(pDoorWall) << "door wall tile missing";
        EXPECT_EQ(pDoorWall->GetComponent<RigidBody>(), nullptr)
                        << "door wall tile grew a full-tile box back — the doorway is sealed";

        // the split: side pillars + lintel above the opening
        for (const char * szPart : {"HouseA_F_PillarL", "HouseA_F_PillarR", "HouseA_F_Lintel"}) {
                auto * pPart = BodyOf(pVillage, szPart);
                ASSERT_NE(pPart, nullptr) << szPart << " missing";
                EXPECT_TRUE(pPart->GetHandle().IsValid());
        }

        // door leaf + kinematic leaf body
        auto pLeaf = pVillage->FindChild(SE::StrID("HouseA_F_Door"), /*recursive=*/true);
        ASSERT_TRUE(pLeaf) << "door leaf missing";
        auto * pLeafBody = BodyOf(pVillage, "HouseA_F_DoorBody");
        ASSERT_NE(pLeafBody, nullptr) << "kinematic leaf body missing";
        EXPECT_TRUE(pLeafBody->GetHandle().IsValid());
}

// The doorway itself must NOT be sealed by wall geometry: a ray through the
// opening passes the wall plane and only meets the closed leaf slab (centred
// in the wall, round 7), not a full-tile box face 20 cm in front of it.
TEST_F(ContentPhysTestBase, VillageContent_DoorwayNotSealedByWallBox) {
        auto pVillage = LoadVillage(*scene);
        ASSERT_TRUE(pVillage);

        // door tile HouseA_F_DoorW sits at x = -15, wall face plane z = -5
        SE::PhysicsRay ray;
        ray.vOrigin    = {-15.0f, 2.8f, -4.0f};   // through the opening
        ray.vDirection = {0.0f, 0.0f, -1.0f};
        SE::RaycastHit hit;
        ASSERT_TRUE(SE::GetSystem<SE::PhysicsSystem>().Raycast(ray, hit))
                        << "opening is wide open — the closed leaf body is gone too";
        // old sealing box face: z = -4.795. Leaf slab now: z in [-5.17, -5.03]
        EXPECT_LT(hit.vPoint.z, -4.95f) << "hit a box in front of the wall plane";
        EXPECT_GT(hit.vPoint.z, -5.30f);

        // the jambs still block: side pillar at the same height
        SE::PhysicsRay rayPillar;
        rayPillar.vOrigin    = {-15.9f, 2.8f, -4.0f};
        rayPillar.vDirection = {0.0f, 0.0f, -1.0f};
        SE::RaycastHit hitPillar;
        EXPECT_TRUE(SE::GetSystem<SE::PhysicsSystem>().Raycast(rayPillar, hitPillar));
        EXPECT_NEAR(hitPillar.vPoint.z, -4.795f, 0.05f);
}

// The user-visible round-9 contract: an OPEN inn door must not block the
// entry line it was opened for. The leaf is hinged at the opening's edge and
// the demo swings the inn door INWARD (Scene::SetDoorOpen, sign -1) — the
// street approach stays clear and a character walks straight through.
TEST_F(ContentPhysTestBase, VillageContent_InnDoorway_WalkThroughWhenOpen) {
        auto pVillage = LoadVillage(*scene);
        ASSERT_TRUE(pVillage);

        auto pLeaf = pVillage->FindChild(SE::StrID("Inn_GF_F_Door"), /*recursive=*/true);
        ASSERT_NE(pLeaf, nullptr) << "inn door leaf missing";

        // open the door the way the demo does for the inn: inward. Consume the
        // pending invalidation on the leaf AND its kinematic body first —
        // SetRotation's listener dispatch early-returns while either node's
        // CDirty is set from scene load, and this fixture has nothing querying
        // their worlds per frame (the demo's door scans do).
        auto pBody = pLeaf->FindChild(SE::StrID("Inn_GF_F_DoorBody"), /*recursive=*/true);
        ASSERT_NE(pBody, nullptr) << "inn door body missing";
        (void)pLeaf->GetTransform().GetWorld();
        (void)pBody->GetTransform().GetWorld();
        const float fClosed = pLeaf->GetTransform().GetRotationDeg().y;
        pLeaf->SetRotation(glm::vec3(0.f, fClosed - 90.f, 0.f));
        StepN(8);   // flush the kinematic leaf body to its swung pose

        // level walk at the floor-top height (2.05): the fixture has no
        // terrain, so a descending character would drop under the exterior
        // floor tiles and hit their side face — an artifact, not the door.
        const float approach_y = 2.05f;
        const float speed_z    = 1.5f;
        // demo player proportions (0.35/0.65 = 2.0 m) — the kit door openings
        // are 2.1 m high, so the test default (2.4 m) would wedge under the
        // lintel above the doorway
        const float door_r = 0.35f, door_hh = 0.65f;

        // 1) off-centre street approach (x = 8.8): with the old mid-opening
        //    hinge swinging OUTWARD, the leaf fin jut across this line and
        //    the character stopped in the street. It must now cross.
        auto chApproach = SpawnChar(*scene, {8.8f, approach_y, -14.6f}, door_r, door_hh);
        SE::GetSystem<SE::PhysicsSystem>().SetCharacterVelocity(chApproach.handle,
                        {0.0f, 0.0f, speed_z});
        StepN(120);
        const glm::vec3 vApproach = chApproach.pNode->GetTransform().GetWorldPos();
        EXPECT_GT(vApproach.z, -14.0f) << "open leaf still blocks the street approach";

        // 2) centred walk-through: straight through the open doorway, inside
        auto chIn = SpawnChar(*scene, {9.0f, approach_y, -14.6f}, door_r, door_hh);
        SE::GetSystem<SE::PhysicsSystem>().SetCharacterVelocity(chIn.handle,
                        {0.0f, 0.0f, speed_z});
        StepN(240);
        const glm::vec3 vIn = chIn.pNode->GetTransform().GetWorldPos();
        EXPECT_GT(vIn.z, -12.5f) << "could not walk through the open doorway";
}

// The round-10 contract: walking into the bell trigger writes the authored
// bindings onto the GLOBAL blackboard. This is the whole chain — physics
// overlap → TriggerVolume → ETriggerFired → GetNode(trigger body) →
// TriggerBinding → global writes (bool value + the entering actor's position
// through its presence body). It silently no-op'd for every trigger in the
// game until the trigger bodies gained their node registration.
TEST_F(ContentPhysTestBase, VillageContent_BellBindingWritesGlobalBlackboard) {
        auto pVillage = LoadVillage(*scene);
        ASSERT_TRUE(pVillage);

        // the fixture instantiates the sub-level directly (no world
        // manifest), so init the global board from the asset the demo declares
        auto hBB = SE::CreateResource<SE::BlackboardAsset>(
                        "resource/ai/world_demo/bb_world.sebb");
        ASSERT_TRUE(hBB.IsValid());
        SE::GetSystem<SE::BlackboardSystem>().GetGlobal().Init(
                        SE::GetResource(hBB));

        // walk INTO the bell arch (trigger box at (0, 2, 10), half 1.2×1.5×1.2)
        // — like the player does. The demo's streaming activation runs the
        // Enable lifecycle (which arms TriggerVolume::enabled) — the raw
        // Instantiate does not. StepFrame (not StepN): the queued
        // ETriggerFired only dispatches in the loop's Events().Process() phase.
        pVillage->EnableRecursive();
        auto ch = SpawnChar(*scene, {0.0f, 2.05f, 7.0f});
        SE::GetSystem<SE::PhysicsSystem>().SetCharacterVelocity(ch.handle,
                        {0.0f, 0.0f, 1.5f});
        for (int i = 0; i < 150; ++i) { StepFrame(1.f / 60.f); }

        auto & oGlobal = SE::GetSystem<SE::BlackboardSystem>().GetGlobal();
        EXPECT_TRUE(oGlobal.GetBool(SE::StrID("alert")))
                        << "bell binding did not set alert on the global board";
        // noise_pos = the entering actor's position when the trigger fired
        const glm::vec3 vNoise = oGlobal.GetVec3(SE::StrID("noise_pos"));
        EXPECT_NEAR(vNoise.x, 0.0f, 0.5f) << "noise_pos is not the entering actor";
        EXPECT_NEAR(vNoise.z, 10.0f, 2.0f);
}

// NPCs are solid: the shipped guard template root must carry the kinematic
// capsule child (raised 1 m — node origin is at the feet). Checked on the
// baked flatbuffer: instantiating would drag in the animation/renderer stack.
TEST_F(ContentPhysTestBase, NpcTemplateCarriesKinematicCapsule) {
        std::ifstream in("resource/world/village_gen/wd_guard.sesc", std::ios::binary);
        ASSERT_TRUE(in) << "guard template scene missing";
        std::vector<char> bytes((std::istreambuf_iterator<char>(in)),
                                std::istreambuf_iterator<char>());

        auto * pTree = flatbuffers::GetRoot<SE::FlatBuffers::SceneTree>(bytes.data());
        ASSERT_NE(pTree->root(), nullptr);

        const SE::FlatBuffers::Node * pPhysics = nullptr;
        std::function<void(const SE::FlatBuffers::Node *)> walk =
                [&](const SE::FlatBuffers::Node * pNode) {
                        if (pPhysics || !pNode) { return; }
                        if (pNode->name() && pNode->name()->str() == "Physics") { pPhysics = pNode; return; }
                        if (const auto * pChildren = pNode->children()) {
                                for (const auto * pChild : *pChildren) { walk(pChild); }
                        }
                };
        walk(pTree->root());
        ASSERT_NE(pPhysics, nullptr) << "NPC template lost its kinematic capsule child";

        const SE::FlatBuffers::RigidBody * pRB = nullptr;
        if (const auto * pComps = pPhysics->components()) {
                for (const auto * pComp : *pComps) {
                        if (pComp->component_type() == SE::FlatBuffers::ComponentU::RigidBody) {
                                pRB = static_cast<const SE::FlatBuffers::RigidBody *>(pComp->component());
                        }
                }
        }
        ASSERT_NE(pRB, nullptr) << "Physics child carries no RigidBody";
        EXPECT_TRUE(pRB->is_kinematic());

        // capsule r=0.35 / hh=0.65 raised to y=1.0 → solid 2 m feet-to-head
        ASSERT_EQ(pRB->collider_type(), SE::FlatBuffers::ColliderU::CapsuleCollider);
        auto * pCapsule = static_cast<const SE::FlatBuffers::CapsuleCollider *>(pRB->collider());
        EXPECT_FLOAT_EQ(pCapsule->radius(), 0.35f);
        EXPECT_FLOAT_EQ(pCapsule->half_height(), 0.65f);
        ASSERT_NE(pPhysics->translation(), nullptr);
        EXPECT_FLOAT_EQ(pPhysics->translation()->y(), 1.0f);
}

// The inn doorway must stay walkable: the round-8 review found the interior
// crate pile sealing the opening ~1 m inside the door. No interior collider's
// world AABB may intrude into the doorway corridor.
TEST_F(ContentPhysTestBase, InnContent_DoorwayCorridorClear) {
        std::ifstream in("resource/world/village_gen/wd_inn_interior.sesc", std::ios::binary);
        ASSERT_TRUE(in) << "inn interior scene missing";
        std::vector<char> bytes((std::istreambuf_iterator<char>(in)),
                                std::istreambuf_iterator<char>());
        auto * pTree = flatbuffers::GetRoot<SE::FlatBuffers::SceneTree>(bytes.data());
        ASSERT_NE(pTree->root(), nullptr);

        // Doorway opening spans x [8.352, 9.648] in the front wall at z = -14
        // (jamb faces at 8.36 / 9.64); the corridor is that footprint from the
        // wall out to 1.5 m inside the room.
        constexpr float kMinX =  8.352f, kMaxX =  9.648f;
        constexpr float kMinZ = -14.205f, kMaxZ = -12.5f;

        std::vector<std::string> blockers;
        std::function<void(const SE::FlatBuffers::Node *)> walk =
                [&](const SE::FlatBuffers::Node * pNode) {
                        if (!pNode) { return; }
                        if (const auto * pComps = pNode->components()) {
                                for (const auto * pComp : *pComps) {
                                        if (pComp->component_type() != SE::FlatBuffers::ComponentU::RigidBody) { continue; }
                                        auto * pRB = static_cast<const SE::FlatBuffers::RigidBody *>(pComp->component());
                                        if (pRB->collider_type() != SE::FlatBuffers::ColliderU::BoxCollider) { continue; }
                                        auto * pBox = static_cast<const SE::FlatBuffers::BoxCollider *>(pRB->collider());
                                        const auto * pT = pNode->translation();
                                        const auto * pR = pNode->rotation();
                                        const float yaw = pR ? pR->y() * 3.14159265f / 180.f : 0.f;
                                        const float c = std::cos(yaw), s = std::sin(yaw);
                                        const auto * pH = pBox->half_extents();
                                        ASSERT_NE(pH, nullptr);
                                        // yaw-rotated AABB half extents in the x/z plane
                                        const float hx = std::fabs(pH->x() * c) + std::fabs(pH->z() * s);
                                        const float hz = std::fabs(pH->x() * s) + std::fabs(pH->z() * c);
                                        const float px = pT ? pT->x() : 0.f;
                                        const float pz = pT ? pT->z() : 0.f;
                                        if (px + hx > kMinX && px - hx < kMaxX
                                            && pz + hz > kMinZ && pz - hz < kMaxZ) {
                                                blockers.push_back(pNode->name() ? pNode->name()->str() : "?");
                                        }
                                }
                        }
                        if (const auto * pChildren = pNode->children()) {
                                for (const auto * pChild : *pChildren) { walk(pChild); }
                        }
                };
        walk(pTree->root());

        for (const auto & b : blockers) {
                ADD_FAILURE() << "collider blocks the inn doorway corridor: " << b;
        }
}

// The collider is a real body in the Jolt world: a ray from outside the house
// hits the front wall.
TEST_F(ContentPhysTestBase, VillageContent_WallRaycastHit) {
        auto pVillage = LoadVillage(*scene);
        ASSERT_TRUE(pVillage);

        // front wall spans z ≈ [-5.2 .. -4.8]; shoot -z at its outer face
        SE::PhysicsRay ray;
        ray.vOrigin    = {kHouseX, 3.0f, -4.0f};
        ray.vDirection = {0.0f, 0.0f, -1.0f};
        SE::RaycastHit hit;
        EXPECT_TRUE(SE::GetSystem<SE::PhysicsSystem>().Raycast(ray, hit));
        EXPECT_NEAR(hit.vPoint.z, -4.795f, 0.05f);
}

// The user-visible contract: walking into the house wall stops the character.
TEST_F(ContentPhysTestBase, VillageContent_CharacterBlockedByHouseWall) {
        auto pVillage = LoadVillage(*scene);
        ASSERT_TRUE(pVillage);

        // Spawn inside the house over the interior floor tiles (they carry
        // colliders now), then walk toward the front wall.
        auto ch = SpawnChar(*scene, {kHouseX, 3.5f, -7.5f});
        SE::GetSystem<SE::PhysicsSystem>().SetCharacterVelocity(ch.handle,
                        {0.0f, -0.5f, 1.5f});
        StepN(240);

        const glm::vec3 vPos = ch.pNode->GetTransform().GetWorldPos();
        // The inner wall face sits at z ≈ -5.205; the capsule (r = 0.3) rests
        // at ≈ -5.6. Blocked well short of the wall plane, well past the spawn.
        EXPECT_LT(vPos.z, -5.25f);
        EXPECT_GT(vPos.z, -6.3f);
        EXPECT_TRUE(SE::GetSystem<SE::PhysicsSystem>().IsCharacterGrounded(ch.handle));
        EXPECT_NEAR(vPos.y, 2.0f, 0.3f);   // character position = feet, floor top ≈ 2.01
}

