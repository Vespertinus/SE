
// Headless StaticModel double — impl for the shadow header.
// Same declarations as units/StaticModel.tcc, but no GL resource loads and no
// renderer registration. Compiled once per TU through CoreComponents' impl
// section (SE_IMPL).

#ifdef SE_IMPL

#include <StaticModel.h>

#include <Logging.h>

namespace SE {

StaticModel::StaticModel() {}

StaticModel::StaticModel(TSceneTree::TSceneNodeExact * pNewNode)
        : pNode(pNewNode) {}

StaticModel::StaticModel(TSceneTree::TSceneNodeExact * pNewNode,
                         H<TMesh> hNewMesh,
                         H<Material> hNewMaterial)
        : hMesh(hNewMesh),
          pNode(pNewNode) {
        hMaterials.push_back(hNewMaterial);
}

StaticModel::StaticModel(TSceneTree::TSceneNodeExact  * pNewNode,
                         const SE::FlatBuffers::StaticModel * /*pModel*/)
        : pNode(pNewNode) {
        // production parses mesh/material refs here and loads them through the
        // manager — both loads touch GL and are skipped in the headless tier
}

StaticModel::~StaticModel() noexcept {

        Disable();
}

void StaticModel::FillRenderCommands() {}

void StaticModel::SetMesh(H<TMesh> hNewMesh) {

        hMesh = hNewMesh;
}

void StaticModel::SetMaterial(H<Material> hNewMaterial) {

        if (hMaterials.empty()) {
                hMaterials.push_back(hNewMaterial);
        }
        else {
                hMaterials[0] = hNewMaterial;
        }
}

void StaticModel::AddMaterial(H<Material> hNewMaterial) {

        hMaterials.push_back(hNewMaterial);
}

Material * StaticModel::GetMaterial(size_t idx) const {

        if (idx >= hMaterials.size()) { return nullptr; }
        return GetResource(hMaterials[idx]);
}

void StaticModel::Enable() {}

void StaticModel::Disable() {}

void StaticModel::Print(const size_t /*indent*/) {}

std::string StaticModel::Str() const {
        return fmt::format("StaticModel(shadow)[node='{}']", pNode ? pNode->GetName() : "");
}

const std::vector<RenderCommand> & StaticModel::GetRenderCommands() const {

        return vRenderCommands;
}

void StaticModel::DrawDebug() const {}

} // namespace SE

#endif // SE_IMPL
