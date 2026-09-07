
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
                throw(std::runtime_error(fmt::format("Animator: null AnimGraph on node '{}'", pNode->GetName() )));
        } else {
                oGraphInstance.Init(*pGraph);
                log_d("Animator: graph initialized on node '{}'", pNode->GetName());
        }

        // Derive skeleton from the AnimatedModel on the same node
        if (auto* pAnimModel = pNode->GetComponent<AnimatedModel>()) {
                hSkeleton = pAnimModel->GetSkeletonHandle();
        } else {
                throw(std::runtime_error(fmt::format("Animator: no AnimatedModel on node '{}' — skeleton unavailable", pNode->GetName() )));
        }

        // No Enable() here: SceneNode::CreateComponent enables the component on
        // enabled nodes — a second Enable() double-subscribes EUpdate.
}

Animator::~Animator() noexcept {
        Disable();
}

Animator::Animator(TSceneTree::TSceneNodeExact* pNewNode,
                   const SE::FlatBuffers::Animator* pFB)
        : pNode(pNewNode)
{
        const auto* pHolder = pFB ? pFB->animation_graph() : nullptr;
        if (!pHolder) {
                throw(std::runtime_error(fmt::format("Animator: null FlatBuffer or missing animation_graph on node '{}'", pNode->GetName() )));
        }

        const bool has_path = pHolder->path() && pHolder->path()->size() > 0;
        const bool has_name = pHolder->name() && pHolder->name()->size() > 0;

        if (has_path) {
                std::string sKey = has_name ? pHolder->name()->str() : pHolder->path()->str();
                hGraph = CreateResource<AnimGraph>(sKey);
        } else if (pHolder->graph()) {
                std::string sKey = has_name ? pHolder->name()->str()
                                            : "@anim_graph/" + pNode->GetFullName();
                hGraph = CreateResource<AnimGraph>(sKey, pHolder->graph());
        } else {
                throw(std::runtime_error(fmt::format("Animator: AnimationGraphHolder has neither path nor inline graph on node '{}'", pNode->GetName() )));
        }

        if (auto* pGraph = GetResource(hGraph)) {
                oGraphInstance.Init(*pGraph);
                log_d("Animator: graph initialized on node '{}'", pNode->GetName());
        } else {
                throw(std::runtime_error(fmt::format("Animator: failed to create AnimGraph resource on node '{}'", pNode->GetName() )));
        }

        // Skeleton derivation is deferred to PostLoad because AnimatedModel::PostLoad
        // (which binds the skeleton) runs before Animator::PostLoad in node component order.
}

SE::ret_code_t Animator::PostLoad(const SE::FlatBuffers::Animator* /*pFB*/) {

        if (auto* pAnimModel = pNode->GetComponent<AnimatedModel>()) {
                hSkeleton = pAnimModel->GetSkeletonHandle();
                if (!hSkeleton.IsValid()) {
                        log_w("Animator::PostLoad: AnimatedModel on node '{}' has no skeleton yet", pNode->GetName());
                        return uLOGIC_ERROR;
                }
        } else {
                log_w("Animator::PostLoad: no AnimatedModel on node '{}' — skeleton unavailable", pNode->GetName());
                return uLOGIC_ERROR;
        }

        // No Enable() here: the node already enabled this component during
        // CreateComponent — a second Enable() double-subscribes EUpdate.
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

        // EXTERNAL pose source (e.g. physics ragdoll): leave the joint nodes for the
        // owning system to write. The graph is not advanced while external.
        if (pose_source == PoseSource::EXTERNAL) return;

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
                last_root_delta = oGraphInstance.GetRootMotionDelta();

                // 2. Seed pose from bind pose so un-animated bones keep their rest position.
                //    SampleClip uses replace semantics — it overwrites only the channels
                //    present in the clip, leaving all other bone TRS at bind values.
                InitBindPose(pose, *pSkel);

                // 3. Evaluate the blend tree into the pose
                oGraphInstance.EvaluateBlendTree(1.0f, pose, alloc, *pSkel);

                // 4. Keep quaternions unit-length after sampling
                RenormalizeRotations(pose);

                // 5. Root motion: the locomotion layer receives the root-bone
                //    translation as velocity — keep it out of the pose so the
                //    mesh is not moved by it a second time.
                if (oGraphInstance.IsRootMotionEnabled()) {
                        const auto& vBones = pSkel->Bones();
                        if (!vBones.empty()) pose.pPos[0] = vBones[0].bindPos;
                }
        }

        // 6. Push the resulting local transforms to the joint scene nodes
        ApplyPoseToJointNodes(pose);
}

void Animator::ApplyPoseToJointNodes(const LocalPose& pose) {

        // Resolved per call (a compile-time-typed O(k) scan over a handful of
        // components) — a cached pointer here would go stale if the component is
        // destroyed and re-created on this living node.
        auto* pAnimModel = pNode->GetComponent<AnimatedModel>();
        if (!pAnimModel) return;

        // Joint nodes are owned shared refs (see AnimatedModel::vJointNodes):
        // .get() is a raw pointer load, no refcount traffic on this hot path.
        const auto& vJointNodes = pAnimModel->JointNodes();
        const uint32_t limit = static_cast<uint32_t>(
                std::min<size_t>(pose.bone_count, vJointNodes.size()));

        for (uint32_t i = 0; i < limit; ++i) {
                if (auto* pJointNode = vJointNodes[i].get()) {
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
