
#ifdef SE_IMPL

#include <GlobalTypes.h>
#include <Animator.h>
#include <AnimatedModel.h>
#include <AnimGraph.h>
#include <Skeleton.h>
#include <AnimEvaluator.h>
#include <AnimGraphRuntime.h>
#include <Logging.h>
#include <Component_generated.h>

namespace SE {

Animator::Animator(
        TSceneTree::TSceneNodeExact* pNewNode,
        H<AnimGraph>                 hNewGraph)
        : pNode(pNewNode)
        , hGraph(hNewGraph)
{
        auto* pGraph = GetResource(hGraph);
        if (!pGraph) {
                log_e("Animator: null AnimGraph on node '{}'", pNode->GetName());
        } else {
                oGraphInstance.Init(*pGraph);
                log_d("Animator: graph initialized on node '{}'", pNode->GetName());
        }

        // Derive skeleton from the AnimatedModel on the same node
        if (auto* pAnimModel = pNode->GetComponent<AnimatedModel>()) {
                hSkeleton = pAnimModel->GetSkeletonHandle();
        } else {
                log_w("Animator: no AnimatedModel on node '{}' — skeleton unavailable",
                      pNode->GetName());
        }

        Enable();
}

Animator::~Animator() noexcept {
        Disable();
}

Animator::Animator(TSceneTree::TSceneNodeExact* pNewNode,
                   const SE::FlatBuffers::Animator* pFB)
        : pNode(pNewNode)
{
        if (!pFB || !pFB->animation_graph()) {
                log_e("Animator: null FlatBuffer or missing animation_graph on node '{}'",
                      pNode->GetName());
                return;
        }

        // Create an AnimGraph resource from the inline FlatBuffer data.
        // The FlatBuffer lives in the SceneTree buffer which stays alive while
        // the scene is loaded — no copy needed.
        std::string sGraphName = "@anim_graph/" + pNode->GetFullName();
        hGraph = CreateResource<AnimGraph>(sGraphName, pFB->animation_graph());

        if (auto* pGraph = GetResource(hGraph)) {
                oGraphInstance.Init(*pGraph);
                log_d("Animator: graph '{}' initialized on node '{}'",
                      sGraphName, pNode->GetName());
        } else {
                log_e("Animator: failed to create AnimGraph resource on node '{}'",
                      pNode->GetName());
        }

        // Skeleton derivation is deferred to PostLoad because AnimatedModel::PostLoad
        // (which binds the skeleton) runs before Animator::PostLoad in node component order.
}

SE::ret_code_t Animator::PostLoad(const SE::FlatBuffers::Animator* /*pFB*/) {

        if (auto* pAnimModel = pNode->GetComponent<AnimatedModel>()) {
                hSkeleton = pAnimModel->GetSkeletonHandle();
                if (!hSkeleton.IsValid()) {
                        log_w("Animator::PostLoad: AnimatedModel on node '{}' has no skeleton yet",
                              pNode->GetName());
                }
        } else {
                log_w("Animator::PostLoad: no AnimatedModel on node '{}' — skeleton unavailable",
                      pNode->GetName());
        }

        Enable();
        return uSUCCESS;
}

void Animator::Enable() {
        GetSystem<EventManager>().AddListener<EUpdate, &Animator::OnUpdate>(this);
}

void Animator::Disable() {
        GetSystem<EventManager>().RemoveListener<EUpdate, &Animator::OnUpdate>(this);
}

void Animator::OnUpdate(const Event& oEvent) {
        Evaluate(oEvent.Get<EUpdate>().last_frame_time);
}

void Animator::Evaluate(float dt) {

        const Skeleton* pSkel = GetResource(hSkeleton);
        if (!pSkel) {
                log_w("Animator: node '{}' has no Skeleton; "
                      "skipping blend-tree evaluation", pNode->GetName());
                return;
        }

        const uint32_t bone_count = pSkel->BoneCount();
        if (bone_count == 0) return;

        auto& alloc = GetSystem<FrameAllocator>();
        LocalPose pose = AllocatePose(bone_count, alloc);

        if (show_bind_pose) {
                // Show bind pose — skip graph evaluation entirely
                InitBindPose(pose, *pSkel);
        } else {
                // 1. Tick state machine
                oGraphInstance.Update(dt);

                // 2. Seed pose from bind pose so un-animated bones keep their rest position.
                //    SampleClip uses replace semantics — it overwrites only the channels
                //    present in the clip, leaving all other bone TRS at bind values.
                InitBindPose(pose, *pSkel);

                // 3. Evaluate the blend tree into the pose
                oGraphInstance.EvaluateBlendTree(1.0f, pose, alloc, *pSkel);

                // 4. Keep quaternions unit-length after sampling
                RenormalizeRotations(pose);
        }

        // 6. Push the resulting local transforms to the joint scene nodes
        ApplyPoseToJointNodes(pose);
}

void Animator::ApplyPoseToJointNodes(const LocalPose& pose) {

        auto* pAnimModel = pNode->GetComponent<AnimatedModel>();
        if (!pAnimModel) return;

        const auto& vJointNodes = pAnimModel->JointNodes();
        const uint32_t limit = static_cast<uint32_t>(
                std::min<size_t>(pose.bone_count, vJointNodes.size()));

        for (uint32_t i = 0; i < limit; ++i) {
                if (auto pJointNode = vJointNodes[i].lock()) {
                        pJointNode->SetPos(pose.pPos[i]);
                        pJointNode->SetRotation(pose.pRot[i]);
                        pJointNode->SetScale(pose.pScl[i]);
                }
        }
}

std::string Animator::Str() const {
        return fmt::format("Animator[node='{}', graph={}]",
                           pNode->GetName(), hGraph);
}

} // namespace SE

#endif
