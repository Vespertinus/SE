
#include <stdint.h>
#include <string_view>

#include <fbxsdk.h>
#include "FBXReader.h"

#include <Logging.h>
#include <GeometryUtil.h>

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
/*
        for(int32_t i = 0; i < pNode->GetChildCount(); ++i) {
                if (ret_code_t res = ImportNode(pNode->GetChild(i)); res != uSUCCESS) {
                        log_e("failed to import node");
                        return res;
                }
        }

        return uSUCCESS;
*/
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
        log_d("polygon: {}, vertex: local index {}, global index {}, tex coord: {}, {}",
                        polygon_num,
                        polygon_vert_ind,
                        vertex_ind,
                        oUV[0],
                        oUV[1]);

        return uSUCCESS;
}

static ret_code_t ImportAttributes(FbxNode * pNode, NodeData & oNodeData, ImportCtx & oCtx) {

        FbxNodeAttribute::EType attribute_type;

        auto * pAttribute = pNode->GetNodeAttribute();
        if (pAttribute == nullptr) {
                log_d("node '{}' does't contain attribute", pNode->GetName());
                return uSUCCESS;
        }

        attribute_type = pAttribute->GetAttributeType();

        if (attribute_type != FbxNodeAttribute::eMesh) {

                log_d("empty node: '{}'", pNode->GetName());
        }
        else {
                log_d("mesh node: '{}'", pNode->GetName());
                FbxMesh * pMesh = (FbxMesh *) pNode->GetNodeAttribute ();

                int32_t         vertices_cnt    = pMesh->GetControlPointsCount();
                FbxVector4    * pControlPoints  = pMesh->GetControlPoints();

                log_d("vertices cnt: {}", vertices_cnt);

                if ( pMesh->GetElementUVCount() == 0) {
                        log_e("failed to get UV coordinates for mesh '{}'", pNode->GetName());
                        return uWRONG_INPUT_DATA;
                }

                MeshData        oMesh;
                ShapeData       oShapeData;
                oMesh.skip_normals              = oCtx.skip_normals;

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
                oShapeData.triangles_cnt        = static_cast<uint32_t>(polygon_cnt);

                for (int32_t polygon_num = 0; polygon_num < polygon_cnt; ++polygon_num) {

                        polygon_size = pMesh->GetPolygonSize(polygon_num);

                        if (polygon_size != 3) {
                                log_e("wrong polygon size == {}, must be triangle! (scene was triangulate previously)", polygon_size);
                                return uWRONG_INPUT_DATA;
                        }

                        for (int32_t polygon_vert_ind = 0; polygon_vert_ind < polygon_size; ++polygon_vert_ind) {

                                int32_t vertex_ind = pMesh->GetPolygonVertex(polygon_num, polygon_vert_ind);

                                log_d("polygon: {}, vertex: local index {}, global index {}, pos {}, {}, {}",
                                                polygon_num,
                                                polygon_vert_ind,
                                                vertex_ind,
                                                pControlPoints[vertex_ind][0],
                                                pControlPoints[vertex_ind][1],
                                                pControlPoints[vertex_ind][2]);

                                oShapeData.vVertices.push_back(pControlPoints[vertex_ind][0]);
                                oShapeData.vVertices.push_back(pControlPoints[vertex_ind][1]);
                                oShapeData.vVertices.push_back(pControlPoints[vertex_ind][2]);

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
                                        oShapeData.vVertices.push_back(oNormal[0]);
                                        oShapeData.vVertices.push_back(oNormal[1]);
                                        oShapeData.vVertices.push_back(oNormal[2]);

                                        log_d("polygon: {}, vertex: local index {}, global index {}, normal: {}, {}, {}",
                                                        polygon_num,
                                                        polygon_vert_ind,
                                                        vertex_ind,
                                                        oNormal[0],
                                                        oNormal[1],
                                                        oNormal[2]);
                                }


                                //TODO replace with GetPolygonVertexUV
                                FbxVector2 oUV;
                                ret_code_t result = GetUV(pUV, pNode, pMesh, polygon_num, polygon_vert_ind, vertex_ind, oUV);
                                if (result != uSUCCESS) {
                                        log_e("failed to get uv");
                                        return result;
                                }

                                oShapeData.vVertices.push_back(oUV[0]);
                                oShapeData.vVertices.push_back(oUV[1]);
                        }

                }

                oCtx.total_triangles_cnt += oShapeData.triangles_cnt;
                ++oCtx.mesh_cnt;

                CalcBasicBBox(oShapeData.vVertices,  (oCtx.skip_normals) ? VERTEX_BASE_SIZE : VERTEX_SIZE, oShapeData.min, oShapeData.max);
                oMesh.vShapes.emplace_back(std::move(oShapeData));
                CalcCompositeBBox(oMesh.vShapes, oMesh.min, oMesh.max);
                oNodeData.vEntity.emplace_back(std::move(oMesh));

        }

        return uSUCCESS;
}

ret_code_t ImportNode(FbxNode * pNode, NodeData & oNodeData, ImportCtx & oCtx) {

        FbxDouble3 translation  = pNode->LclTranslation.Get();
        FbxDouble3 rotation     = pNode->LclRotation.Get();
        FbxDouble3 scaling      = pNode->LclScaling.Get();

        log_d("node translation: {}, {}, {}", translation[0], translation[1], translation[2]);
        log_d("node rotation:    {}, {}, {}", rotation[0], rotation[1], rotation[2]);
        log_d("node scaling:     {}, {}, {}", scaling[0], scaling[1], scaling[2]);

        oNodeData.translation.x = translation[0];
        oNodeData.translation.y = translation[1];
        oNodeData.translation.z = translation[2];

        oNodeData.rotation.x = rotation[0];
        oNodeData.rotation.y = rotation[1];
        oNodeData.rotation.z = rotation[2];

        oNodeData.scale.x = scaling[0];
        oNodeData.scale.y = scaling[1];
        oNodeData.scale.z = scaling[2];

        oNodeData.sName = pNode->GetName();

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
