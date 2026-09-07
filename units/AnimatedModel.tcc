
namespace SE {

const StrID AnimatedModel::BS_WEIGHT            = "BlendShapesWeights";
const StrID AnimatedModel::BS_WEIGHTS_CNT       = "BlendShapesCnt";
const StrID AnimatedModel::JOINTS_CNT           = "JointsCnt";
const StrID AnimatedModel::JOINTS_PER_VERTEX    = "JointsPerVertex";
const StrID AnimatedModel::JOINTS_MATRICES      = "JointsMatrices";

// Moved from Skeleton.tcc — only user is AnimatedModel
static glm::mat4 BuildTransform(const SE::FlatBuffers::BindSQT * pBindPose) {

        glm::mat4 mBind(1.0f);

        if (pBindPose) {

                glm::quat bind_qrot     = *reinterpret_cast<const glm::quat *>(pBindPose->bind_rot());
                glm::vec3 bind_pos      = *reinterpret_cast<const glm::vec3 *>(pBindPose->bind_pos());
                glm::vec3 bind_scale    = *reinterpret_cast<const glm::vec3 *>(pBindPose->bind_scale());

                glm::mat4 mTranslate    = glm::translate(glm::mat4(1.0), bind_pos);
                glm::mat4 mScale        = glm::scale   (glm::mat4(1.0), bind_scale);
                glm::mat4 mRotation     = glm::toMat4(bind_qrot);

                mBind  = mTranslate * mRotation * mScale;
        }

        return mBind;
}


ret_code_t AnimatedModel::SkeletonPart::FillData(
                const SE::FlatBuffers::AnimatedModel * pModel,
                TSceneTree::TSceneNodeExact * pTargetNode) {

        const auto * pJointsIndices     = pModel->joints_indexes();
        const auto * pJointsInvBindPose = pModel->joints_inv_bind_pose();
        const auto * pMeshBindPose      = pModel->mesh_bind_pos();

        if (!pJointsIndices || !pMeshBindPose) { return uSUCCESS; }

        //setup bind matrix
        mBindPose = BuildTransform(pMeshBindPose);

        if (pModel->skeleton_root_node())
                sSkeletonRootNode = pModel->skeleton_root_node()->c_str();

        if (pJointsIndices->Length() != pJointsInvBindPose->Length()) {
                log_e("joints data mismatch, indexes cnt {} != inv bind pose cnt {}, for node: '{}'",
                                pJointsIndices->Length(),
                                pJointsInvBindPose->Length(),
                                pTargetNode->GetFullName());
                return uWRONG_INPUT_DATA;
        }

        vJointsIndexes.reserve(pJointsIndices->Length());
        vJointsInvBindPose.reserve(pJointsInvBindPose->Length());

        for (uint16_t joint_ind = 0; joint_ind < pJointsIndices->Length(); ++joint_ind) {

                uint16_t cur_joint_ind = pJointsIndices->Get(joint_ind);

                log_d("add joint: {}, to node: '{}'", cur_joint_ind, pTargetNode->GetName());

                vJointsIndexes.emplace_back(cur_joint_ind);
                vJointsInvBindPose.emplace_back(BuildTransform(pJointsInvBindPose->Get(joint_ind)));
        }

        if (pModel->skeleton()) {
                const auto& oConfig = GetSystem<Config>();
                if (pModel->skeleton()->path()) {
                        hSkeleton = CreateResource<Skeleton>(
                                oConfig.sResourceDir + pModel->skeleton()->path()->c_str());
                }
                else if (pModel->skeleton()->name() && pModel->skeleton()->skeleton()) {
                        hSkeleton = CreateResource<Skeleton>(
                                pModel->skeleton()->name()->c_str(),
                                pModel->skeleton()->skeleton());
                }
        }

        return uSUCCESS;
}


AnimatedModel::AnimatedModel(TSceneTree::TSceneNodeExact * pNewNode,
                             H<TMesh> hNewMesh,
                             H<Material> hNewMaterial,
                             H<TTexture> hNewTexBuf,
                             const uint8_t new_blendshapes_cnt) :
        StaticModel(pNewNode, hNewMesh, hNewMaterial),
        hTexBuffer(hNewTexBuf),
        blendshapes_cnt(new_blendshapes_cnt) {

        auto * pMat = GetResource(hMaterials[0]);

        if (!pMat->GetShader()->OwnTextureUnit(TextureUnit::BUFFER)) {

                throw(std::runtime_error(fmt::format(
                                                "wrong material: '{}', shader: '{}', does not own TextureUnit::BUFFER, node: '{}'",
                                                pMat->Name(),
                                                pMat->GetShader()->Name(),
                                                pNode->GetFullName()
                                                )));
        }

        pBlock = std::make_unique<UniformBlock>(pMat->GetShader(), UniformUnitInfo::Type::ANIMATION);
        auto res = pBlock->SetVariable(BS_WEIGHTS_CNT, blendshapes_cnt);
        if (res != uSUCCESS) {
                throw(std::runtime_error(fmt::format("failed to set blendshape cnt variable: '{}', shader: '{}', node: '{}'",
                                                BS_WEIGHTS_CNT,
                                                pMat->GetShader()->Name(),
                                                pNode->GetFullName())));
        }
}


AnimatedModel::AnimatedModel(
                TSceneTree::TSceneNodeExact * pNewNode,
                const SE::FlatBuffers::AnimatedModel * pModel) {

        const auto & oConfig = GetSystem<Config>();

        pNode = pNewNode;

        if (pModel->mesh()->path() != nullptr) {
                hMesh = CreateResource<TMesh>(pModel->mesh()->path()->c_str());
        }
        else if (pModel->mesh()->name() != nullptr && pModel->mesh()->mesh() != nullptr) {

                hMesh = CreateResource<TMesh>(pModel->mesh()->name()->c_str(), pModel->mesh()->mesh());
        }
        else {
                throw(std::runtime_error(fmt::format("wrong mesh state, mesh {:p}, name {:p}",
                                                (void *)pModel->mesh()->mesh(),
                                                (void *)pModel->mesh()->name()
                                                )));
        }

        if (pModel->materials() && pModel->materials()->size() > 0) {
                for (uint32_t i = 0; i < pModel->materials()->size(); ++i) {
                        const auto * pMat = pModel->materials()->Get(i);
                        if (pMat->path() != nullptr) {
                                hMaterials.push_back(CreateResource<Material>(oConfig.sResourceDir + pMat->path()->c_str()));
                        }
                        else if (pMat->name() != nullptr && pMat->material() != nullptr) {
                                hMaterials.push_back(CreateResource<Material>(
                                                pMat->name()->c_str(),
                                                pMat->material()));
                        }
                        else {
                                throw(std::runtime_error(fmt::format("wrong material state at index {}, material {:p}, name {:p}",
                                                        i,
                                                        (void *)pMat->material(),
                                                        (void *)pMat->name()
                                                        )));
                        }
                }
        }
        else {
                hMaterials.push_back(CreateResource<SE::Material>(oConfig.sResourceDir + "material/default_morph.semt"));
        }

        auto * pMat = GetResource(hMaterials[0]);

        pBlock = std::make_unique<UniformBlock>(pMat->GetShader(), UniformUnitInfo::Type::ANIMATION);

        if (pModel->blendshapes_weights()) {

                if (!pMat->GetShader()->OwnTextureUnit(TextureUnit::BUFFER)) {

                        throw(std::runtime_error(fmt::format(
                                                        "wrong material: '{}', does not own TextureUnit::BUFFER, node: '{}'",
                                                        pMat->Name(),
                                                        pNode->GetFullName()
                                                        )));
                }


                blendshapes_cnt = pModel->blendshapes_weights()->Length();

                hTexBuffer = LoadTexture(pModel->blendshapes());

                if (!hTexBuffer.IsValid() || static_cast<TextureUnit>(pModel->blendshapes()->unit()) != TextureUnit::BUFFER) {

                        throw(std::runtime_error(fmt::format("failed to load texture buffer from texture holder, tex valid: {}, texture_unit: {}, node: '{}'",
                                                        hTexBuffer.IsValid(),
                                                        EnumNameTextureUnit(pModel->blendshapes()->unit()),
                                                        pNode->GetFullName())));
                }

                ret_code_t res;
                for (size_t i = 0; i < blendshapes_cnt; ++i) {

                        res = pBlock->SetArrayElement(BS_WEIGHT, i, pModel->blendshapes_weights()->Get(i));
                        if (res != uSUCCESS) {
                                throw(std::runtime_error(fmt::format("failed to set blendshape weight variable: '{}', shader: '{}', node: '{}'",
                                                                BS_WEIGHT,
                                                                pMat->GetShader()->Name(),
                                                                pNode->GetFullName())));
                        }
                }

                res = pBlock->SetVariable(BS_WEIGHTS_CNT, blendshapes_cnt);
                if (res != uSUCCESS) {
                        throw(std::runtime_error(fmt::format("failed to set blendshape cnt variable: '{}', shader: '{}', node: '{}'",
                                                        BS_WEIGHTS_CNT,
                                                        pMat->GetShader()->Name(),
                                                        pNode->GetFullName())));
                }

        }

}

AnimatedModel::~AnimatedModel() noexcept {

        Disable();
}

ret_code_t AnimatedModel::PostLoad(const SE::FlatBuffers::AnimatedModel * pModel) {

        auto res = oSkeletonMeta.FillData(pModel, pNode);

        if (res != uSUCCESS) { return res; }

        if (oSkeletonMeta.hSkeleton.IsValid()) {
                BindSkeleton(oSkeletonMeta.hSkeleton);
        }

        static StrID IndAttrID("JointIndices");

        if (!oSkeletonMeta.vJointsIndexes.empty()) {

                for (auto & oAttrInfo : GetResource(hMesh)->GetAttrInfo()) {
                        if (oAttrInfo.first == IndAttrID) {
                                joints_per_vertex = static_cast<uint8_t>(oAttrInfo.second);
                                break;
                        }
                }

                if (!joints_per_vertex || joints_per_vertex > 4) {
                        log_e("wrong joints number per vertex: {}, max 4 allowed", joints_per_vertex);
                        return uWRONG_INPUT_DATA;
                }

                res = pBlock->SetVariable(JOINTS_PER_VERTEX, static_cast<uint32_t>(joints_per_vertex));
                if (res != uSUCCESS) {
                        log_e("failed to set joints per vertex var: '{}', shader: '{}', node: '{}'",
                                        joints_per_vertex,
                                        GetResource(hMaterials[0])->GetShader()->Name(),
                                        pNode->GetFullName());
                        return res;
                }
        }

        FillRenderCommands();

        return res;
}

void AnimatedModel::BindSkeleton(H<Skeleton> hSkel) {

        // Unsubscribe from any previously bound joint nodes
        for (auto idx : oSkeletonMeta.vJointsIndexes) {
                if (idx < vJointNodes.size()) {
                        vJointNodes[idx]->RemoveListener(this);
                }
        }
        vJointNodes.clear();

        oSkeletonMeta.hSkeleton = hSkel;
        auto* pSkel = GetResource(oSkeletonMeta.hSkeleton);
        if (!pSkel) {
                log_e("AnimatedModel::BindSkeleton: null Skeleton on node '{}'", pNode->GetName());
                return;
        }

        if (oSkeletonMeta.sSkeletonRootNode.empty()) {
                log_w("AnimatedModel::BindSkeleton: no skeleton_root_node set on node '{}'", pNode->GetName());
                return;
        }

        auto* pScene = pNode->GetScene();
        auto* vNodes = pScene->FindLocalName(oSkeletonMeta.sSkeletonRootNode.c_str());

        if (!vNodes || vNodes->empty()) {
                log_e("AnimatedModel::BindSkeleton: skeleton root '{}' not found (or ambiguous) on node '{}'",
                      oSkeletonMeta.sSkeletonRootNode, pNode->GetName());
                return;
        }

        //THINK consider changing the candidate comparison method.
        // When multiple nodes share the same local name (multi-character scenes),
        // pick the one whose full path shares the longest prefix with this mesh node.
        auto pRootNode = (*vNodes)[0];
        if (vNodes->size() > 1) {
                const std::string& sMeshPath = pNode->GetFullName();
                size_t best = 0;
                for (const auto& candidate : *vNodes) {
                        const std::string& sCandPath = candidate->GetFullName();
                        size_t n = 0;
                        while (n < sMeshPath.size() && n < sCandPath.size() && sMeshPath[n] == sCandPath[n]) {
                                ++n;
                        }
                        if (n > best) {
                                best = n;
                                pRootNode = candidate;
                        }
                }
        }
        const auto& vBones = pSkel->Bones();
        vJointNodes.reserve(vBones.size());

        for (const auto& bone : vBones) {
                auto pJointNode = pRootNode->FindChild(bone.name, true);
                if (!pJointNode) {
                        // The skeleton root joint is not a child of itself; match it directly.
                        if (bone.name == pRootNode->GetName()) {
                                pJointNode = pRootNode;
                        } else {
                                throw std::runtime_error(fmt::format(
                                        "AnimatedModel::BindSkeleton: bone '{}' not found under root '{}', node '{}'",
                                        bone.name, oSkeletonMeta.sSkeletonRootNode, pNode->GetName()));
                        }
                }
                // vJointNodes owns shared refs: the joints cannot be destroyed while
                // this model lives, regardless of where the bind resolved them (skin
                // meshes are siblings of the bone nodes, not their ancestors).
                vJointNodes.emplace_back(pJointNode);
        }

        log_d("AnimatedModel::BindSkeleton: bound {} joint nodes on node '{}'",
              vJointNodes.size(), pNode->GetName());

        for (auto idx : oSkeletonMeta.vJointsIndexes) {
                if (idx < vJointNodes.size()) {
                        vJointNodes[idx]->AddListener(this);
                }
        }

        // Joint nodes already have their world transforms set from scene loading.
        // Since listeners weren't attached before BindSkeleton, TargetTransformChanged
        // never fired → skinning_dirty stayed false → matrices would stay zero.
        // Force a recompute on the next PostUpdate after every bind.
        skinning_dirty = true;
}

void AnimatedModel::TargetTransformChanged(TSceneTree::TSceneNodeExact* /* pTargetNode */) {

        skinning_dirty = true;
}

void AnimatedModel::MarkSkinningDirty() {

        skinning_dirty = true;
}

void AnimatedModel::PostUpdate(const Event & oEvent [[maybe_unused]]) {

        if (!skinning_dirty) { return; }
        if (oSkeletonMeta.vJointsIndexes.empty()) { return; }

        if (vJointNodes.empty()) { return; }

        //log_d("update skinning: node: '{}'", pNode->GetName());

        // Skin matrices are baked in MODEL space: the vertex shaders apply the model
        // world matrix (MVMatrix) on top of them, so the joint chain must be made
        // relative to the model node first — otherwise the model transform (capsule
        // movement, facing rotation) is applied twice to skinned vertices.
        const glm::mat4 mModelWorldInv = glm::inverse(pNode->GetTransform().GetWorld());

        for (size_t i = 0; i < oSkeletonMeta.vJointsIndexes.size(); ++i) {

                const uint16_t joint_idx = oSkeletonMeta.vJointsIndexes[i];
                if (joint_idx >= vJointNodes.size()) {
                        log_e("joint index {} out of range ({}), node: '{}'",
                                joint_idx, vJointNodes.size(), pNode->GetFullName());
                        continue;
                }

                if (const auto* pJointNode = vJointNodes[joint_idx].get()) {

                        auto & mJointInvBindPose = oSkeletonMeta.vJointsInvBindPose[i];

                        // Model-relative linear blend skinning:
                        // skinMatrix = inverse(modelWorld) * jointWorld * invBind
                        // invBind = inverse(bindPose) was precomputed at import.
                        glm::mat4 mResTransform =
                                mModelWorldInv
                                * pJointNode->GetTransform().GetWorld()
                                * mJointInvBindPose;

                        pBlock->SetArrayElement(JOINTS_MATRICES, i, mResTransform );
                }
                else {
                        // Unbound joint: identity keeps affected vertices at their
                        // bind pose position in model space.
                        pBlock->SetArrayElement(JOINTS_MATRICES, i, glm::mat4(1.0f));
                }
        }

        skinning_dirty = false;
}

ret_code_t AnimatedModel::SetMaterial(H<Material> hNewMaterial) {

        auto * pNewMaterial = GetResource(hNewMaterial);

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

                if (!oSkeletonMeta.vJointsIndexes.empty()) {
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


        hMaterials[0] = hNewMaterial;
        if (!oSkeletonMeta.vJointsIndexes.empty()) {
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
                        oItem.State().SetTexture(TextureUnit::BUFFER, hTexBuffer);
                }
        }
}

std::string AnimatedModel::Str() const {

        return fmt::format("AnimatedModel: Mesh: '{}', bs cnt: {}, joints: {}, Material: '{}'",
                        GetResource(hMesh)->Name(),
                        blendshapes_cnt,
                        oSkeletonMeta.vJointsIndexes.size(),
                        GetResource(hMaterials[0])->Name()
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

void AnimatedModel::Enable() {

        StaticModel::Enable();
        // Skin matrices are baked on EPreRenderUpdate: after physics transform
        // writeback (Interpolate), immediately before rendering. Enable/Disable
        // own this subscription — PostLoad must not re-add it (SceneNode calls
        // Enable before PostLoad, and RemoveListener removes a single entry).
        GetSystem<EventManager>().AddListener<EPreRenderUpdate, &AnimatedModel::PostUpdate>(this);

        for (auto idx : oSkeletonMeta.vJointsIndexes) {
                if (idx < vJointNodes.size()) {
                        vJointNodes[idx]->AddListener(this);
                }
        }
}

void AnimatedModel::Disable() {

        for (auto idx : oSkeletonMeta.vJointsIndexes) {
                if (idx < vJointNodes.size()) {
                        vJointNodes[idx]->RemoveListener(this);
                }
        }

        StaticModel::Disable();
        // Matches the AddListener in Enable(); the removal must happen exactly
        // once, otherwise the delegate outlives this component (use-after-free).
        GetSystem<EventManager>().RemoveListener<EPreRenderUpdate, &AnimatedModel::PostUpdate>(this);
}

void AnimatedModel::DrawDebug() const {

        StaticModel::DrawDebug();
}

const std::string & AnimatedModel::GetSkeletonRootNode() const {

        return oSkeletonMeta.sSkeletonRootNode;
}

const std::vector<uint16_t> & AnimatedModel::GetJointIndexes() const {

        return oSkeletonMeta.vJointsIndexes;
}

std::vector<uint16_t> & AnimatedModel::GetJointIndexes() {

        return oSkeletonMeta.vJointsIndexes;
}

}


