
namespace SE {

Skeleton::Skeleton(
                const std::string & sName,
                const rid_t new_rid,
                const SE::FlatBuffers::Skeleton * pSkeleton) :
        ResourceHolder(new_rid, sName) {

        uint32_t joints_cnt = pSkeleton->joints()->Length();
        se_assert(joints_cnt <= 255);
        vJoints.reserve(joints_cnt);

        for (uint32_t i = 0; i < joints_cnt; ++i) {
                auto pCurJoint = pSkeleton->joints()->Get(i);
                vJoints.emplace_back(Joint{
                                pCurJoint->name()->c_str(),
                                pCurJoint->parent_index()
                                }
                                );
        }

        log_d("name: '{}', joints cnt: {}", sName, vJoints.size());
}

Skeleton::Skeleton(const std::string & sName, const rid_t new_rid) : ResourceHolder(new_rid, sName) {

        //TODO load from file
        log_e("unimplemented yet");
        abort();
}

uint8_t Skeleton::GetJointsCnt() const {
        return vJoints.size();
}

const std::vector<Joint> & Skeleton::Joints() const {
        return vJoints;
}


CharacterShell::CharacterShell(
                const std::string & sName,
                const rid_t new_rid,
                TSceneTree::TSceneNodeExact * pTargetNode) : ResourceHolder(new_rid, sName) {

        //TODO load from file
        log_e("unimplemented yet");
        abort();
}

CharacterShell::CharacterShell(
                const std::string & sName,
                const rid_t new_rid,
                const SE::FlatBuffers::CharacterShell * pShell,
                TSceneTree::TSceneNodeExact * pTargetNode) :
        ResourceHolder(new_rid, sName) {

        //if (!pSkeletonHolder) { return uSUCCESS; }

        if (pShell->skeleton()->path() != nullptr) {
                pSkeleton = CreateResource<Skeleton>(GetSystem<Config>().sResourceDir + pShell->skeleton()->path()->c_str());
        }
        else if (pShell->skeleton()->name() != nullptr && pShell->skeleton()->skeleton() != nullptr) {
                pSkeleton = CreateResource<Skeleton>(
                                pShell->skeleton()->name()->c_str(),
                                pShell->skeleton()->skeleton());
        }
        else {
                throw(std::runtime_error(fmt::format(
                                                "wrong skeleton holder state, skeleton {:p}, name {:p}",
                                                (void *)pShell->skeleton()->skeleton(),
                                                (void *)pShell->skeleton()->name()
                                                )));

        }

        auto * pScene = pTargetNode->GetScene();
        se_assert(pScene);

        auto * vNodes = pScene->FindLocalName(pShell->skeleton_root_node()->c_str());

        if (!vNodes || vNodes->size() != 1) {

                throw(std::runtime_error(fmt::format(
                                                "failed to find skeleton root node ('{}'), skeleton name: '{}', target node: '{}'",
                                                pShell->skeleton_root_node()->c_str(),
                                                pSkeleton->Name(),
                                                pTargetNode->GetName()
                                                )));
        }

        auto & pRootNode = (*vNodes)[0];

        auto & vJoints = pSkeleton->Joints();
        vJointNodes.reserve(vJointNodes.size());
        for (auto & oJoint : vJoints) {
                auto pJointNode = pRootNode->FindChild(oJoint.sName, true);
                if (!pJointNode) {

                throw(std::runtime_error(fmt::format(
                                                "failed to find skeleton node ('{}') inside root '{}', skeleton name: '{}', target node: '{}'",
                                                oJoint.sName,
                                                pShell->skeleton_root_node()->c_str(),
                                                pSkeleton->Name(),
                                                pTargetNode->GetName()
                                                )));
                }
                vJointNodes.emplace_back(pJointNode);
        }

}


Skeleton * CharacterShell::GetSkeleton() const {

        return pSkeleton;
}

const std::vector<TSceneTree::TSceneNodeWeak> & CharacterShell::JointNodes() {
        return vJointNodes;
}


}
