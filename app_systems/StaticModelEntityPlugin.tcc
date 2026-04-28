
#include <EntityTemplateUtility.h>

namespace SE {

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

void EntityTemplatePlugin<StaticModel>::ApplyField(FlatBuffers::StaticModelT& obj,
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
                        log_e("StaticModel::ApplyField: materials[{}] out of range (size={})", seg0.index, obj.materials.size());
                        return;
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

} // namespace SE
