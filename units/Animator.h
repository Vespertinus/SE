
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

class Animator {

        TSceneTree::TSceneNodeExact*  pNode;
        H<AnimGraph>                  hGraph;
        H<Skeleton>                   hSkeleton;
        AnimGraphInstance             oGraphInstance;
        bool                          show_bind_pose = false;

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

        std::string Str()       const;
        void        DrawDebug() const {}
};

} // namespace SE

#endif
