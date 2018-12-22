
namespace SE {

AnimatedModel::AnimatedModel(TSceneTree::TSceneNodeExact * pNewNode,
                             bool enabled,
                             TMesh * pNewMesh,
                             Material * pNewMaterial,
                             const uint8_t blendshapes_cnt) :
        StaticModel(pNewNode, enabled, pNewMesh, pNewMaterial) {

        vWeights.resize(blendshapes_cnt, 0);
        if (!pMaterial->GetShader()->OwnTextureUnit(TextureUnit::BUFFER)) {

                throw(std::runtime_error(fmt::format(
                                                "wrong material: '{}', does not own TextureUnit::BUFFER, node: '{}'",
                                                pMaterial->Name(),
                                                pNode->GetFullName()
                                                )));
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

        if (!pMaterial->GetShader()->OwnTextureUnit(TextureUnit::BUFFER)) {

                throw(std::runtime_error(fmt::format(
                                                "wrong material: '{}', does not own TextureUnit::BUFFER, node: '{}'",
                                                pMaterial->Name(),
                                                pNode->GetFullName()
                                                )));
        }

        FillRenderCommands();

        auto bs_weights = pModel->blendshapes_weights()->Length();
        vWeights.reserve(bs_weights);
        for (uint32_t i = 0; i < bs_weights; ++i) {

                vWeights.emplace_back(pModel->blendshapes_weights()->Get(i));
        }

        auto * pTexBuffer = LoadTexture(pModel->blendshapes());

        if (!pTexBuffer || static_cast<TextureUnit>(pModel->blendshapes()->unit()) != TextureUnit::BUFFER) {

                throw(std::runtime_error(fmt::format("failed to load texture buffer from texture holder, tex: {:p}, texture_unit: {}, node: '{}'",
                                                (void *)pTexBuffer,
                                                EnumNameTextureUnit(pModel->blendshapes()->unit()),
                                                pNode->GetFullName())));
        }

        ret_code_t res = pMaterial->SetTexture(TextureUnit::BUFFER, pTexBuffer);
        if (res != uSUCCESS) {
                throw(std::runtime_error(fmt::format("failed to set texture buffer to material: '{}', node: '{}'",
                                                pMaterial->Name(),
                                                pNode->GetFullName())));

        }

        for (size_t i = 0; i < vWeights.size(); ++i) {

                res = pMaterial->SetVariable(vWeightsNames[i], vWeights[i]);
                if (res != uSUCCESS) {
                        throw(std::runtime_error(fmt::format("failed to set weight variable: '{}' to material: '{}', node: '{}'",
                                                        vWeightsNames[i],
                                                        pMaterial->Name(),
                                                        pNode->GetFullName())));
                }
        }
}

ret_code_t AnimatedModel::SetMaterial(Material * pNewMaterial, const uint8_t blendshapes_cnt) {

        if (!pNewMaterial->GetShader()->OwnTextureUnit(TextureUnit::BUFFER)) {

        log_e("wrong material: '{}', does not own TextureUnit::BUFFER, node: '{}'",
                        pNewMaterial->Name(),
                        pNode->GetFullName());
                return uWRONG_INPUT_DATA;
        }

        vWeights.resize(blendshapes_cnt, 0);
        pMaterial = pNewMaterial;

        FillRenderCommands();
        /*
        for (auto & oItem : vRenderCommands) {
                oItem.SetMaterial(pNewMaterial);
        }*/

        return uSUCCESS;
}

std::string AnimatedModel::Str() const {

        return fmt::format("AnimatedModel: Mesh: '{}', Material: '{}', bs cnt: {}",
                        pMesh->Name(),
                        pMaterial->Name(),
                        vWeights.size());
}

ret_code_t AnimatedModel::SetWeight(const uint8_t index, const float weight) {

        if (index >= vWeights.size()) {
                log_e("wrong weight index: {}, max allowed: {}, node: '{}'",
                                index,
                                vWeights.size(),
                                pNode->GetFullName());
                return uWRONG_INPUT_DATA;
        }

//        log_d("index: {}, weight: {}", index, weight);

        auto res = pMaterial->SetVariable(vWeightsNames[index], weight);
        if (res == uSUCCESS) {
                vWeights[index] = weight;
        }
        return res;
}

uint8_t AnimatedModel::BlendShapesCnt() const {
        return vWeights.size();
}

float AnimatedModel::GetWeight(const uint8_t index) {

        if (index >= vWeights.size()) {
                log_e("wrong weight index: {}, max allowed: {}, node: '{}'",
                                index,
                                vWeights.size(),
                                pNode->GetFullName());
                return 0;
        }
        return vWeights[index];
}

}

