
#include <GeometryEntity.h>

namespace SE {

RenderCommand::RenderCommand(
                const GeometryEntity * pNewGeom,
                const Material * pNewMaterial,
                const glm::mat4 & oTransform) :
        pGeom(pNewGeom),
        pMaterial(pNewMaterial),
        oWorldTransform(oTransform)  {


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
        TRenderState::Instance().SetTransform(oWorldTransform);
        pGeom->Draw();
}


} //namespace SE
