
#include <stdint.h>
#include <string_view>

#include <fbxsdk.h>
#include "FBXReader.h"

#include <Logging.h>
#include <GeometryUtil.h>

#include <Mesh_generated.h>

/**

  import from fbx:
        scene hierarchy
                - for each node:
                        - translation (3f)
                        - rotation (3f)
                        - scale (3f)
               - for each Mesh node:
                        - name
                        - first material diffuse texture path
                        - for each polygon:
                                - vertex position (3f)
                                - normal (3f)
                                - texture uv0 set (2f)
*/
//TODO split mesh on shapes based on material count and links to polygons

namespace SE {
namespace TOOLS {

ret_code_t ImportNode(FbxNode * pNode, NodeData & oNodeData, ImportCtx & oCtx);


FBXReader::FBXReader() {

        pManager = FbxManager::Create();
        if (pManager == nullptr) {
                throw(std::runtime_error("FBXReader: failed to create FbxManager") );
        }

        log_d("FBX SDK version: {}", pManager->GetVersion());

        FbxIOSettings * pIOS = FbxIOSettings::Create(pManager, IOSROOT);
        pManager->SetIOSettings(pIOS);
}

FBXReader::~FBXReader() noexcept {

        if(pManager) pManager->Destroy();
}

ret_code_t FBXReader::ReadScene(const std::string_view sPath, NodeData & oRootNode, ImportCtx & oCtx) {

        bool    result;
        int32_t file_version[3];
        int32_t sdk_version[3];

        std::unique_ptr<FbxScene, std::function<void (FbxScene *)> > pScene(FbxScene::Create(pManager, ""),
                        [](FbxScene * pObj) {
                                pObj->Destroy();
                        });
        if (!pScene) {
                log_e("failed to create FbxScene");
                return uEXT_LIBRARY_ERROR;
        }

        FbxManager::GetFileFormatVersion(sdk_version[0], sdk_version[1], sdk_version[2]);

        std::unique_ptr <FbxImporter, std::function<void(FbxImporter *)> > pImporter(FbxImporter::Create(pManager,""),
                        [](FbxImporter * pObj) {
                                pObj->Destroy();
                        });

        if (!pImporter) {
                log_e("failed to create FbxImporter");
                return uEXT_LIBRARY_ERROR;
        }

        result = pImporter->Initialize(sPath.data(), -1, pManager->GetIOSettings());
        pImporter->GetFileVersion(file_version[0], file_version[1], file_version[2]);

        if (!result) {
                log_e("failed to initialize FbxImporter, reason: '{}'", pImporter->GetStatus().GetErrorString());
                if (pImporter->GetStatus().GetCode() == FbxStatus::eInvalidFileVersion) {
                        log_e("FBX file format version for this FBX SDK is {}.{}.{}",
                                        sdk_version[0],
                                        sdk_version[1],
                                        sdk_version[2]);
                        log_e("FBX file format version for file '{}' is {}.{}.{}",
                                        sPath,
                                        file_version[0],
                                        file_version[1],
                                        file_version[2]);
                }
                return uREAD_FILE_ERROR;
        }

        log_d("FBX file format version for this FBX SDK is {}.{}.{}",
                        sdk_version[0],
                        sdk_version[1],
                        sdk_version[2]);
        log_d("FBX file format version for file '{}' is {}.{}.{}",
                        sPath,
                        file_version[0],
                        file_version[1],
                        file_version[2]);

        result = pImporter->Import(pScene.get());
        pImporter.reset();

        if (!result) {

                if (pImporter->GetStatus().GetCode() == FbxStatus::ePasswordError) {
                        log_e("failed to import file '{}', reason: protected by password", sPath);
                }
                else {
                        log_e("failed to import file '{}'", sPath);
                }
                return uREAD_FILE_ERROR;
        }

        FbxGeometryConverter oGeomConverter(pManager);
        if (oGeomConverter.Triangulate(pScene.get(), true) == false) {
                log_e("failed to triangulate scene from: '{}'", sPath);
                return uEXT_LIBRARY_ERROR;
        }

        //TODO import camera settings

        FbxNode * pNode = pScene->GetRootNode();
        if (pNode == nullptr) {
                log_e("empty fbx scene, can't get root node. imported from: '{}'", sPath);
                return uWRONG_INPUT_DATA;
        }
        return ImportNode(pNode, oRootNode, oCtx);
}

static ret_code_t GetUV(
                FbxGeometryElementUV * pUV,
                FbxNode * pNode,
                FbxMesh * pMesh,
                const int32_t polygon_num,
                const int32_t polygon_vert_ind,
                const int32_t vertex_ind,
                FbxVector2 & oUV) {

        switch (pUV->GetMappingMode()) {

                case FbxGeometryElement::eByControlPoint:
                        switch (pUV->GetReferenceMode())
                        {
                                case FbxGeometryElement::eDirect:
                                        oUV = pUV->GetDirectArray().GetAt(vertex_ind);
                                        break;
                                case FbxGeometryElement::eIndexToDirect:
                                        {
                                                int id = pUV->GetIndexArray().GetAt(vertex_ind);
                                                oUV = pUV->GetDirectArray().GetAt(id);
                                        }
                                        break;
                                default:
                                        log_e("unsupported UV ReferenceMode in polygon {}, (allowed: Direct or IndexToDirect), current ReferenceMode = {}, Mesh: '{}'",
                                                        polygon_num,
                                                        pUV->GetReferenceMode(),
                                                        pNode->GetName());
                                        return uWRONG_INPUT_DATA;
                        }
                        break;

                case FbxGeometryElement::eByPolygonVertex:
                        {
                                int32_t UVIndex = pMesh->GetTextureUVIndex(polygon_num, polygon_vert_ind);
                                switch (pUV->GetReferenceMode()) {
                                        case FbxGeometryElement::eDirect:
                                        case FbxGeometryElement::eIndexToDirect:
                                                {
                                                        oUV = pUV->GetDirectArray().GetAt(UVIndex);
                                                }
                                                break;
                                        default:
                                                log_e("unsupported UV ReferenceMode in polygon {}, (allowed: Direct or IndexToDirect)), current ReferenceMode = {}, Mesh: '{}'",
                                                                polygon_num,
                                                                pUV->GetReferenceMode(),
                                                                pNode->GetName());
                                                return uWRONG_INPUT_DATA;
                                }
                        }
                        break;
                default:
                        log_e("unsupported UV MappingMode = {}, mesh: '{}'", pUV->GetMappingMode(), pNode->GetName() );
                        return uWRONG_INPUT_DATA;
        }
/*
        log_d("polygon: {}, vertex: local index {}, global index {}, tex coord: {}, {}",
                        polygon_num,
                        polygon_vert_ind,
                        vertex_ind,
                        oUV[0],
                        oUV[1]);
*/

        return uSUCCESS;
}

/*
static void PolygonFlipYZ(float * data, const uint8_t elem_size) {

        float old_y[3] = { data[1], data[1 + elem_size], data[1 + elem_size * 2]};

        data[1]                 = - data[2];
        data[1 + elem_size]     = - data[2 + elem_size];
        data[1 + elem_size * 2] = - data[2 + elem_size * 2];

        data[2]                 = old_y[0];
        data[2 + elem_size]     = old_y[1];
        data[2 + elem_size * 2] = old_y[2];
}
*/
static void VertexFlipYZ(float * data) {

        float old_y = data[1];

        data[1] = - data[2];
        data[2] = old_y;
}

static ret_code_t ImportAttributes(FbxNode * pNode, NodeData & oNodeData, ImportCtx & oCtx) {

        FbxNodeAttribute::EType attribute_type;
        std::vector<float>      vVertexData;
        vVertexData.reserve(8);
        VertexIndex             oVertexIndex;
        uint32_t                cur_index = 0;
        TPackVertexIndex        Pack;

        auto * pAttribute = pNode->GetNodeAttribute();
        if (pAttribute == nullptr) {
                log_d("node '{}' does't contain attribute", pNode->GetName());
                return uSUCCESS;
        }

        attribute_type = pAttribute->GetAttributeType();

        if (attribute_type != FbxNodeAttribute::eMesh) {

                log_d("empty node: '{}'", pNode->GetName());
                return uSUCCESS;
        }

        log_d("mesh node: '{}', name: '{}'", pNode->GetName(), pAttribute->GetName());
        FbxMesh * pMesh = (FbxMesh *) pNode->GetNodeAttribute ();

        int32_t         vertices_cnt    = pMesh->GetControlPointsCount();
        FbxVector4    * pControlPoints  = pMesh->GetControlPoints();

        log_d("vertices cnt: {}", vertices_cnt);

        if ( pMesh->GetElementUVCount() == 0) {
                log_e("failed to get UV coordinates for mesh '{}'", pNode->GetName());
                return uWRONG_INPUT_DATA;
        }

        std::vector<float> vVertices;

        MeshData        oMesh;
        ShapeData       oShapeData;
        oMesh.sName                     = pAttribute->GetName();
        oShapeData.stride               = ((oCtx.skip_normals) ? VERTEX_BASE_SIZE : VERTEX_SIZE) * sizeof(float);

        if (pNode->GetSrcObjectCount<FbxSurfaceMaterial>() > 0) {

                FbxSurfaceMaterial * pMaterial = pNode->GetSrcObject<FbxSurfaceMaterial>(0);

                if(pMaterial) {

                        auto oProperty = pMaterial->FindProperty(FbxSurfaceMaterial::sDiffuse);

                        if (oProperty.IsValid() &&  oProperty.GetSrcObjectCount<FbxTexture>() > 0) {

                                if (FbxLayeredTexture * pLayeredTexture = oProperty.GetSrcObject<FbxLayeredTexture>(0);
                                                pLayeredTexture != nullptr &&
                                                pLayeredTexture->GetSrcObjectCount<FbxTexture>() > 0) {

                                        FbxTexture * pTexture = pLayeredTexture->GetSrcObject<FbxTexture>(0);
                                        if (pTexture != nullptr) {
                                                FbxFileTexture * pFileTexture = FbxCast<FbxFileTexture>(pTexture);
                                                oShapeData.sTextureName = pFileTexture->GetFileName();
                                                oCtx.FixPath(oShapeData.sTextureName);
                                                ++oCtx.textures_cnt;
                                        }
                                }
                                else if (FbxTexture * pTexture = oProperty.GetSrcObject<FbxTexture>(0) ) {
                                        FbxFileTexture * pFileTexture = FbxCast<FbxFileTexture>(pTexture);
                                        oShapeData.sTextureName = pFileTexture->GetFileName();
                                        oCtx.FixPath(oShapeData.sTextureName);
                                        ++oCtx.textures_cnt;
                                }
                        }
                }
        }

        log_d("diffuse texture: '{}'", oShapeData.sTextureName);

        int32_t polygon_cnt             = pMesh->GetPolygonCount();
        int32_t polygon_size;
        FbxGeometryElementUV * pUV      = pMesh->GetElementUV(0);
        log_d("polygons: cnt = {}", polygon_cnt);
        oShapeData.triangles_cnt           = static_cast<uint32_t>(polygon_cnt);

        uint32_t index_size = oShapeData.triangles_cnt * 3;

        Pack = PackVertexIndexInit(index_size, oShapeData.oIndex);

        for (int32_t polygon_num = 0; polygon_num < polygon_cnt; ++polygon_num) {

                polygon_size = pMesh->GetPolygonSize(polygon_num);

                if (polygon_size != 3) {
                        log_e("wrong polygon size == {}, must be triangle! (scene was triangulate previously)", polygon_size);
                        return uWRONG_INPUT_DATA;
                }

                for (int32_t polygon_vert_ind = 0; polygon_vert_ind < polygon_size; ++polygon_vert_ind) {

                        int32_t vertex_ind = pMesh->GetPolygonVertex(polygon_num, polygon_vert_ind);
                        /*
                           log_d("polygon: {}, vertex: local index {}, global index {}, pos {}, {}, {}",
                           polygon_num,
                           polygon_vert_ind,
                           vertex_ind,
                           pControlPoints[vertex_ind][0],
                           pControlPoints[vertex_ind][1],
                           pControlPoints[vertex_ind][2]);

                         */

                        vVertexData.clear();

                        vVertexData.push_back(pControlPoints[vertex_ind][0]);
                        vVertexData.push_back(pControlPoints[vertex_ind][1]);
                        vVertexData.push_back(pControlPoints[vertex_ind][2]);

                        if (!oCtx.skip_normals) {
                                FbxVector4 oNormal;
                                bool res = pMesh->GetPolygonVertexNormal(polygon_num, polygon_vert_ind, oNormal);
                                if (!res) {
                                        log_e("failed to get normal for: polygon {}, local vert ind {}, global vert ind {}, mesh: '{}'",
                                                        polygon_num,
                                                        polygon_vert_ind,
                                                        vertex_ind,
                                                        pNode->GetName());
                                        return uWRONG_INPUT_DATA;
                                        //TODO calc normals
                                }

                                vVertexData.push_back(oNormal[0]);
                                vVertexData.push_back(oNormal[1]);
                                vVertexData.push_back(oNormal[2]);
                                /*
                                   log_d("polygon: {}, vertex: local index {}, global index {}, normal: {}, {}, {}",
                                   polygon_num,
                                   polygon_vert_ind,
                                   vertex_ind,
                                   oNormal[0],
                                   oNormal[1],
                                   oNormal[2]);
                                 */
                        }


                        //TODO replace with GetPolygonVertexUV
                        FbxVector2 oUV;
                        ret_code_t result = GetUV(pUV, pNode, pMesh, polygon_num, polygon_vert_ind, vertex_ind, oUV);
                        if (result != uSUCCESS) {
                                log_e("failed to get uv");
                                return result;
                        }

                        vVertexData.push_back(oUV[0]);
                        vVertexData.push_back(oUV[1]);

                        if (oVertexIndex.Get(vVertexData, cur_index)) {

                                if (oCtx.flip_yz) {
                                        VertexFlipYZ(&vVertexData[0]);
                                        if (!oCtx.skip_normals) {
                                                VertexFlipYZ(&vVertexData[3]);
                                        }
                                }

                                log_d("new index = {}, pos ({}, {}, {}), rot ({}, {}, {}), uv ({}, {})",
                                                cur_index,
                                                vVertexData[0], vVertexData[1], vVertexData[2],
                                                (oCtx.skip_normals) ? 0 : vVertexData[3],
                                                (oCtx.skip_normals) ? 0 : vVertexData[4],
                                                (oCtx.skip_normals) ? 0 : vVertexData[5],
                                                vVertexData[6], vVertexData[7]);

                                vVertices.insert(vVertices.end(), vVertexData.begin(), vVertexData.end());
                                ++oCtx.total_vertices_cnt;
                        }
                        else {
                                log_d("old index = {}, pos ({}, {}, {}), rot ({}, {}, {}), uv ({}, {})",
                                                cur_index,
                                                vVertexData[0], vVertexData[1], vVertexData[2],
                                                (oCtx.skip_normals) ? 0 : vVertexData[3],
                                                (oCtx.skip_normals) ? 0 : vVertexData[4],
                                                (oCtx.skip_normals) ? 0 : vVertexData[5],
                                                vVertexData[6], vVertexData[7]);
                        }
                        Pack(oShapeData.oIndex, cur_index);
                }
        }

        oCtx.total_triangles_cnt += oShapeData.triangles_cnt;
        ++oCtx.mesh_cnt;


        CalcBasicBBox(vVertices,  (oCtx.skip_normals) ? VERTEX_BASE_SIZE : VERTEX_SIZE, oShapeData.min, oShapeData.max);
        oShapeData.vVertexBuffers.emplace_back(std::move(vVertices));

        uint16_t next_offset = 3;

        oShapeData.vAttributes.emplace_back(ShapeData::VertexAttribute{
                        "Position",
                        0,
                        3,
                        0 });
        if (!oCtx.skip_normals) {
                oShapeData.vAttributes.emplace_back(ShapeData::VertexAttribute{
                                "Normal",
                                static_cast<uint16_t>(3 * sizeof(float)),
                                3,
                                0 });
                next_offset = 6;
        }
        oShapeData.vAttributes.emplace_back(ShapeData::VertexAttribute{
                        "TexCoord0",
                        static_cast<uint16_t>(next_offset * sizeof(float)),
                        2,
                        0 });

        oMesh.vShapes.emplace_back(std::move(oShapeData));
        CalcCompositeBBox(oMesh.vShapes, oMesh.min, oMesh.max);
        oNodeData.vEntity.emplace_back(std::move(oMesh));

        return uSUCCESS;
}

ret_code_t ImportNode(FbxNode * pNode, NodeData & oNodeData, ImportCtx & oCtx) {

        FbxDouble3 translation  = pNode->LclTranslation.Get();
        FbxDouble3 rotation     = pNode->LclRotation.Get();
        FbxDouble3 scaling      = pNode->LclScaling.Get();

        if (oCtx.flip_yz) {
                oNodeData.translation.x = translation[0];
                oNodeData.translation.y = - translation[2];
                oNodeData.translation.z = translation[1];

                oNodeData.rotation.x = rotation[0];
                oNodeData.rotation.y = - rotation[2];
                oNodeData.rotation.z = rotation[1];

                oNodeData.scale.x = scaling[0];
                oNodeData.scale.y = - scaling[2];
                oNodeData.scale.z = scaling[1];
        }
        else {
                oNodeData.translation.x = translation[0];
                oNodeData.translation.y = translation[1];
                oNodeData.translation.z = translation[2];

                oNodeData.rotation.x = rotation[0];
                oNodeData.rotation.y = rotation[1];
                oNodeData.rotation.z = rotation[2];

                oNodeData.scale.x = scaling[0];
                oNodeData.scale.y = scaling[1];
                oNodeData.scale.z = scaling[2];
        }

        oNodeData.sName = pNode->GetName();

        log_d("node translation: {}, {}, {}", oNodeData.translation.x, oNodeData.translation.y, oNodeData.translation.z);
        log_d("node rotation:    {}, {}, {}", oNodeData.rotation.x, oNodeData.rotation.y, oNodeData.rotation.z);
        log_d("node scaling:     {}, {}, {}", oNodeData.scale.x, oNodeData.scale.y, oNodeData.scale.z);

        ret_code_t res = ImportAttributes(pNode, oNodeData, oCtx);
        if (res != uSUCCESS) {
                return res;
        }

        for(int32_t i = 0; i < pNode->GetChildCount(); ++i) {
                auto & oChildNodeData = oNodeData.vChildren.emplace_back(NodeData{});
                if (ret_code_t res = ImportNode(pNode->GetChild(i), oChildNodeData, oCtx); res != uSUCCESS) {
                        log_e("failed to import node");
                        return res;
                }
                ++oCtx.node_cnt;
        }

        ++oCtx.node_cnt;
        return uSUCCESS;
}


}

}
