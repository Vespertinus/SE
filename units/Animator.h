
#ifndef __ANIMATOR_H__
#define __ANIMATOR_H__ 1

#include <string>
#include <AnimGraphRuntime.h>
#include <ResourceHandle.h>
#include <StrID.h>

namespace SE::FlatBuffers { struct Animator; }

namespace SE {

class AnimGraph;
class Skeleton;
class AnimatedModel;

class Animator {

public:
        // Where the per-frame joint pose comes from.
        //   GRAPH    — evaluate the AnimGraph (default).
        //   EXTERNAL — skip graph evaluation; another system (e.g. physics ragdoll)
        //              writes the joint nodes. Set via the character animation layer.
        enum class PoseSource : uint8_t { GRAPH, EXTERNAL };

private:
        TSceneTree::TSceneNodeExact*  pNode;
        H<AnimGraph>                  hGraph;
        H<Skeleton>                   hSkeleton;
        AnimGraphInstance             oGraphInstance;
        // Root delta extracted by the last graph Update() (valid when root motion enabled).
        RootMotionDelta               last_root_delta;
        bool                          show_bind_pose = false;
        PoseSource                    pose_source    = PoseSource::GRAPH;

        void ApplyPoseToJointNodes(const LocalPose& pose);
        void OnUpdate(const Event& oEvent);

public:
        using TSerialized = FlatBuffers::Animator;

        Animator(TSceneTree::TSceneNodeExact* pNode,
                 H<AnimGraph>                 hGraph);

        /** FlatBuffer-based constructor — invoked by SceneTree loader.
         *  Initialises the AnimGraph from inline FlatBuffer data.
         *  Skeleton derivation is deferred to PostLoad (AnimatedModel's PostLoad runs first). */
        Animator(TSceneTree::TSceneNodeExact* pNode,
                 const SE::FlatBuffers::Animator* pFB);

        ~Animator() noexcept;

        /** Called by SceneTree after all components on the node have run PostLoad.
         *  Derives the skeleton handle from the co-located AnimatedModel. */
        SE::ret_code_t PostLoad(const SE::FlatBuffers::Animator* pFB);

        void Enable();
        void Disable();

        void Evaluate(float dt);

        // Parameter setters — delegates to graph instance
        void SetFloat(StrID name, float v)   { oGraphInstance.SetFloat(name, v); }
        void SetBool(StrID name, bool v)     { oGraphInstance.SetBool(name, v); }
        void SetInt(StrID name, int v)       { oGraphInstance.SetInt(name, v); }
        void SetTrigger(StrID name)          { oGraphInstance.SetTrigger(name); }

        // Direct access to the graph instance for tooling (e.g. scene_viewer UI).
        AnimGraphInstance&       GetInstance()       { return oGraphInstance; }
        const AnimGraphInstance& GetInstance() const { return oGraphInstance; }

        // Bind pose mode — when true, Evaluate() applies the skeleton's bind pose
        // instead of evaluating the animation graph. Used for tooling/debugging.
        void SetShowBindPose(bool show) { show_bind_pose = show; }
        bool IsShowingBindPose() const  { return show_bind_pose; }

        // Pose-source seam — when EXTERNAL, Evaluate() skips graph evaluation and leaves
        // the joint nodes untouched so an external system (physics ragdoll) can drive them.
        void       SetPoseSource(PoseSource src) { pose_source = src; }
        PoseSource GetPoseSource() const         { return pose_source; }

        // Root motion — forwards to the graph instance; when enabled, Evaluate()
        // also strips the animated root-bone translation from the pose (the
        // locomotion layer receives it as velocity instead) and exposes the
        // delta extracted by the last graph Update().
        void SetUseRootMotion(bool enable) { oGraphInstance.SetUseRootMotion(enable); }
        bool IsRootMotionEnabled() const   { return oGraphInstance.IsRootMotionEnabled(); }
        const RootMotionDelta& GetRootMotionDelta() const { return last_root_delta; }

        std::string Str()       const;
        void        DrawDebug() const {}

        // Weak handle to the owning node — lifetime guard for systems that hold
        // raw Animator pointers (e.g. CharacterAnimationSystem links).
        TSceneTree::TSceneNodeWeak GetNodeWeak() const {
                return pNode ? pNode->weak_from_this() : TSceneTree::TSceneNodeWeak{};
        }
};

} // namespace SE

#endif
