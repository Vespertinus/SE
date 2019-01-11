
#include <GeometryEntity.h>

namespace SE {

RenderCommand::RenderCommand(
                const GeometryEntity * pNewGeom,
                const Material * pNewMaterial,
                const Transform & oNewTransform) :
        pGeom(pNewGeom),
        pMaterial(pNewMaterial),
        oTransform(oNewTransform)  {


        //TEMP, FIXME
        if (!pGeom || !pMaterial) {
                throw(std::runtime_error(fmt::format(
                                                "wrong input data, geom: {:p}, material: {:p}",
                                                (void*)pGeom,
                                                (void*)pMaterial)));
        }

        //UpdateKey
}

void RenderCommand::Draw() const {

        pMaterial->Apply();
        TGraphicsState::Instance().SetTransform(oTransform.GetWorld());
        pGeom->Draw();
}


} //namespace SE
