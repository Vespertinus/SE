
namespace SE {

StaticModel::StaticModel(TSceneTree::TSceneNodeExact * pNewNode) :
        hMesh(CreateResource<SE::TMesh>(GetSystem<Config>().sResourceDir + "mesh/default-01.sems")),
        pNode(pNewNode) {

        hMaterials.push_back(CreateResource<SE::Material>(GetSystem<Config>().sResourceDir + "material/wireframe.semt"));
        FillRenderCommands();
}

StaticModel::StaticModel(TSceneTree::TSceneNodeExact * pNewNode,
                         H<TMesh> hNewMesh,
                         H<Material> hNewMaterial) :
        hMesh(hNewMesh),
        pNode(pNewNode) {

        hMaterials.push_back(hNewMaterial);
        FillRenderCommands();
}

StaticModel::StaticModel() { ;; }


StaticModel::StaticModel(
                TSceneTree::TSceneNodeExact * pNewNode,
                const SE::FlatBuffers::StaticModel * pModel) : pNode(pNewNode) {

        //TODO
        const auto & oConfig = GetSystem<Config>();

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
                                hMaterials.push_back(CreateResource<Material>(pMat->name()->c_str(), pMat->material()));
                        }
                        else {
                                throw(std::runtime_error(fmt::format("wrong material state at index {}, material {:p}, name {:p}",
                                                        i,
                                                        (void *)pMat->material(),
                                                        (void *)pMat->name())));
                        }
                }
        }
        else {
                hMaterials.push_back(CreateResource<SE::Material>(oConfig.sResourceDir + "material/wireframe.semt"));
        }

        FillRenderCommands();
}

StaticModel::~StaticModel() noexcept {

        Disable();
}

void StaticModel::Enable() {

        TEngine::Instance().Get<TRenderer>().AddRenderable(this);
}

void StaticModel::Disable() {

        TEngine::Instance().Get<TRenderer>().RemoveRenderable(this);
}


void StaticModel::SetMesh(H<TMesh> hNewMesh) {

        hMesh = hNewMesh;
        FillRenderCommands();
}

void StaticModel::SetMaterial(H<Material> hNewMaterial) {

        if (hMaterials.empty()) {
                hMaterials.push_back(hNewMaterial);
        }
        else {
                hMaterials[0] = hNewMaterial;
        }
        FillRenderCommands();
}

void StaticModel::AddMaterial(H<Material> hNewMaterial) {

        hMaterials.push_back(hNewMaterial);
        FillRenderCommands();
}

std::string StaticModel::Str() const {

        std::string sMat = hMaterials.empty() ? "none" : GetResource(hMaterials[0])->Name();
        return fmt::format("StaticModel: Mesh: '{}', Materials: {} (first: '{}')",
                        GetResource(hMesh)->Name(),
                        hMaterials.size(),
                        sMat);
}

const std::vector<RenderCommand> & StaticModel::GetRenderCommands() const {

        return vRenderCommands;
}

void StaticModel::FillRenderCommands() {

        auto & vShapes = GetResource(hMesh)->GetShapes();

        vRenderCommands.clear();
        vRenderCommands.reserve(vShapes.size());

        for (auto & oSubMesh : vShapes) {
                uint16_t mat_idx = oSubMesh.GetMaterialIndex();
                Material * pMat;
                if (hMaterials.empty()) {
                        pMat = nullptr;
                }
                else if (mat_idx < static_cast<uint16_t>(hMaterials.size())) {
                        pMat = GetResource(hMaterials[mat_idx]);
                }
                else {
                        pMat = GetResource(hMaterials[0]);
                }
                vRenderCommands.emplace_back(&oSubMesh, pMat, pNode->GetTransform());
        }

        //TODO if already enabled -> invalidate render commands list!
        //need to disable \ enable
        //internal enabled state.. ? remove / add sequence
        //2 level state, node + component? Enable, Disable, UpdateState?
}

Material * StaticModel::GetMaterial(size_t idx) const {

        if (idx < hMaterials.size()) return GetResource(hMaterials[idx]);
        return nullptr;
}

void StaticModel::DrawDebug() const {

        GetSystem<DebugRenderer>().DrawBBox(GetResource(hMesh)->GetBBox(), pNode->GetTransform());
}

}
