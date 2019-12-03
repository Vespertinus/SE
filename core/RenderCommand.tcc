
#include <GeometryEntity.h>
#include <ShaderProgramState.h>

namespace SE {

RenderCommand::RenderCommand(
                const GeometryEntity * pNewGeom,
                const Material * pNewMaterial,
                const Transform & oNewTransform) :
        pGeom(pNewGeom),
        pMaterial(pNewMaterial),
        oState(pMaterial),
        oTransform(oNewTransform)  {


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

        //UpdateKey
}

void RenderCommand::Draw() const {

        oState.Apply();
        GetSystem<GraphicsState>().SetTransform(oTransform.GetWorld()); //TEMP
        pGeom->Draw();
}

ShaderProgramState & RenderCommand::State() {
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
