
#include <GeometryEntity.h>
#include <ShaderProgramState.h>

namespace SE {

RenderCommand::RenderCommand(
                const GeometryEntity * pNewGeom,
                const Material * pNewMaterial,
                const Transform & oNewTransform) :
        pGeom(pNewGeom),
        pMaterial(pNewMaterial),
        oState(pMaterial->GetShader()),
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


        //UpdateKey
}

void RenderCommand::Draw() const {

        //pMaterial->Apply();
        oState.Apply();
        TGraphicsState::Instance().SetTransform(oTransform.GetWorld()); //TEMP
        pGeom->Draw();
}


} //namespace SE
