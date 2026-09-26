
#include <GeometryEntity.h>
#include <ShaderProgramState.h>

namespace SE {

static PoolAllocator<sizeof(RenderCommand)> sRenderCommandPool{1024};

void* RenderCommand::operator new(size_t)       { return sRenderCommandPool.allocate(); }
void  RenderCommand::operator delete(void* ptr) noexcept { sRenderCommandPool.deallocate(ptr); }

RenderCommand::RenderCommand(
                const GeometryEntity * pNewGeom,
                const Material * pNewMaterial,
                const Transform & oNewTransform) :
        pGeom(pNewGeom),
        pMaterial(pNewMaterial),
        oState(pMaterial),
        oTransform(oNewTransform) {


        //TEMP, FIXME
        if (!pGeom || !pMaterial) {
                throw(std::runtime_error(fmt::format(
                                                "wrong input data, geom: {:p}, material: {:p}",
                                                (void*)pGeom,
                                                (void*)pMaterial)));
        }

        if (auto * pBlock = pMaterial->GetUniformBlock()) {
                auto res = oState.SetBlock(UniformUnitInfo::Type::MATERIAL, pBlock);
                if (res != uSUCCESS) {
                        throw(std::runtime_error(fmt::format(
                                        "failed to set material block from '{}'",
                                        pMaterial->Name())));
                }
        }

        if (auto res = oState.SetTextures(UniformUnitInfo::Type::MATERIAL, pMaterial->GetTextures()); res != uSUCCESS) {
                throw(std::runtime_error(fmt::format(
                                "failed to set textures for material block from '{}'",
                                pMaterial->Name())));
        }

        UpdateKey();
}

/** 64-bit state-group key: [63:48] shader, [47:24] material, [23:0] geometry.
 *  Pointer-derived (heap pointers share address bits) and folded to keep the
 *  slices from aliasing; stable within a session, which is all sorting needs. */
void RenderCommand::UpdateKey() {

        const auto oFold16 = [](const void * p) -> uint64_t {
                const uint64_t v = reinterpret_cast<uintptr_t>(p);
                return (v >> 4) ^ (v >> 20) ^ (v >> 36) ^ (v >> 52);
        };

        sort_key = (oFold16(pMaterial->GetShader()) << 48)
                 | ((oFold16(pMaterial) & 0xFFFFFFull) << 24)
                 |  (oFold16(pGeom)       & 0xFFFFFFull);
}

void RenderCommand::SetMaterial(const Material * pNewMaterial) {

        if (!pNewMaterial || pNewMaterial == pMaterial) { return; }

        pMaterial = pNewMaterial;
        oState    = ShaderProgramState(pMaterial);
        UpdateKey();
}

void RenderCommand::Draw() const {

        oState.Apply();
        GetSystem<GraphicsState>().SetTransform(oTransform.GetWorld()); //TEMP
        pGeom->Draw();
}

void RenderCommand::DrawDepth() const {

        // Skinned models keep their animated pose in the shadow map: bind the
        // ANIMATION block (skin matrices) — the skinned depth shader applies
        // the same skinning as the geometry pass. Static models run the
        // block-free depth variant selected in ShadowPass (IsSkinned()); their
        // stale-binding hazard is gone because that program has no Animation
        // uniform at all.
        oState.ApplyBlock(UniformUnitInfo::Type::ANIMATION);

        GetSystem<GraphicsState>().SetTransform(oTransform.GetWorld());
        pGeom->Draw();
}

void RenderCommand::DrawTransparent() const {

        oState.ApplyMaterialStateOnly();
        GetSystem<GraphicsState>().SetTransform(oTransform.GetWorld());
        pGeom->Draw();
}

uint32_t RenderCommand::GetGeomIndexCount() const {
        return pGeom ? pGeom->GetIndicesCnt() : 0;
}

const Transform & RenderCommand::GetTransform() const {
        return oTransform;
}

ShaderProgramState & RenderCommand::State() {
        return oState;
}

const ShaderProgramState & RenderCommand::State() const {
        return oState;
}

std::string RenderCommand::StrDump(const size_t indent) const {

        std::string sResult = fmt::format("{:>{}} RenderCommand: \n", ">", indent);
        sResult += pGeom->StrDump(indent + 2) + "\n";
        sResult += oState.StrDump(indent + 2) + "\n";
        sResult += oTransform.StrDump(indent + 2);

        return sResult;
}

} //namespace SE
