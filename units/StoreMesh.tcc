
#include <GeometryUtil.h>

namespace SE {


StoreMesh::StoreMesh(const Settings & oNewSettings) : oSettings(oNewSettings) { ;; }



StoreMesh::~StoreMesh() throw() { }



ret_code_t StoreMesh::Store(MeshStock & oMeshStock, MeshCtx & oMeshCtx) {

        if (!oMeshStock.oMeshState.vShapes.size() ) {

                log_e("empty mesh data");
                return uWRONG_INPUT_DATA;
        }

        oMeshCtx.stride         = ((oMeshStock.oMeshState.skip_normals) ? VERTEX_BASE_SIZE : VERTEX_SIZE) * sizeof(float);
        oMeshCtx.min            = oMeshStock.oMeshState.min;
        oMeshCtx.max            = oMeshStock.oMeshState.max;
        oMeshCtx.skip_normals   = oMeshStock.oMeshState.skip_normals;

        log_d("mesh: shape cnt = {}, stride = {}, skip_normals = {}, min ({}, {}, {}), max({}, {}, {})",
                        oMeshStock.oMeshState.vShapes.size(),
                        oMeshCtx.stride,
                        oMeshCtx.skip_normals,
                        oMeshCtx.min.x,
                        oMeshCtx.min.y,
                        oMeshCtx.min.z,
                        oMeshCtx.max.x,
                        oMeshCtx.max.y,
                        oMeshCtx.max.z);

        for (auto & oItem : oMeshStock.oMeshState.vShapes) {

                uint32_t buf_id         = 0;

                glGenBuffers(1, &buf_id);
                glBindBuffer(GL_ARRAY_BUFFER, buf_id);
                glBufferData(GL_ARRAY_BUFFER,
                             oItem.vVertices.size() * sizeof(float),
                             &oItem.vVertices.at(0),
                             GL_STATIC_DRAW);

                log_d("shape[{}] name = '{}', triangles cnt = {}, buf_id = {}, texture id = {}, min x = {}, y = {}, z = {}, max x = {}, y = {}, z = {}",
                                &oItem - &oMeshStock.oMeshState.vShapes[0],
                                oItem.sName,
                                oItem.triangles_cnt,
                                buf_id,
                                (oItem.pTexture) ? oItem.pTexture->GetID() : 0,
                                oItem.min.x, oItem.min.y, oItem.min.z,
                                oItem.max.x, oItem.max.y, oItem.max.z);

                oMeshCtx.vShapes.emplace_back(ShapeCtx{ buf_id, oItem.triangles_cnt, oItem.pTexture, oItem.sName, oItem.min, oItem.max } );
        }

        return uSUCCESS;
}


} // namespace SE

