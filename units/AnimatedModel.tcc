
namespace SE {

const StrID AnimatedModel::BS_WEIGHT            = "BlendShapesWeights";
const StrID AnimatedModel::BS_WEIGHTS_CNT       = "BlendShapesCnt";
const StrID AnimatedModel::JOINTS_CNT           = "JointsCnt";
const StrID AnimatedModel::JOINTS_PER_VERTEX    = "JointsPerVertex";
const StrID AnimatedModel::JOINTS_MATRICES      = "JointsMatrices";


/*ret_code_t AnimatedModel::SkeletonPart::FillData(
                const SE::FlatBuffers::CharacterShellHolder * pHolder,
                const flatbuffers::Vector<uint8_t> * pJointIndices,
                const flatbuffers::Vector<flatbuffers::Offset<SE::FlatBuffers::JointBind>>> * pJointBindMat,
                TSceneTree::TSceneNodeExact * pTargetNode) */

ret_code_t AnimatedModel::SkeletonPart::FillData(
                const SE::FlatBuffers::AnimatedModel * pModel,
                TSceneTree::TSceneNodeExact * pTargetNode) {

        const auto * pHolder            = pModel->shell();
        const auto * pJointIndices      = pModel->joint_indexes();
        const auto * pJointBindMat      = pModel->joint_bind_mat();

        if (!pHolder || !pJointIndices || ! pJointBindMat || (pJointIndices->Length() != pJointBindMat->Length())) { return uSUCCESS; }

        if (pHolder->path() != nullptr) {
                pShell = CreateResource<CharacterShell>(GetSystem<Config>().sResourceDir + pHolder->path()->c_str(), pTargetNode);
        }
        else if (pHolder->name() != nullptr && pHolder->shell() != nullptr) {
                /**TODO
                   create clone + reload
                 */

                //HACK -->
                std::string sShellName = fmt::format("{}|scn:{}",
                                pHolder->name()->c_str(),
                                StrID(pTargetNode->GetScene()->Name()) );

                pShell = CreateResource<CharacterShell>(
                                sShellName,
                                pHolder->shell(),
                                pTargetNode);
        }
        else {
                log_e("wrong character shell holder state, skeleton {:p}, name {:p}",
                                                (void *)pHolder->shell(),
                                                (void *)pHolder->name()
                                                );
                return uWRONG_INPUT_DATA;
        }

        //auto & vSkeletonJointsInfo = pShell->GetSkeleton()->Joints();
        vJointBaseMat.reserve(pJointIndices->Length());
        vJointIndexes.reserve(pJointIndices->Length());

        for (uint8_t joint_ind = 0; joint_ind < pJointIndices->Length(); ++joint_ind) {

                uint8_t cur_joint_ind = pJointIndices->Get(joint_ind);
                if (cur_joint_ind >= pShell->JointNodes().size()) {
                        log_e("wrong joint index: {}, max allowed: {}, shell: '{}', node: '{}'",
                                        cur_joint_ind,
                                        pShell->JointNodes().size(),
                                        pShell->Name(),
                                        pTargetNode->GetFullName());
                        return uWRONG_INPUT_DATA;
                }

                //auto & oJoint = vSkeletonJointsInfo[cur_joint_ind];
                auto * pJoint = pJointBindMat->Get(joint_ind);
                glm::quat bind_qrot     = *reinterpret_cast<const glm::quat *>(pJoint->bind_rot());
                glm::vec3 bind_pos      = *reinterpret_cast<const glm::vec3 *>(pJoint->bind_pos());
                glm::vec3 bind_scale    = *reinterpret_cast<const glm::vec3 *>(pJoint->bind_scale());

                //TODO serialize mJointBind on asset import
                glm::mat4 mJointBind;
                glm::mat4 mTranslate    = glm::translate(glm::mat4(1.0), bind_pos);
                glm::mat4 mScale        = glm::scale (glm::mat4(1.0), bind_scale);
                glm::mat4 mRotation     = glm::toMat4(bind_qrot);
                mJointBind  = mTranslate * mRotation * mScale;

                vJointBaseMat.emplace_back(mJointBind);
                vJointIndexes.emplace_back(cur_joint_ind);
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
        if (oSkeletonMeta.pShell) {
                auto & vJointNodes = oSkeletonMeta.pShell->JointNodes();

                for (auto cur_joint : oSkeletonMeta.vJointIndexes) {
                        auto & pWeakJointNode = vJointNodes[cur_joint];
                        if (auto pJointNode = pWeakJointNode.lock()) {
                                pJointNode->RemoveListener(this);
                        }
                }
        }

}

ret_code_t AnimatedModel::PostLoad(const SE::FlatBuffers::AnimatedModel * pModel) {

        auto res = oSkeletonMeta.FillData(
                        pModel,
                        /*pModel->shell(),
                        //pModel->skeleton_root_node() ? pModel->skeleton_root_node()->c_str() : "",
                        pModel->joint_indexes(),
                        pModel->joint_bind_mat(),*/
                        pNode);

        if (res != uSUCCESS) { return res; }

        static StrID IndAttrID("JointIndices");

        if (oSkeletonMeta.pShell && oSkeletonMeta.pShell->JointNodes().size()) {


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

                auto & vJointNodes = oSkeletonMeta.pShell->JointNodes();

                for (auto cur_joint : oSkeletonMeta.vJointIndexes) {
                        if (cur_joint >= vJointNodes.size()) {
                                log_e("wrong joint ind: {}, count: {}, current node: '{}'",
                                                cur_joint,
                                                vJointNodes.size(),
                                                pNode->GetFullName());
                                return uWRONG_INPUT_DATA;
                        }

                        auto & pWeakJointNode = vJointNodes[cur_joint];
                        if (auto pJointNode = pWeakJointNode.lock()) {
                                pJointNode->AddListener(this);
                                log_d("node '{}' listen to '{}'", pNode->GetName(), pJointNode->GetName());
                        }
                        else {
                               log_e("failed to get joint node, current node: '{}', joint ind: {}", pNode->GetFullName(), cur_joint);
                               return uLOGIC_ERROR;
                        }
                }
                pNode->AddListener(this);

                skinning_dirty = true;
        }

        FillRenderCommands();

        return res;
}

void AnimatedModel::TargetTransformChanged(TSceneTree::TSceneNodeExact * pTargetNode [[maybe_unused]]) {

        log_d("target node '{}', listener node: '{}'", pTargetNode->GetName(), pNode->GetName());

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

                if (oSkeletonMeta.vJointIndexes.size()) {
                        res = pNewBlock->SetVariable(JOINTS_PER_VERTEX, static_cast<uint32_t>(joints_per_vertex));
                        if (res != uSUCCESS) {
                                log_e("failed to set joints per vertex var: '{}', shader: '{}', node: '{}'",
                                                joints_per_vertex,
                                                pNewMaterial->GetShader()->Name(),
                                                pNode->GetFullName());
                                return res;
                        }
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
        if (oSkeletonMeta.vJointIndexes.size()) {
                skinning_dirty = true;
        }
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

        return fmt::format("AnimatedModel: Mesh: '{}', bs cnt: {}, joints: {}, Material: '{}'",
                        pMesh->Name(),
                        blendshapes_cnt,
                        oSkeletonMeta.vJointIndexes.size(),
                        pMaterial->Name()
                        );
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

        log_d("need to update skinning: node: '{}', shell: '{}', skeleton: '{}'",
                        pNode->GetName(),
                        oSkeletonMeta.pShell->Name(),
                        oSkeletonMeta.pShell->GetSkeleton()->Name());

        auto & vJointNodes = oSkeletonMeta.pShell->JointNodes();

        for (size_t i = 0; i < oSkeletonMeta.vJointIndexes.size(); ++i) {

                if (auto pJointNode = vJointNodes[oSkeletonMeta.vJointIndexes[i]].lock()) {

                        /*
                           log_d("joint[{}] world pos: ({}, {}, {})",
                                        i,
                                        pJointNode->GetTransform().GetWorldPos().x,
                                        pJointNode->GetTransform().GetWorldPos().y,
                                        pJointNode->GetTransform().GetWorldPos().z
                                        );
                         */

                        //pBlock->SetArrayElement(JOINTS_MATRICES, i, pJointNode->GetTransform().GetWorld() * oSkeletonMeta.vJointBaseMat[i]);

                        glm::mat4 mResTransform =
                                        glm::inverse(pNode->GetTransform().GetWorld())
                                        *
                                        pJointNode->GetTransform().GetWorld()
                                        *
                                        oSkeletonMeta.vJointBaseMat[i];
                        auto res_world_pos = glm::vec3(mResTransform[3]);
                        /*
                        glm::vec3 scale;
                        glm::quat rotation;
                        glm::vec3 translation;
                        glm::vec3 skew;
                        glm::vec4 perspective;
                        glm::decompose(mResTransform, scale, rotation, translation, skew,perspective);


                        log_d("node: '{}', joint: '{}', pos: ({}, {}, {}), scale: ({}, {}, {})",
                                        pNode->GetName(),
                                        pJointNode->GetName(),
                                        res_world_pos[0],
                                        res_world_pos[1],
                                        res_world_pos[2],
                                        scale[0],
                                        scale[1],
                                        scale[2]
                                        );
                        */

                        pBlock->SetArrayElement(JOINTS_MATRICES, i, mResTransform );
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
        if (!oSkeletonMeta.pShell) { return; }

        auto & vJointNodes      = oSkeletonMeta.pShell->JointNodes();
        auto pSkeleton          = oSkeletonMeta.pShell->GetSkeleton();

        for (uint32_t i = 0; i < oSkeletonMeta.vJointIndexes.size(); ++i) {

                if (pSkeleton->Joints()[oSkeletonMeta.vJointIndexes[i]].parent_ind == 255) {
                        continue;
                }

                auto j = pSkeleton->Joints()[oSkeletonMeta.vJointIndexes[i]].parent_ind;

                if (auto pStart = vJointNodes[oSkeletonMeta.vJointIndexes[i]].lock()) {
                        if (auto pEnd = vJointNodes[j].lock()) {

                                GetSystem<DebugRenderer>().DrawLine(
                                                pStart->GetTransform().GetWorldPos(),
                                                pEnd->GetTransform().GetWorldPos(),
                                                glm::vec4(0.8, 0.2, 0.15, 1.0));
                        }
                }
        }
}

const CharacterShell * AnimatedModel::GetShell() const {
        return oSkeletonMeta.pShell;
}

const std::vector<glm::mat4> & AnimatedModel::GetJointOffsetMat() const {

        return oSkeletonMeta.vJointBaseMat;
}

const std::vector<uint8_t> & AnimatedModel::GetJointIndexes() const {

        return oSkeletonMeta.vJointIndexes;
}

std::vector<glm::mat4> & AnimatedModel::GetJointOffsetMat() {

        return oSkeletonMeta.vJointBaseMat;
}

std::vector<uint8_t> & AnimatedModel::GetJointIndexes() {

        return oSkeletonMeta.vJointIndexes;
}

}

