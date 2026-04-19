
#include <EntityTemplateUtility.h>

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

StaticModel::StaticModel() {}


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

namespace {

static void ApplyMaterialField(FlatBuffers::MaterialT& mat,
                                const std::vector<PathSegment>& segs, size_t i,
                                const FlatBuffers::FieldOverride& fo) {

        if (i >= segs.size()) { log_e("StaticModel::ApplyField: path ends inside material"); return; }
        const auto& seg = segs[i];

        if (seg.name == "blend_mode") {
                if (auto* p = fo.value_as_Int()) {
                        mat.blend_mode = static_cast<FlatBuffers::BlendMode>(p->value()); return;
                }
                log_e("StaticModel::ApplyField: blend_mode expects Int value"); return;
        }

        if (seg.name == "variables") {
                FlatBuffers::ShaderVariableT* pVar = nullptr;
                if (seg.index >= 0) {
                        if (static_cast<size_t>(seg.index) < mat.variables.size())
                                pVar = mat.variables[static_cast<size_t>(seg.index)].get();
                } else {
                        for (auto& pV : mat.variables)
                                if (pV && pV->name == seg.key) { pVar = pV.get(); break; }
                }
                if (!pVar) {
                        log_e("StaticModel::ApplyField: variable '{}' not found",
                              seg.index >= 0 ? std::to_string(seg.index) : seg.key);
                        return;
                }
                if (i + 1 >= segs.size()) { log_e("StaticModel::ApplyField: path ends at variable"); return; }
                const auto& leaf = segs[i + 1];
                if      (leaf.name == "float_val") { if (auto* p = fo.value_as_Float()) { pVar->float_val = p->value(); return; } }
                else if (leaf.name == "int_val")   { if (auto* p = fo.value_as_Int())   { pVar->int_val   = p->value(); return; } }
                else if (leaf.name == "vec2_val")  { if (auto* p = fo.value_as_Vec2())  { pVar->vec2_val  = std::make_unique<FlatBuffers::Vec2>(p->u(), p->v()); return; } }
                else if (leaf.name == "vec3_val")  { if (auto* p = fo.value_as_Vec3())  { pVar->vec3_val  = std::make_unique<FlatBuffers::Vec3>(p->x(), p->y(), p->z()); return; } }
                else if (leaf.name == "vec4_val")  { if (auto* p = fo.value_as_Vec4())  { pVar->vec4_val  = std::make_unique<FlatBuffers::Vec4>(p->x(), p->y(), p->z(), p->w()); return; } }
                log_e("StaticModel::ApplyField: unknown variable field '{}'", leaf.name); return;
        }

        if (seg.name == "textures") {
                FlatBuffers::TextureHolderT* pTex = nullptr;
                if (seg.index >= 0) {
                        if (static_cast<size_t>(seg.index) < mat.textures.size())
                                pTex = mat.textures[static_cast<size_t>(seg.index)].get();
                } else {
                        for (auto& pT : mat.textures)
                                if (pT && pT->name == seg.key) { pTex = pT.get(); break; }
                }
                if (!pTex) {
                        log_e("StaticModel::ApplyField: texture '{}' not found",
                              seg.index >= 0 ? std::to_string(seg.index) : seg.key);
                        return;
                }
                if (i + 1 >= segs.size()) { log_e("StaticModel::ApplyField: path ends at texture"); return; }
                const auto& leaf = segs[i + 1];
                if (leaf.name == "path") {
                        if (auto* p = fo.value_as_ResourcePath()) { pTex->path = p->path()->str(); return; }
                } else if (leaf.name == "name") {
                        if (auto* p = fo.value_as_ResourcePath()) { pTex->name = p->path()->str(); return; }
                }
                log_e("StaticModel::ApplyField: unknown texture field '{}'", leaf.name); return;
        }

        log_e("StaticModel::ApplyField: unknown material field '{}'", seg.name);
}

} // anonymous namespace

// static
void StaticModel::ApplyField(FlatBuffers::StaticModelT& obj,
                              std::string_view path,
                              const FlatBuffers::FieldOverride& fo) {

        const auto segs = EntityTemplateUtility::ParsePath(path);
        if (segs.empty()) { log_e("StaticModel::ApplyField: empty path"); return; }

        const auto& seg0 = segs[0];

        if (seg0.name == "mesh") {
                if (!obj.mesh) obj.mesh = std::make_unique<FlatBuffers::MeshHolderT>();
                if (segs.size() < 2) { log_e("StaticModel::ApplyField: path ends at mesh"); return; }
                const auto& seg1 = segs[1];
                if (seg1.name == "path") {
                        if (auto* p = fo.value_as_ResourcePath()) { obj.mesh->path = p->path()->str(); return; }
                } else if (seg1.name == "name") {
                        if (auto* p = fo.value_as_ResourcePath()) { obj.mesh->name = p->path()->str(); return; }
                }
                log_e("StaticModel::ApplyField: unknown mesh field '{}'", seg1.name); return;
        }

        if (seg0.name == "materials") {
                if (seg0.index < 0) {
                        log_e("StaticModel::ApplyField: 'materials' requires numeric index in '{}'", path); return;
                }
                if (static_cast<size_t>(seg0.index) >= obj.materials.size()) {
                        log_e("StaticModel::ApplyField: materials[{}] out of range (size={})",
                              seg0.index, obj.materials.size()); return;
                }
                auto& pHolder = obj.materials[static_cast<size_t>(seg0.index)];
                if (!pHolder) pHolder = std::make_unique<FlatBuffers::MaterialHolderT>();

                if (segs.size() < 2) { log_e("StaticModel::ApplyField: path ends at materials[{}]", seg0.index); return; }
                const auto& seg1 = segs[1];

                if (seg1.name == "path") {
                        if (auto* p = fo.value_as_ResourcePath()) { pHolder->path = p->path()->str(); return; }
                } else if (seg1.name == "name") {
                        if (auto* p = fo.value_as_ResourcePath()) { pHolder->name = p->path()->str(); return; }
                } else if (seg1.name == "material") {
                        if (!pHolder->material) pHolder->material = std::make_unique<FlatBuffers::MaterialT>();
                        ApplyMaterialField(*pHolder->material, segs, 2, fo);
                        return;
                }
                log_e("StaticModel::ApplyField: unknown MaterialHolder field '{}'", seg1.name); return;
        }

        log_e("StaticModel::ApplyField: unknown field '{}'", seg0.name);
}

}
