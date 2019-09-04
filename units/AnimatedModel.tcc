
namespace SE {

const StrID AnimatedModel::BS_WEIGHT            = "BlendShapesWeights";
const StrID AnimatedModel::BS_WEIGHTS_CNT       = "BlendShapesCnt";
const StrID AnimatedModel::JOINTS_CNT           = "JointsCnt";
const StrID AnimatedModel::JOINTS_PER_VERTEX    = "JointsPerVertex";
const StrID AnimatedModel::JOINTS_MATRICES      = "JointsMatrices";

ret_code_t SkeletonData::FillData(
                const SE::FlatBuffers::SkeletonHolder * pSkeletonHolder,
                std::string_view sSkeletonRootNode,
                TSceneTree::TSceneNodeExact * pTargetNode) {

        if (!pSkeletonHolder) { return uSUCCESS; }

        if (pSkeletonHolder->path() != nullptr) {
                pSkeleton = CreateResource<Skeleton>(GetSystem<Config>().sResourceDir + pSkeletonHolder->path()->c_str());
        }
        else if (pSkeletonHolder->name() != nullptr && pSkeletonHolder->skeleton() != nullptr) {
                pSkeleton = CreateResource<Skeleton>(
                                pSkeletonHolder->name()->c_str(),
                                pSkeletonHolder->skeleton());
        }
        else {
                log_e("wrong skeleton holder state, skeleton {:p}, name {:p}",
                                                (void *)pSkeletonHolder->skeleton(),
                                                (void *)pSkeletonHolder->name()
                                                );
                return uWRONG_INPUT_DATA;
        }

        auto * pScene = pTargetNode->GetScene();
        se_assert(pScene);

        auto * vNodes = pScene->FindLocalName(sSkeletonRootNode);

        if (!vNodes || vNodes->size() != 1) {

                log_e("failed to find skeleton root node ('{}'), skeleton name: '{}', target node: '{}'",
                                                sSkeletonRootNode,
                                                pSkeleton->Name(),
                                                pTargetNode->GetName() );
                return uWRONG_INPUT_DATA;
        }

        auto & pRootNode = (*vNodes)[0];

        auto & vJoints = pSkeleton->Joints();
        vJointNodes.reserve(vJointNodes.size());
        for (auto & oJoint : vJoints) {
                auto pJointNode = pRootNode->FindChild(oJoint.sName, true);
                if (!pJointNode) {

                        log_e("failed to find skeleton node ('{}') inside root '{}', skeleton name: '{}', target node: '{}'",
                                                        oJoint.sName,
                                                        sSkeletonRootNode,
                                                        pSkeleton->Name(),
                                                        pTargetNode->GetName()
                                                        );
                        return uWRONG_INPUT_DATA;
                }

                vJointNodes.emplace_back(pJointNode);
                //TODO serialize mJointBind on asset import
                glm::mat4 mJointBind;
                glm::mat4 mTranslate    = glm::translate(glm::mat4(1.0), oJoint.bind_pos);
                glm::mat4 mScale        = glm::scale (glm::mat4(1.0), oJoint.bind_scale);
                glm::mat4 mRotation     = glm::toMat4(oJoint.bind_qrot);
                mJointBind  = mTranslate * mRotation * mScale;

                vJointBaseMat.emplace_back(glm::inverse(mJointBind) * pTargetNode->GetTransform().GetWorld());
        }

        return uSUCCESS;
}

AnimatedModel::AnimatedModel(TSceneTree::TSceneNodeExact * pNewNode,
                             TMesh * pNewMesh,
                             Material * pNewMaterial,
                             TTexture * pNewTexBuf,
                             const uint8_t new_blendshapes_cnt) :
        StaticModel(pNewNode, pNewMesh, pNewMaterial),
        pTexBuffer(pNewTexBuf),
        blendshapes_cnt(new_blendshapes_cnt) {

        if (!pMaterial->GetShader()->OwnTextureUnit(TextureUnit::BUFFER)) {

                throw(std::runtime_error(fmt::format(
                                                "wrong material: '{}', shader: '{}', does not own TextureUnit::BUFFER, node: '{}'",
                                                pMaterial->Name(),
                                                pMaterial->GetShader()->Name(),
                                                pNode->GetFullName()
                                                )));
        }

        pBlock = std::make_unique<UniformBlock>(pMaterial->GetShader(), UniformUnitInfo::Type::ANIMATION);
        auto res = pBlock->SetVariable(BS_WEIGHTS_CNT, blendshapes_cnt);
        if (res != uSUCCESS) {
                throw(std::runtime_error(fmt::format("failed to set blendshape cnt variable: '{}', shader: '{}', node: '{}'",
                                                BS_WEIGHTS_CNT,
                                                pMaterial->GetShader()->Name(),
                                                pNode->GetFullName())));
        }
}


AnimatedModel::AnimatedModel(
                TSceneTree::TSceneNodeExact * pNewNode,
                const SE::FlatBuffers::AnimatedModel * pModel) {

        const auto & oConfig = GetSystem<Config>();

        pNode = pNewNode;

        if (pModel->mesh()->path() != nullptr) {
                pMesh = CreateResource<TMesh>(pModel->mesh()->path()->c_str());
        }
        else if (pModel->mesh()->name() != nullptr && pModel->mesh()->mesh() != nullptr) {

                pMesh = CreateResource<TMesh>(pModel->mesh()->name()->c_str(), pModel->mesh()->mesh());
        }
        else {
                throw(std::runtime_error(fmt::format("wrong mesh state, mesh {:p}, name {:p}",
                                                (void *)pModel->mesh()->mesh(),
                                                (void *)pModel->mesh()->name()
                                                )));
        }

        if (pModel->material()) {
                if (pModel->material()->path() != nullptr) {
                        pMaterial = CreateResource<Material>(oConfig.sResourceDir + pModel->material()->path()->c_str());
                }
                else if (pModel->material()->name() != nullptr && pModel->material()->material() != nullptr) {
                        pMaterial = CreateResource<Material>(
                                        pModel->material()->name()->c_str(),
                                        pModel->material()->material());
                }
                else {
                        throw(std::runtime_error(fmt::format("wrong material state, material {:p}, name {:p}",
                                                        (void *)pModel->material()->material(),
                                                        (void *)pModel->material()->name()
                                                        )));
                }
        }
        else {
                pMaterial = CreateResource<SE::Material>(oConfig.sResourceDir + "material/default_morph.semt");
        }

        pBlock = std::make_unique<UniformBlock>(pMaterial->GetShader(), UniformUnitInfo::Type::ANIMATION);

        if (pModel->blendshapes_weights()) {

                if (!pMaterial->GetShader()->OwnTextureUnit(TextureUnit::BUFFER)) {

                        throw(std::runtime_error(fmt::format(
                                                        "wrong material: '{}', does not own TextureUnit::BUFFER, node: '{}'",
                                                        pMaterial->Name(),
                                                        pNode->GetFullName()
                                                        )));
                }


                blendshapes_cnt = pModel->blendshapes_weights()->Length();

                pTexBuffer = LoadTexture(pModel->blendshapes());

                if (!pTexBuffer || static_cast<TextureUnit>(pModel->blendshapes()->unit()) != TextureUnit::BUFFER) {

                        throw(std::runtime_error(fmt::format("failed to load texture buffer from texture holder, tex: {:p}, texture_unit: {}, node: '{}'",
                                                        (void *)pTexBuffer,
                                                        EnumNameTextureUnit(pModel->blendshapes()->unit()),
                                                        pNode->GetFullName())));
                }

                ret_code_t res;
                for (size_t i = 0; i < blendshapes_cnt; ++i) {

                        res = pBlock->SetArrayElement(BS_WEIGHT, i, pModel->blendshapes_weights()->Get(i));
                        if (res != uSUCCESS) {
                                throw(std::runtime_error(fmt::format("failed to set blendshape weight variable: '{}', shader: '{}', node: '{}'",
                                                                BS_WEIGHT,
                                                                pMaterial->GetShader()->Name(),
                                                                pNode->GetFullName())));
                        }
                }

                res = pBlock->SetVariable(BS_WEIGHTS_CNT, blendshapes_cnt);
                if (res != uSUCCESS) {
                        throw(std::runtime_error(fmt::format("failed to set blendshape cnt variable: '{}', shader: '{}', node: '{}'",
                                                        BS_WEIGHTS_CNT,
                                                        pMaterial->GetShader()->Name(),
                                                        pNode->GetFullName())));
                }

        }

}

AnimatedModel::~AnimatedModel() noexcept {

        Disable();

        //THINK move to enable \ disable ???
        if (oSkeleton.vJointNodes.size()) {

                for (auto & pWeakJointNode : oSkeleton.vJointNodes) {
                        if (auto pJointNode = pWeakJointNode.lock()) {
                                pJointNode->RemoveListener(this);
                        }
                }
        }

}

ret_code_t AnimatedModel::PostLoad(const SE::FlatBuffers::AnimatedModel * pModel) {

        auto res = oSkeleton.FillData(
                        pModel->skeleton(),
                        pModel->skeleton_root_node() ? pModel->skeleton_root_node()->c_str() : "",
                        pNode);

        if (res != uSUCCESS) { return res; }

        static StrID IndAttrID("JointIndices");

        if (oSkeleton.vJointNodes.size()) {


                for (auto & oAttrInfo : pMesh->GetAttrInfo()) {
                        if (oAttrInfo.first == IndAttrID) {
                                joints_per_vertex = static_cast<uint8_t>(oAttrInfo.second);
                                break;
                        }
                }

                if (!joints_per_vertex || joints_per_vertex > 4) {
                        log_e("wrong joints numver per vertex: {}, max 4 allowed", joints_per_vertex);
                        return uWRONG_INPUT_DATA;
                }
/*
                res = pBlock->SetVariable(JOINTS_CNT, static_cast<uint32_t>(oSkeleton.vJointNodes.size()));
                if (res != uSUCCESS) {
                        log_e("failed to set joints cnt: '{}', shader: '{}', node: '{}'",
                                        oSkeleton.vJointNodes.size(),
                                        pMaterial->GetShader()->Name(),
                                        pNode->GetFullName());
                        return res;
                }
*/
                res = pBlock->SetVariable(JOINTS_PER_VERTEX, static_cast<uint32_t>(joints_per_vertex));
                if (res != uSUCCESS) {
                        log_e("failed to set joints per vertex var: '{}', shader: '{}', node: '{}'",
                                        joints_per_vertex,
                                        pMaterial->GetShader()->Name(),
                                        pNode->GetFullName());
                        return res;
                }

                for (auto & pWeakJointNode : oSkeleton.vJointNodes) {
                        if (auto pJointNode = pWeakJointNode.lock()) {
                                pJointNode->AddListener(this);
                        }
                        else {
                               log_e("failed to get joint node, current node: '{}'", pNode->GetFullName());
                               return uLOGIC_ERROR;
                        }
                }

                skinning_dirty = true;
        }

        FillRenderCommands();

        return res;
}

void AnimatedModel::TargetTransformChanged(TSceneTree::TSceneNodeExact * pTargetNode [[maybe_unused]]) {

        skinning_dirty = true;
}

ret_code_t AnimatedModel::SetMaterial(Material * pNewMaterial) {

        if (!pNewMaterial->GetShader()->OwnTextureUnit(TextureUnit::BUFFER)) {

        log_e("wrong material: '{}', does not own TextureUnit::BUFFER, node: '{}'",
                        pNewMaterial->Name(),
                        pNode->GetFullName());
                return uWRONG_INPUT_DATA;
        }

        try {
                ret_code_t res;
                auto pNewBlock = std::make_unique<UniformBlock>(pNewMaterial->GetShader(), UniformUnitInfo::Type::ANIMATION);

                if (res = pNewBlock->SetVariable(BS_WEIGHTS_CNT, blendshapes_cnt); res != uSUCCESS) {
                        log_e("failed to set blendshape cnt variable: '{}', shader: '{}', node: '{}'",
                                        BS_WEIGHTS_CNT,
                                        pNewMaterial->GetShader()->Name(),
                                        pNode->GetFullName());
                        return res;
                }

                for (size_t i = 0; i < blendshapes_cnt; ++i) {

                        const float * pValue = nullptr;
                        if (res = pBlock->GetArrayElement(BS_WEIGHT, i, pValue); res != uSUCCESS) {
                                return res;
                        }

                        res = pNewBlock->SetArrayElement(BS_WEIGHT, i, *pValue);
                        if (res != uSUCCESS) {
                                log_e("failed to set blendshape weight variable: '{}', shader: '{}', node: '{}'",
                                                                BS_WEIGHT,
                                                                pNewMaterial->GetShader()->Name(),
                                                                pNode->GetFullName());
                        }
                }

                pBlock = std::move(pNewBlock);
        }
        catch(std::exception & ex) {
                log_e("failed to allocate Animation uniform block from new material: '{}', shader: '{}', reason: '{}'",
                                pNewMaterial->Name(),
                                pNewMaterial->GetShader()->Name(),
                                ex.what());
                return uWRONG_INPUT_DATA;
        }
        catch(...) {
                log_e("failed to allocate Animation uniform block from new material: '{}', shader: '{}'",
                                pNewMaterial->Name(),
                                pNewMaterial->GetShader()->Name() );
                return uWRONG_INPUT_DATA;
        }


        pMaterial = pNewMaterial;
        FillRenderCommands();
        return uSUCCESS;
}

void AnimatedModel::FillRenderCommands() {

        StaticModel::FillRenderCommands();
        for (auto & oItem : vRenderCommands) {
                oItem.State().SetBlock(UniformUnitInfo::Type::ANIMATION, pBlock.get());
                if (blendshapes_cnt) {
                        oItem.State().SetTexture(TextureUnit::BUFFER, pTexBuffer);
                }
        }
}

std::string AnimatedModel::Str() const {

        return fmt::format("AnimatedModel: Mesh: '{}', Material: '{}', bs cnt: {}",
                        pMesh->Name(),
                        pMaterial->Name(),
                        blendshapes_cnt);
}

ret_code_t AnimatedModel::SetWeight(const uint8_t index, const float weight) {

        if (index >= blendshapes_cnt) {
                log_e("wrong weight index: {}, max allowed: {}, node: '{}'",
                                index,
                                blendshapes_cnt,
                                pNode->GetFullName());
                return uWRONG_INPUT_DATA;
        }

//        log_d("index: {}, weight: {}", index, weight);
        return pBlock->SetArrayElement(BS_WEIGHT, index, weight);
}

uint8_t AnimatedModel::BlendShapesCnt() const {
        return blendshapes_cnt;
}

float AnimatedModel::GetWeight(const uint8_t index) {

        const float * pValue = nullptr;

        if (index >= blendshapes_cnt) {
                log_e("wrong weight index: {}, max allowed: {}, node: '{}'",
                                index,
                                blendshapes_cnt,
                                pNode->GetFullName());
                return 0;
        }

        if (auto res = pBlock->GetArrayElement(BS_WEIGHT, index, pValue); res != uSUCCESS) {
                return 0;
        }

        return *pValue;
}

void AnimatedModel::PostUpdate(const Event & oEvent [[maybe_unused]]) {

        if (!skinning_dirty) { return; }

        log_d("need to update skinning: node: '{}', skeleton: '{}'", pNode->GetName(), oSkeleton.pSkeleton->Name());

        for (size_t i = 0; i < oSkeleton.vJointNodes.size(); ++i) {

                if (auto pJointNode = oSkeleton.vJointNodes[i].lock()) {

                        pBlock->SetArrayElement(JOINTS_MATRICES, i, pJointNode->GetTransform().GetWorld() * oSkeleton.vJointBaseMat[i]);
                }
                else {
                        pBlock->SetArrayElement(JOINTS_MATRICES, i, pNode->GetTransform().GetWorld());
                }
        }

        skinning_dirty = false;
}

void AnimatedModel::Enable() {

        StaticModel::Enable();
        GetSystem<EventManager>().AddListener<EPostUpdate, &AnimatedModel::PostUpdate>(this);
}

void AnimatedModel::Disable() {

        StaticModel::Disable();
        GetSystem<EventManager>().RemoveListener<EPostUpdate, &AnimatedModel::PostUpdate>(this);
}

void AnimatedModel::DrawDebug() const {

        StaticModel::DrawDebug();
        if (!oSkeleton.vJointNodes.size()) { return; }

        for (uint32_t i = 0; i < oSkeleton.vJointNodes.size(); ++i) {

                if (oSkeleton.pSkeleton->Joints()[i].parent_ind == 255) {
                        continue;
                }

                auto j = oSkeleton.pSkeleton->Joints()[i].parent_ind;

                if (auto pStart = oSkeleton.vJointNodes[i].lock()) {
                        if (auto pEnd = oSkeleton.vJointNodes[j].lock()) {

                                GetSystem<DebugRenderer>().DrawLine(
                                                pStart->GetTransform().GetWorldPos(),
                                                pEnd->GetTransform().GetWorldPos(),
                                                glm::vec4(0.8, 0.2, 0.15, 1.0));
                        }
                }
        }
}


}

