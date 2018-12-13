
namespace SE {

StaticModel::StaticModel(TSceneTree::TSceneNodeExact * pNewNode, bool enabled) :
        pMesh(CreateResource<SE::TMesh>("resource/mesh/default-01.sems")),
        pMaterial(CreateResource<SE::Material>("resource/material/wireframe.semt")),
        pNode(pNewNode) {

        FillRenderCommands();

        if (enabled) {
                Enable();
        }
}


StaticModel::StaticModel(
                TSceneTree::TSceneNodeExact * pNewNode,
                const SE::FlatBuffers::StaticModel * pModel) : pNode(pNewNode) {

        //TODO

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
                        pMaterial = CreateResource<Material>(pModel->material()->path()->c_str());
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
                pMaterial = CreateResource<SE::Material>("resource/material/wireframe.semt");
        }

        FillRenderCommands();
        Enable();
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
                vRenderCommands.emplace_back(&oSubMesh, pMaterial, pNode->GetTransform().GetWorld());
        }

        //THINK if already enabled -> invalidate render commands list???
        //internal enabled state.. ? remove / add sequence
}

}
