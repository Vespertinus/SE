
// Headless AnimatedModel double — impl for the shadow header.
// Compiled once per TU through CoreComponents' impl section (SE_IMPL).

#ifdef SE_IMPL

#include <AnimatedModel.h>
#include <Skeleton.h>
#include <Logging.h>

namespace SE {

AnimatedModel::AnimatedModel(TSceneTree::TSceneNodeExact * pNewNode)
        : pNode(pNewNode) {}

AnimatedModel::AnimatedModel(TSceneTree::TSceneNodeExact * pNewNode,
                             const SE::FlatBuffers::AnimatedModel * /*pModel*/)
        : pNode(pNewNode) {}

AnimatedModel::~AnimatedModel() noexcept = default;

ret_code_t AnimatedModel::PostLoad(const SE::FlatBuffers::AnimatedModel * /*pModel*/) {
        return uSUCCESS;
}

void AnimatedModel::BindSkeleton(H<Skeleton> hNewSkeleton) {

        hSkeleton = hNewSkeleton;
        vJointNodes.clear();

        const Skeleton* pSkel = GetResource(hSkeleton);
        if (!pSkel) {
                log_w("AnimatedModel(shadow): null skeleton on node '{}'", pNode->GetName());
                return;
        }

        vJointNodes.reserve(pSkel->BoneCount());
        for (const Skeleton::BoneData& oBone : pSkel->Bones()) {
                vJointNodes.push_back(pNode->FindChild(StrID(oBone.name), /*recursive=*/true));
        }
        log_d("AnimatedModel(shadow): bound {} joint nodes on node '{}'",
              vJointNodes.size(), pNode->GetName());
}

std::string AnimatedModel::Str() const {
        return fmt::format("AnimatedModel(shadow)[joints={}]", vJointNodes.size());
}

} // namespace SE

#endif // SE_IMPL
