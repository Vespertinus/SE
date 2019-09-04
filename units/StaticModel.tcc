
namespace SE {

StaticModel::StaticModel(TSceneTree::TSceneNodeExact * pNewNode) :
        pMesh(CreateResource<SE::TMesh>(GetSystem<Config>().sResourceDir + "mesh/default-01.sems")),
        pMaterial(CreateResource<SE::Material>(GetSystem<Config>().sResourceDir + "material/wireframe.semt")),
        pNode(pNewNode) {

        FillRenderCommands();
}

StaticModel::StaticModel(TSceneTree::TSceneNodeExact * pNewNode,
                         TMesh * pNewMesh,
                         Material * pNewMaterial) :
        pMesh(pNewMesh),
        pMaterial(pNewMaterial),
        pNode(pNewNode) {

        FillRenderCommands();

}

StaticModel::StaticModel() { ;; }


StaticModel::StaticModel(
                TSceneTree::TSceneNodeExact * pNewNode,
                const SE::FlatBuffers::StaticModel * pModel) : pNode(pNewNode) {

        //TODO
        const auto & oConfig = GetSystem<Config>();

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
                pMaterial = CreateResource<SE::Material>(oConfig.sResourceDir + "material/wireframe.semt");
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


void StaticModel::SetMesh(TMesh * pNewMesh) {

        pMesh = pNewMesh;
        FillRenderCommands();
}

void StaticModel::SetMaterial(Material * pNewMaterial) {

        pMaterial = pNewMaterial;

        FillRenderCommands();
        /*
        for (auto & oItem : vRenderCommands) {
                oItem.SetMaterial(pNewMaterial);
        }*/
}

std::string StaticModel::Str() const {

        return fmt::format("StaticModel: Mesh: '{}', Material: '{}'", pMesh->Name(), pMaterial->Name());
}

const std::vector<RenderCommand> & StaticModel::GetRenderCommands() const {

        return vRenderCommands;
}

void StaticModel::FillRenderCommands() {

        auto & vShapes = pMesh->GetShapes();

        vRenderCommands.clear();
        vRenderCommands.reserve(vShapes.size());

        for (auto & oSubMesh : vShapes) {
                vRenderCommands.emplace_back(&oSubMesh, pMaterial, pNode->GetTransform());
        }

        //TODO if already enabled -> invalidate render commands list!
        //need to disable \ enable
        //internal enabled state.. ? remove / add sequence
        //2 level state, node + component? Enable, Disable, UpdateState?
}

Material * StaticModel::GetMaterial() const {

                return pMaterial;
}

void StaticModel::DrawDebug() const {

        GetSystem<DebugRenderer>().DrawBBox(pMesh->GetBBox(), pNode->GetTransform());
}

}
