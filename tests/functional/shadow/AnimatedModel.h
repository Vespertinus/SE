
#ifndef __ANIMATED_MODEL_H__
#define __ANIMATED_MODEL_H__

// ---------------------------------------------------------------------------
// HEADLESS TEST DOUBLE — shadows units/AnimatedModel.h for exactly one TU.
//
// The production AnimatedModel pulls Material/UniformBlock/mesh GPU state, so
// a real instance cannot exist in the no-GL functional tier. This double keeps
// the class identity (same name/guard, derives from the real StaticModel) but
// replaces skinning/GPU machinery with nothing, exposing only what Animator
// and the tests need:
//   GetSkeletonHandle() / JointNodes() / BindSkeleton(H<Skeleton>)
//
// BindSkeleton mirrors the production bind: one joint scene node per skeleton
// bone, resolved by name via FindChild (recursive).
//
// RULES (see agent_workspace/artifacts/func_tests_infra_001.md §7):
//   * shadow/ is placed FIRST on the include path of ONE dedicated target
//     (anim_animator_test) — never add it to global include dirs;
//   * one TU per binary, as everywhere in tests/functional;
//   * the shadow .tcc is compiled via CoreComponents' impl section
//     (GlobalTypes.h → AllImpl.tcc) — do not include it manually.
// ---------------------------------------------------------------------------

#include <Component_generated.h>
#include <ResourceHandle.h>
#include <StrID.h>

namespace SE {

class Skeleton;

// Deliberately NOT deriving from StaticModel: the real base ctor loads GL
// shader materials (wireframe.semt), which cannot exist in the headless tier.
// Same precedent as tests/StaticModelMock.h.
class AnimatedModel {

        public:
        using TSerialized = FlatBuffers::AnimatedModel;

        AnimatedModel(TSceneTree::TSceneNodeExact * pNewNode);
        AnimatedModel(TSceneTree::TSceneNodeExact * pNewNode,
                      const SE::FlatBuffers::AnimatedModel * pModel);
        ~AnimatedModel() noexcept;

        ret_code_t PostLoad(const SE::FlatBuffers::AnimatedModel * pModel);

        // Component lifecycle hooks required by SceneNode::CreateComponent.
        void Enable() {}
        void Disable() {}

        // Production-like bind: resolve joint nodes by bone name.
        void BindSkeleton(H<Skeleton> hSkel);

        std::string Str() const;

        const std::vector<TSceneTree::TSceneNode>& JointNodes() const { return vJointNodes; }
        H<Skeleton> GetSkeletonHandle() const { return hSkeleton; }

        private:
        TSceneTree::TSceneNodeExact *       pNode = nullptr;
        std::vector<TSceneTree::TSceneNode> vJointNodes;
        H<Skeleton>                         hSkeleton;
};

}

#endif
