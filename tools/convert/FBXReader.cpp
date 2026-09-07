
#include <stdint.h>
#include <string_view>
#include <unordered_set>
#include <fstream>
#include <algorithm>

#include <fbxsdk.h>
#include "FBXReader.h"

#include <Logging.h>
#include <GeometryUtil.h>
#include <StrID.h>

#include <BoundingBox.h>
#include <BoundingBox.tcc>

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
                        - material with diffuse / normal / specular / emissive textures
                        - shader auto-selected: pbr_geometry.sesp when normal or specular present
                        - for each polygon:
                                - vertex position (3f)
                                - normal (3f)
                                - texture uv0 set (2f)
*/
//TODO split mesh on shapes based on material count and links to polygons

namespace SE {
namespace TOOLS {

ret_code_t ImportNode(FbxNode * pNode, NodeData & oNodeData, ImportCtx & oCtx, ResourceStash & oResStash);
static ret_code_t ImportAnimationClips(fbxsdk::FbxScene * pScene, ResourceStash & oResStash, ImportCtx & oCtx);


struct SkinVertInfo {
        glm::vec4       vWeights{0};
        uint16_t        indices[4]{0};
        uint8_t         cur_joint{0};

        uint8_t AddJoint(const uint16_t joint_id, const float weight, const uint32_t vert_id);
        void    Normalize();
};

uint8_t SkinVertInfo::AddJoint(const uint16_t joint_id, const float weight, const uint32_t vert_id) {

        /** exceed allowed joints cnt per vertex
            drop joint with smallest weight
            later re normalize weights
        */
        if (cur_joint >= 4) {
                float   min_w   = vWeights[0];
                uint8_t min_ind = 0;

                for (uint8_t i = 1; i < 4; ++i) {
                        if (vWeights[i] < min_w) {
                                min_w   = vWeights[i];
                                min_ind = i;
                        }
                }

                if (weight < min_w) {
                        log_w("drop {} joint info (only 4 allowed) for vertex: {}, joint id: {}, weight: {}",
                                        cur_joint + 1,
                                        vert_id,
                                        joint_id,
                                        weight);
                        return ++cur_joint;
                }

                log_w("drop {} joint info (only 4 allowed) for vertex: {}, joint id: {}, weight: {}",
                                cur_joint + 1,
                                vert_id,
                                indices[min_ind],
                                vWeights[min_ind]);

                vWeights[min_ind] = weight;
                indices[min_ind] = joint_id;
        }
        else {
                vWeights[cur_joint] = weight;
                indices[cur_joint] = joint_id;
        }

        return ++cur_joint;
}

void SkinVertInfo::Normalize() {

        if (cur_joint <= 4) { return; }

        float sum_w = vWeights.x + vWeights.y + vWeights.z + vWeights.w;
        vWeights *= 1 / sum_w;
}


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

        oResStash.Clear();

        //TODO import camera settings

        FbxNode * pNode = pScene->GetRootNode();
        if (pNode == nullptr) {
                log_e("empty fbx scene, can't get root node. imported from: '{}'", sPath);
                return uWRONG_INPUT_DATA;
        }

        if (auto res = ImportNode(pNode, oRootNode, oCtx, oResStash); res != uSUCCESS) {
                return res;
        }

        if (oCtx.import_animations) {
                if (auto res = ImportAnimationClips(pScene.get(), oResStash, oCtx); res != uSUCCESS) {
                        return res;
                }
        }

        return uSUCCESS;
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
                                                        (int)pUV->GetReferenceMode(),
                                                        pNode->GetName());
                                        return uWRONG_INPUT_DATA;
                        }
                        break;

                case FbxGeometryElement::eByPolygonVertex:
                        {
                                /** scene was triangulated on load */
                                int32_t polygon_index = polygon_num * 3 /*polygon_size*/ + polygon_vert_ind;
                                switch (pUV->GetReferenceMode()) {
                                        case FbxGeometryElement::eDirect:
                                                oUV = pUV->GetDirectArray().GetAt(polygon_index);
                                                break;
                                        case FbxGeometryElement::eIndexToDirect:
                                                {
                                                        int32_t UVIndex = pUV->GetIndexArray().GetAt(polygon_index);
                                                        oUV = pUV->GetDirectArray().GetAt(UVIndex);
                                                }
                                                break;
                                        default:
                                                log_e("unsupported UV ReferenceMode in polygon {}, (allowed: Direct or IndexToDirect)), current ReferenceMode = {}, Mesh: '{}'",
                                                                polygon_num,
                                                                (int)pUV->GetReferenceMode(),
                                                                pNode->GetName());
                                                return uWRONG_INPUT_DATA;
                                }
                        }
                        break;
                default:
                        log_e("unsupported UV MappingMode = {}, mesh: '{}'", (int)pUV->GetMappingMode(), pNode->GetName() );
                        return uWRONG_INPUT_DATA;
        }
/*
        log_d("polygon: {}, vertex: local index {}, global index {}, tex coord: {}, {}, mapping: {}, reference: {}",
                        polygon_num,
                        polygon_vert_ind,
                        vertex_ind,
                        oUV[0],
                        oUV[1],
                        pUV->GetMappingMode(),
                        pUV->GetReferenceMode()
                        );
*/

        return uSUCCESS;
}

static void VertexFlipYZ(float * data) {

        float old_y = data[1];

        data[1] = - data[2];
        data[2] = old_y;
}

static ret_code_t ImportBlendShapes(
                FbxMesh * pMesh,
                ModelData & oModel,
                const std::unordered_map<uint32_t, std::unordered_set<uint32_t>> & mRemapIndex,
                const uint32_t remaped_vertices_cnt,
                ResourceStash & oResStash,
                const std::string & sPackName) {

        FbxVector4    * pMeshControlPoints = pMesh->GetControlPoints();
        uint32_t        bs_cnt             = pMesh->GetDeformerCount(FbxDeformer::eBlendShape);
        uint32_t        bs_total_cnt       = 0;
        uint32_t        bs_cur             = 0;
        uint32_t        cur_pos;

        std::string sBlendshapesName = sPackName;

        for (uint32_t bs_ind = 0; bs_ind < bs_cnt; ++bs_ind) {

                FbxBlendShape * pBlendShape = (FbxBlendShape*) pMesh->GetDeformer(bs_ind, FbxDeformer::eBlendShape);

                uint32_t bs_channel_cnt = pBlendShape->GetBlendShapeChannelCount();
                for (uint32_t bs_channel_ind = 0; bs_channel_ind < bs_channel_cnt; ++bs_channel_ind) {

                        auto * pBlendShapeChannel = pBlendShape->GetBlendShapeChannel(bs_channel_ind);
                        uint32_t target_shape_cnt = pBlendShapeChannel->GetTargetShapeCount();
                        if (target_shape_cnt != 1) {
                                log_e("BlendShape channel: '{}' got {} target shapes count, support only 1, without in between shapes",
                                                pBlendShapeChannel->GetName(),
                                                target_shape_cnt);
                                return uWRONG_INPUT_DATA;
                        }

                        if (!sBlendshapesName.empty()) {
                                sBlendshapesName += "|";
                        }
                        sBlendshapesName += pBlendShapeChannel->GetName();
                }
                bs_total_cnt += bs_channel_cnt;
        }

        if (bs_total_cnt == 0) { return uSUCCESS; }

        auto created = oResStash.GetResourceData(sBlendshapesName, &oModel.pBlendShape);
        if (!created) {
                return uSUCCESS;
        }
        oModel.pBlendShape->sName = sBlendshapesName;

        size_t total_elements_cnt = bs_total_cnt * remaped_vertices_cnt * 3 /* position */;
        if (total_elements_cnt * sizeof (float) >= (100 * 1024 * 1024)) {
                log_e("do you really want to allocate > 100mb ({} bytes) for {} blend shapes channels, each {} vertices",
                                total_elements_cnt * sizeof (float),
                                bs_total_cnt,
                                remaped_vertices_cnt);
                return uMEMORY_ALLOCATION_ERROR;
        }
        log_d("total blend shape channels: {}, vertices cnt: {}, bs buffer size: {}",
                        bs_total_cnt,
                        remaped_vertices_cnt,
                        total_elements_cnt);
        oModel.pBlendShape->vBuffer.resize(total_elements_cnt);

/**
data layout inside buffer:
|BS1.Vert1.x|BS1.Vert1.y|BS1.Vert1.z|BS2.Vert1.x|BS2.Vert1.y|BS2.Vert1.z|BS1.Vert2.x|BS1.Vert2.y|BS1.Vert2.z|...
*/

        for (uint32_t bs_ind = 0; bs_ind < bs_cnt; ++bs_ind) {

                FbxBlendShape * pBlendShape = (FbxBlendShape*) pMesh->GetDeformer(bs_ind, FbxDeformer::eBlendShape);
                log_d("BlendShape: '{}'", pBlendShape->GetName());

                uint32_t bs_channel_cnt = pBlendShape->GetBlendShapeChannelCount();
                for (uint32_t bs_channel_ind = 0; bs_channel_ind < bs_channel_cnt; ++bs_channel_ind) {

                        auto * pBlendShapeChannel = pBlendShape->GetBlendShapeChannel(bs_channel_ind);
                        log_d("BlendShape channel: '{}', default val: {}",
                                        pBlendShapeChannel->GetName(),
                                        pBlendShapeChannel->DeformPercent.Get());

                        oModel.pBlendShape->vDefaultWeights.emplace_back(pBlendShapeChannel->DeformPercent.Get() / 100.0f);

                        FbxShape*       pShape            = pBlendShapeChannel->GetTargetShape(0);
                        int32_t         vertices_cnt      = pShape->GetControlPointsCount();
                        FbxVector4    * pBSControlPoints  = pShape->GetControlPoints();

                        /*
                        log_d("BlendShape channel: '{}', vert cnt {}, normals cnt {}",
                                        pBlendShapeChannel->GetName(),
                                        vertices_cnt,
                                        pNormals->GetCount());
                        */


                        for (int32_t i = 0 ; i < vertices_cnt; ++i) {


                                auto it = mRemapIndex.find(i);
                                if (it == mRemapIndex.end()) {
                                        log_w("BlendShape channel: '{}' vertex {} unused in vertex index",
                                                        bs_channel_ind,
                                                        i);
                                        continue;
                                }

                                glm::vec3 vPosDiff(pBSControlPoints[i][0] - pMeshControlPoints[i][0],
                                                   pBSControlPoints[i][1] - pMeshControlPoints[i][1],
                                                   pBSControlPoints[i][2] - pMeshControlPoints[i][2]);

                                for (auto new_index : it->second) {

                                        cur_pos = new_index * bs_total_cnt * 3 + bs_cur * 3;
                                        /*
                                        log_d("cur_pos: {}, diff: ({}, {}, {})",
                                                        cur_pos,
                                                        vPosDiff.x,
                                                        vPosDiff.y,
                                                        vPosDiff.z);
                                                        */

                                        /*
                                        log_d("vert {}, new ind {}: pos ({}, {}, {})",
                                                        i,
                                                        new_index,
                                                        pBSControlPoints[i][0],
                                                        pBSControlPoints[i][1],
                                                        pBSControlPoints[i][2]
                                             );
                                        */
                                        memcpy(&oModel.pBlendShape->vBuffer[cur_pos], &vPosDiff.x, sizeof(float) * 3);
                                }
                        }

                        ++bs_cur;
                }
        }

        /*
        //print bs buf:
        uint32_t vert_id = 0;
        for (uint32_t i = 0; i < total_elements_cnt; i += bs_total_cnt * 3) {
                for (uint32_t j = 0; j < bs_total_cnt; ++j) {
                        log_d("blendshapes buf: vert: {}, bs: {}, pos diff: ({}, {}, {}), buf_pos: {}",
                                        vert_id,
                                        j,
                                        oModel.pBlendShapeData->vBuffer[i + 0 + j * 3],
                                        oModel.pBlendShapeData->vBuffer[i + 1 + j * 3],
                                        oModel.pBlendShapeData->vBuffer[i + 2 + j * 3],
                                        i
                             );
                }
                ++vert_id;
        }*/

        return uSUCCESS;
}

static ret_code_t ImportSkeleton(
                ModelData & oModel,
                FbxNode * pJointNode,
                std::unordered_map<std::string, uint16_t> & mJoints,
                ResourceStash & oResStash,
                const std::string & sPackName) {

        FbxNode * pCurNode = pJointNode;
        FbxNode * pRootNode{};
        std::vector<FbxNode *> vJointFbxNodes;

        se_assert(oModel.pSkin);

        while (pCurNode) {

                if (pCurNode->GetNodeAttribute()->GetAttributeType() != FbxNodeAttribute::eSkeleton) {

                        log_e("wrong joint node: '{}', attribute type != FbxNodeAttribute::eSkeleton", pCurNode->GetName());
                        return uWRONG_INPUT_DATA;
                }

                FbxSkeleton * pJoint = (FbxSkeleton*) pCurNode->GetNodeAttribute();
                if (pJoint->IsSkeletonRoot()) {
                        pRootNode = pCurNode;
                        break;
                }
                pCurNode = pCurNode->GetParent();
        }

        if (!pRootNode) {
                log_e("failed to find skeleton root node for joint node: '{}'", pJointNode->GetName());
                return uWRONG_INPUT_DATA;
        }

        mJoints.emplace(pRootNode->GetName(), uint16_t(0));
        vJointFbxNodes.emplace_back(pRootNode);

        std::function<ret_code_t (FbxNode * pCurNode) > ProcessSkeletonNode;
        ProcessSkeletonNode = [pRootNode, &vJointFbxNodes, &mJoints, &ProcessSkeletonNode](FbxNode * pCurNode) {

                auto child_cnt = pCurNode->GetChildCount();
                for (auto i = 0; i < child_cnt; ++i) {
                        FbxNode * pChild = pCurNode->GetChild(i);

                        if (pChild->GetNodeAttribute()->GetAttributeType() != FbxNodeAttribute::eSkeleton) {
                                continue;
                        }

                        log_d("skeleton contain joint node: '{}'", pChild->GetName() );
                        mJoints.emplace(pChild->GetName(), static_cast<uint16_t>(vJointFbxNodes.size()));
                        vJointFbxNodes.emplace_back(pChild);

                        if (auto res = ProcessSkeletonNode(pChild); res != uSUCCESS) {
                                return res;
                        }
                }

                return uSUCCESS;
        };

        ProcessSkeletonNode(pRootNode);

        se_assert(vJointFbxNodes.size() <= 65535);
        log_d("build skeleton with {} joints", vJointFbxNodes.size());

        std::string sRootNodeName;

        if (auto * pParent = vJointFbxNodes[0]->GetParent()) {
                sRootNodeName = pParent->GetName();
        }
        else {
                sRootNodeName = vJointFbxNodes[0]->GetName();
        };

        oModel.pSkin->sRootNode  = sRootNodeName;

        //build skeleton name
        std::string sSkeletonBones;
        for (uint16_t i = 0; i < static_cast<uint16_t>(vJointFbxNodes.size()); ++i) {

                sSkeletonBones += vJointFbxNodes[i]->GetName();
        }
        std::string sSkeletonName = fmt::format("sk:{}:{}|pc:{}",
                        vJointFbxNodes.size(),
                        StrID(sSkeletonBones),
                        sPackName);

        if (!oResStash.GetResourceData(sSkeletonName, &oModel.pSkin->pSkeleton) ) {

                log_d("skeleton: '{}' already created", sSkeletonName);
                return uSUCCESS;
        }

        oModel.pSkin->pSkeleton->sName = sSkeletonName;
        oModel.pSkin->pSkeleton->vJoints.reserve(vJointFbxNodes.size());

        auto ExtractLocalBindSQT = [](FbxNode * pNode, JointData & jd) {
                // Sample at T=0 which is the reference/bind pose for most DCC tools
                FbxAMatrix local   = pNode->EvaluateLocalTransform(FbxTime(0));
                FbxVector4    t    = local.GetT();
                FbxQuaternion r    = local.GetQ();
                FbxVector4    s    = local.GetS();
                jd.oBindLocal.vBindPos   = {(float)t[0], (float)t[1], (float)t[2]};
                jd.oBindLocal.vBindRot   = {(float)r[0], (float)r[1], (float)r[2], (float)r[3]};
                jd.oBindLocal.vBindScale = {(float)s[0], (float)s[1], (float)s[2]};
                jd.bind_inited = true;
        };

        {
                JointData oRoot;
                oRoot.sName         = vJointFbxNodes[0]->GetName();
                oRoot.parent_index  = JointData::ROOT_PARENT_IND;
                ExtractLocalBindSQT(vJointFbxNodes[0], oRoot);
                oModel.pSkin->pSkeleton->vJoints.emplace_back(std::move(oRoot));
        }

        for (uint16_t i = 1; i < static_cast<uint16_t>(vJointFbxNodes.size()); ++i) {

                auto * pParent = vJointFbxNodes[i]->GetParent();
                se_assert(pParent);

                auto itParentInd = mJoints.find(pParent->GetName());
                if (itParentInd == mJoints.end()) {

                        log_e("joint '{}' parent ({}) outside joints hierarchy",
                                        vJointFbxNodes[i]->GetName(),
                                        pParent->GetName());
                        return uWRONG_INPUT_DATA;
                }

                // Topological sort invariant: parent index must be less than child index.
                // DFS traversal from root guarantees this for well-formed skeletons.
                se_assert(itParentInd->second < i);

                JointData oCurJoint;
                oCurJoint.sName         = vJointFbxNodes[i]->GetName();
                oCurJoint.parent_index  = itParentInd->second;
                ExtractLocalBindSQT(vJointFbxNodes[i], oCurJoint);

                oModel.pSkin->pSkeleton->vJoints.emplace_back(std::move(oCurJoint));
        }

        return uSUCCESS;
}

static ret_code_t ImportSkin(
                FbxNode * pNode,
                FbxMesh * pMesh,
                ModelData & oModel,
                const std::unordered_map<uint32_t, std::unordered_set<uint32_t>> & mRemapIndex,
                const uint32_t remaped_vertices_cnt,
                ResourceStash & oResStash,
                const std::string & sPackName) {

        uint32_t skin_cnt               = pMesh->GetDeformerCount(FbxDeformer::eSkin);
        int32_t  input_vertices_cnt     = pMesh->GetControlPointsCount();

        if (!skin_cnt) { return uSUCCESS; }
        else if (skin_cnt > 1) {
                log_w("import only first skin deformer per mesh");
        }

        std::string sSkinName = fmt::format("skd:mesh:{}", oModel.pMesh->sName);
        if (oResStash.GetResourceData(sSkinName, &oModel.pSkin) ) {

                oModel.pSkin->sName = sSkinName;
        }
        else {
                log_d("skin: '{}' already created", sSkinName);
                return uSUCCESS;
        }

        auto pSkinDeformer = (FbxSkin *)pMesh->GetDeformer(0, FbxDeformer::eSkin);
        auto skinning_type = pSkinDeformer->GetSkinningType();

        if ((skinning_type != FbxSkin::eLinear) && (skinning_type != FbxSkin::eRigid) ) {
                log_e("unsupportes skinning type: '{}', only linear supported", (int)pSkinDeformer->GetSkinningType());
                return uWRONG_INPUT_DATA;
        }

        uint8_t  joints_per_vertex{0};
        uint8_t  cur_joints_per_vertes{0};
        uint32_t cluster_cnt       = ((FbxSkin *)pMesh->GetDeformer(0, FbxDeformer::eSkin))->GetClusterCount();
        uint32_t cur_pos;
        std::unordered_map<std::string, uint16_t> mJoints;

        if (!cluster_cnt) {
                log_w("zero clusters");
                return uSUCCESS;
        }

        auto * pInitialJointNode =  ((FbxSkin *)pMesh->GetDeformer(0, FbxDeformer::eSkin))->GetCluster(0)->GetLink();
        if (auto res = ImportSkeleton(oModel, pInitialJointNode, mJoints, oResStash, sPackName); res != uSUCCESS) {
                return res;
        }

        std::vector<SkinVertInfo> vSkinInfo(input_vertices_cnt);
        FbxCluster * pCluster;

        log_d("node: '{}', try to import {} clusters", oModel.pMesh->sName, cluster_cnt);

        auto GetGeometry = [](FbxNode* pNode) {//TODO calc once

                const FbxVector4 lT = pNode->GetGeometricTranslation(FbxNode::eSourcePivot);
                const FbxVector4 lR = pNode->GetGeometricRotation(FbxNode::eSourcePivot);
                const FbxVector4 lS = pNode->GetGeometricScaling(FbxNode::eSourcePivot);

                //
                log_d("get from node: '{}', pos: ({}, {}, {}, {}), rot: ({}, {}, {}), scale: ({}, {}, {})",
                                pNode->GetName(),
                                lT[0],
                                lT[1],
                                lT[2],
                                lT[3],
                                lR[0],
                                lR[1],
                                lR[2],
                                lS[0],
                                lS[1],
                                lS[2]
                                );
                                //

                return FbxAMatrix(lT, lR, lS);
        };

        //get mesh node bind pose transform matrix
        {
                pCluster = pSkinDeformer->GetCluster(0);

                FbxAMatrix pTransformMatrix;
                FbxAMatrix pReferenceGeomMatrix;

                pCluster->GetTransformMatrix(pTransformMatrix);
                pReferenceGeomMatrix = GetGeometry(pNode);
                pTransformMatrix    *= pReferenceGeomMatrix;

                FbxVector4 vBindPos     = pTransformMatrix.GetT();
                FbxVector4 vBindScale   = pTransformMatrix.GetS();
                FbxQuaternion qRot      = pTransformMatrix.GetQ();

                oModel.pSkin->oMeshBindPose.vBindPos   = glm::vec3(vBindPos[0], vBindPos[1], vBindPos[2]);
                oModel.pSkin->oMeshBindPose.vBindScale = glm::vec3(vBindScale[0], vBindScale[1], vBindScale[2]);
                oModel.pSkin->oMeshBindPose.vBindRot   = glm::vec4(qRot[0], qRot[1], qRot[2], qRot[3]);

                /*
                log_d("orig node: '{}' mesh bind pose global transform pos: ({}, {}, {}), scale: ({}, {}, {})",
                                pNode->GetName(),
                                pTransformMatrix.GetT()[0],
                                pTransformMatrix.GetT()[1],
                                pTransformMatrix.GetT()[2],
                                pTransformMatrix.GetS()[0],
                                pTransformMatrix.GetS()[1],
                                pTransformMatrix.GetS()[2]
                                );
                */

        }


        //find max joints per vertex
        for (uint32_t i = 0; i < cluster_cnt; ++i) {

                pCluster = pSkinDeformer->GetCluster(i);
                if (pCluster->GetLinkMode() != FbxCluster::ELinkMode::eNormalize) {
                        log_e("unsupported cluster link mode: '{}'", (int)pCluster->GetLinkMode());
                        return uWRONG_INPUT_DATA;
                }


                uint32_t indices_cnt    = pCluster->GetControlPointIndicesCount();
                int32_t * pIndices      = pCluster->GetControlPointIndices();
                double  * pWeights      = pCluster->GetControlPointWeights();

                log_d("cluster({}) link node: '{}', vert indices cnt: {}",
                                i,
                                pCluster->GetLink()->GetName(),
                                indices_cnt);

                se_assert(i <= 65535);
                for (uint32_t j = 0; j < indices_cnt; ++j) {
                        se_assert(static_cast<uint32_t>(pIndices[j]) < vSkinInfo.size());
                        cur_joints_per_vertes = vSkinInfo[pIndices[j]].AddJoint(static_cast<uint16_t>(i), pWeights[j], pIndices[j]);
                        if (cur_joints_per_vertes > joints_per_vertex) {
                                joints_per_vertex = cur_joints_per_vertes;
                        }
                }

                auto itJointInd = mJoints.find(pCluster->GetLink()->GetName());
                if (itJointInd == mJoints.end()) {

                        log_e("joint '{}' not found in skeleton: '{}'",
                                        pCluster->GetLink()->GetName(),
                                        oModel.pSkin->pSkeleton->sName);
                        return uWRONG_INPUT_DATA;
                }



                if (pCluster->GetLink()->GetNodeAttribute()->GetAttributeType() != FbxNodeAttribute::eSkeleton) {

                        log_e("wrong joint node: '{}', attribute type != FbxNodeAttribute::eSkeleton",
                                        pCluster->GetLink()->GetName());
                        return uWRONG_INPUT_DATA;
                }

                /**
                need to store joint inverse bind pose for each model and could not store once inside skeleton,
                because different meshes could be skinned in different bind poses.
                rare case
                */
                BindPoseData    oInvBindPose;
                FbxAMatrix      pLinkTransformMatrix;
                pCluster->GetTransformLinkMatrix(pLinkTransformMatrix);

                /*
                log_d("link node: '{}' joint bind pos: ({}, {}, {}), scale: ({}, {}, {})",
                                pCluster->GetLink()->GetName(),
                                pLinkTransformMatrix.GetT()[0],
                                pLinkTransformMatrix.GetT()[1],
                                pLinkTransformMatrix.GetT()[2],
                                pLinkTransformMatrix.GetS()[0],
                                pLinkTransformMatrix.GetS()[1],
                                pLinkTransformMatrix.GetS()[2]
                     );
                */

                pLinkTransformMatrix = pLinkTransformMatrix.Inverse();

                FbxVector4 vBindPos     = pLinkTransformMatrix.GetT();
                FbxVector4 vBindScale   = pLinkTransformMatrix.GetS();
                FbxQuaternion qRot      = pLinkTransformMatrix.GetQ();

                oInvBindPose.vBindPos   = glm::vec3(vBindPos[0], vBindPos[1], vBindPos[2]);
                oInvBindPose.vBindScale = glm::vec3(vBindScale[0], vBindScale[1], vBindScale[2]);
                oInvBindPose.vBindRot   = glm::vec4(qRot[0], qRot[1], qRot[2], qRot[3]);

                // Mirror inv bind SQT into the skeleton for WriteSkeleton.
                // For the common case (single bind pose per skeleton) this is correct.
                // If multiple meshes share a skeleton with different bind poses (rare)
                // the last mesh to call ImportSkin wins; a separate .sesk per mesh
                // would be needed to handle that case correctly.
                {
                        auto & jd        = oModel.pSkin->pSkeleton->vJoints[itJointInd->second];
                        jd.oInvBind      = oInvBindPose;
                }

                //log_d("current node name: {}", pCluster->GetLink()->GetName());

                oModel.pSkin->vJointIndexes.emplace_back(itJointInd->second);
                oModel.pSkin->vJointsInvBindPose.emplace_back(std::move(oInvBindPose));
        }

        if (joints_per_vertex == 0) {
                log_e("empty cluster, no one vertex weight found");
                return uWRONG_INPUT_DATA;
        }
        else if (joints_per_vertex > 4) {

                log_w("some joint weights were dropped, too much clusters per vertex: {}, max 4 allowed",
                                joints_per_vertex);
                joints_per_vertex = 4;

                /** we need to renormalize only if some joints were dropped */
                for (auto & item : vSkinInfo) {
                        item.Normalize();
                }
        }

        /* DEBUG
        for (uint32_t i = 0; i < vSkinInfo.size(); ++i) {

                log_d("vert: {} weights: ({:<10}, {:<10}, {:<10}, {:<10})",
                                i,
                                vSkinInfo[i].vWeights.x,
                                vSkinInfo[i].vWeights.y,
                                vSkinInfo[i].vWeights.z,
                                vSkinInfo[i].vWeights.w);
        }*/

        //remap vertices, compress and write into output buffer
        std::vector<float>    vWeightsBuffer(remaped_vertices_cnt * joints_per_vertex);
        std::vector<uint32_t> vIndicesBuffer(remaped_vertices_cnt * joints_per_vertex);

        for (uint32_t i = 0; i < vSkinInfo.size(); ++i) {

                auto it = mRemapIndex.find(i);
                if (it == mRemapIndex.end()) {
                        log_w("vertex {} unused in skinning vertex index", i);
                        continue;
                }

                for (auto new_index : it->second) {

                        cur_pos = new_index * joints_per_vertex;
                        memcpy(&vWeightsBuffer[cur_pos], &vSkinInfo[i].vWeights[0], sizeof(float) * joints_per_vertex);
                        for (uint8_t j = 0; j < joints_per_vertex; ++j) {
                                vIndicesBuffer[cur_pos + j] = vSkinInfo[i].indices[j];
                        }
                }
        }

        log_d("skeleton: '{}', root node: '{}', cluster cnt: {}, joints_per_vertex: {}",
                        oModel.pSkin->pSkeleton->sName,
                        oModel.pSkin->sRootNode,
                        cluster_cnt,
                        joints_per_vertex);

        uint8_t stride = joints_per_vertex * sizeof(float);
        uint8_t weights_buffer_ind = oModel.pMesh->vVertexBuffers.size();
        oModel.pMesh->vVertexBuffers.emplace_back(MeshData::VertexBuffer{std::move(vWeightsBuffer), stride});
        //TODO write indices and weights in one buffer
        stride = joints_per_vertex * sizeof(uint32_t);
        uint8_t indices_buffer_ind = oModel.pMesh->vVertexBuffers.size();
        oModel.pMesh->vVertexBuffers.emplace_back(MeshData::VertexBuffer{std::move(vIndicesBuffer), stride});

        oModel.pMesh->vAttributes.emplace_back(MeshData::VertexAttribute{
                        "JointWeights",
                        0,
                        joints_per_vertex,
                        weights_buffer_ind });

        oModel.pMesh->vAttributes.emplace_back(MeshData::VertexAttribute{
                        "JointIndices",
                        0,
                        joints_per_vertex,
                        indices_buffer_ind,
                        joints_per_vertex,
                        MeshData::VertexAttribute::Type::DEST_INT});


        return uSUCCESS;
}

static ret_code_t ImportMaterial(FbxNode * pNode, int mat_idx, MaterialData ** pMaterialData, ImportCtx & oCtx, ResourceStash & oResStash) {

        FbxSurfaceMaterial * pMaterial = pNode->GetSrcObject<FbxSurfaceMaterial>(mat_idx);
        if (!pMaterial) {
                return uSUCCESS;
        }

        // Returns the file path for the first texture attached to a material property,
        // or empty string if none found.
        auto GetTexturePath = [&](FbxSurfaceMaterial * pMat, const char * propName) -> std::string {
                auto prop = pMat->FindProperty(propName);
                if (!prop.IsValid() || prop.GetSrcObjectCount<FbxTexture>() == 0)
                        return {};

                FbxTexture * pTex = nullptr;
                if (auto * pLayered = prop.GetSrcObject<FbxLayeredTexture>(0)) {
                        if (pLayered->GetSrcObjectCount<FbxTexture>() > 0)
                                pTex = pLayered->GetSrcObject<FbxTexture>(0);
                }
                if (!pTex)
                        pTex = prop.GetSrcObject<FbxTexture>(0);

                if (auto * pFileTex = FbxCast<FbxFileTexture>(pTex)) {
                        std::string sPath = pFileTex->GetFileName();
                        oCtx.FixPath(sPath);
                        return sPath;
                }
                return {};
        };

        std::string sDiffuse  = GetTexturePath(pMaterial, FbxSurfaceMaterial::sDiffuse);
        std::string sNormal   = GetTexturePath(pMaterial, FbxSurfaceMaterial::sNormalMap);
        if (sNormal.empty())
                sNormal       = GetTexturePath(pMaterial, FbxSurfaceMaterial::sBump);
        std::string sSpecular = GetTexturePath(pMaterial, FbxSurfaceMaterial::sSpecular);
        std::string sEmissive = GetTexturePath(pMaterial, FbxSurfaceMaterial::sEmissive);

        if (sDiffuse.empty() && sNormal.empty() && sSpecular.empty() && sEmissive.empty()) {
                return uSUCCESS;
        }

        const bool bPBR = !sNormal.empty() || !sSpecular.empty();
        const std::string sShader = bPBR
                ? "shader_program/pbr_geometry.sesp"
                : "shader_program/simple_tex.sesp";

        std::string sMaterialName = pMaterial->GetName();
        if (sMaterialName.empty())
                sMaterialName = oCtx.sPackName + std::to_string(StrID(sDiffuse + sNormal + sShader));
        else
                sMaterialName = oCtx.sPackName + sMaterialName;

        log_d("material '{}' shader '{}' diffuse '{}' normal '{}' specular '{}' emissive '{}'",
                        sMaterialName, sShader, sDiffuse, sNormal, sSpecular, sEmissive);

        bool created = oResStash.GetResourceData(StrID(sMaterialName), pMaterialData);
        if (created) {
                (*pMaterialData)->sName       = sMaterialName;
                (*pMaterialData)->sShaderPath = sShader;

                if (bPBR) {
                        (*pMaterialData)->mVariables["BaseColor"] = glm::vec4(1.f, 1.f, 1.f, 1.f);
                        (*pMaterialData)->mVariables["Roughness"] = 0.5f;
                        (*pMaterialData)->mVariables["Metallic"]  = 0.0f;
                        (*pMaterialData)->mVariables["Emissive"]  = glm::vec3(0.f, 0.f, 0.f);
                        (*pMaterialData)->mVariables["AO"]        = 1.0f;
                }

                auto AddTex = [&](const std::string & sPath, TextureUnit unit) {
                        if (sPath.empty()) return;
                        (*pMaterialData)->mTextures.emplace(unit, TextureData{sPath});
                        ++oCtx.textures_cnt;
                };

                AddTex(sDiffuse,  TextureUnit::DIFFUSE);
                AddTex(sNormal,   TextureUnit::NORMAL);
                AddTex(sSpecular, TextureUnit::SPECULAR);
                AddTex(sEmissive, TextureUnit::EMISSIVE);

                // inline_all: read original compressed bytes; clear sPath so writer
                // uses vEncodedData instead of a path reference
                if (oCtx.inline_all) {
                        for (auto & [unit, oTex] : (*pMaterialData)->mTextures) {
                                if (oTex.sPath.empty() || !oTex.vEncodedData.empty()) continue;
                                std::ifstream f(oTex.sPath, std::ios::binary);
                                if (!f) {
                                        log_w("inline_all: cannot open texture '{}'", oTex.sPath);
                                        continue;
                                }
                                oTex.vEncodedData.assign(
                                        std::istreambuf_iterator<char>(f),
                                        std::istreambuf_iterator<char>());

                                // Detect encoding from file extension
                                auto pos = oTex.sPath.rfind('.');
                                std::string ext = (pos != std::string::npos) ? oTex.sPath.substr(pos + 1) : "";
                                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                                if      (ext == "png")              oTex.oEncoding = TextureEncoding::PNG;
                                else if (ext == "jpg" || ext == "jpeg") oTex.oEncoding = TextureEncoding::JPG;
                                else if (ext == "tga")              oTex.oEncoding = TextureEncoding::TGA;
                                else if (ext == "dds")              oTex.oEncoding = TextureEncoding::DDS;
                                else if (ext == "ktx2")             oTex.oEncoding = TextureEncoding::KTX2;
                                else                                oTex.oEncoding = TextureEncoding::RGBA8;

                                oTex.sName = oTex.sPath; // preserve for debug; writer uses vEncodedData
                                oTex.sPath.clear();      // signal writer to use inline encoded path
                        }
                }

                ++oCtx.material_cnt;
        }

        return uSUCCESS;
}

static ret_code_t ImportMesh(
                FbxNode * pNode,
                MeshData ** pMeshData,
                ImportCtx & oCtx,
                ResourceStash & oResStash,
                std::unordered_map<uint32_t, std::unordered_set<uint32_t>> & mRemapIndex,
                uint32_t & remaped_vertices_cnt) {

        VertexIndex             oVertexIndex;
        FbxMesh               * pMesh           = (FbxMesh *) pNode->GetNodeAttribute ();
        int32_t                 vertices_cnt    = pMesh->GetControlPointsCount();
        FbxVector4            * pControlPoints  = pMesh->GetControlPoints();
        std::vector<float>      vVertexData;
        uint32_t                cur_index = 0;
        TPackVertexIndex        Pack;
        uint32_t                uv_sets_cnt = pMesh->GetElementUVCount();

        if ( uv_sets_cnt == 0) {
                log_e("failed to get UV coordinates for mesh '{}'", pNode->GetName());
                return uWRONG_INPUT_DATA;
        }
        else if (uv_sets_cnt > 4) {
                log_e("too many UV sets ({}), up to 4 supported, mesh node: '{}'", uv_sets_cnt, pNode->GetName());
                return uWRONG_INPUT_DATA;
        }

        std::vector<float> vVertices;

        uint8_t elements_cnt         = (oCtx.skip_normals) ? VERTEX_BASE_SIZE : VERTEX_SIZE;
        if (uv_sets_cnt > 1) {
                elements_cnt += (uv_sets_cnt - 1) * 2;
        }
        uint8_t stride               = elements_cnt * sizeof(float);
        std::string sMeshName        = pMesh->GetName();
        vVertexData.reserve(elements_cnt);

        if (sMeshName.empty()) {
                sMeshName = oCtx.sPackName + std::to_string(reinterpret_cast<std::uintptr_t>(pMesh));
        }
        else {
                sMeshName = oCtx.sPackName + sMeshName;
        }

        bool mesh_created = oResStash.GetResourceData(sMeshName, pMeshData);
        auto * pModelMesh = *pMeshData;
        if (mesh_created) {
                pModelMesh->sName = sMeshName;
        }
        else {
                log_d("skip vert processing for mesh: '{}' that already exist, node: '{}'",
                                pModelMesh->sName,
                                pNode->GetName() );
                return uSUCCESS;
        }

        int32_t  polygon_cnt            = pMesh->GetPolygonCount();
        int32_t  polygon_size;
        log_d("polygons: cnt = {}", polygon_cnt);

        uint32_t index_size = static_cast<uint32_t>(polygon_cnt) * 3;

        Pack = PackVertexIndexInit(index_size, pModelMesh->oIndex);

        // --- Per-polygon material index ---
        FbxLayerElementMaterial * pMatLayer = nullptr;
        if (pMesh->GetLayerCount() > 0)
                pMatLayer = pMesh->GetLayer(0)->GetMaterials();

        const bool bByPolygon = pMatLayer &&
                pMatLayer->GetMappingMode() == FbxLayerElement::eByPolygon;

        std::vector<int> vPolyMat(polygon_cnt, 0);
        if (bByPolygon) {
                const auto & rIdx = pMatLayer->GetIndexArray();
                for (int32_t p = 0; p < polygon_cnt; ++p)
                        vPolyMat[p] = rIdx.GetAt(p);
        }

        // Per-material deduplicated index lists (uint32 staging; converted via Pack at end)
        std::map<int, std::vector<uint32_t>> mMatIndices;

        for (int32_t polygon_num = 0; polygon_num < polygon_cnt; ++polygon_num) {

                polygon_size = pMesh->GetPolygonSize(polygon_num);

                if (polygon_size != 3) {
                        log_e("wrong polygon size == {}, must be triangle! (scene was triangulate previously)", polygon_size);
                        return uWRONG_INPUT_DATA;
                }

                auto & vGroupIdx = mMatIndices[vPolyMat[polygon_num]];

                for (int32_t polygon_vert_ind = 0; polygon_vert_ind < polygon_size; ++polygon_vert_ind) {

                        int32_t vertex_ind = pMesh->GetPolygonVertex(polygon_num, polygon_vert_ind);

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
                                }

                                vVertexData.push_back(oNormal[0]);
                                vVertexData.push_back(oNormal[1]);
                                vVertexData.push_back(oNormal[2]);
                        }

                        FbxVector2 oUV;
                        for (uint32_t cur_uv_set = 0; cur_uv_set < uv_sets_cnt; ++cur_uv_set) {
                                ret_code_t result = GetUV(pMesh->GetElementUV(cur_uv_set), pNode, pMesh, polygon_num, polygon_vert_ind, vertex_ind, oUV);
                                if (result != uSUCCESS) {
                                        log_e("failed to get uv");
                                        return result;
                                }

                                vVertexData.push_back(oUV[0]);
                                vVertexData.push_back(oUV[1]);
                        }

                        if (oVertexIndex.Get(vVertexData, cur_index)) {

                                if (oCtx.flip_yz) {
                                        VertexFlipYZ(&vVertexData[0]);
                                        if (!oCtx.skip_normals) {
                                                VertexFlipYZ(&vVertexData[3]);
                                        }
                                }

                                vVertices.insert(vVertices.end(), vVertexData.begin(), vVertexData.end());
                                ++oCtx.total_vertices_cnt;
                                mRemapIndex[vertex_ind].emplace(cur_index);
                        }

                        vGroupIdx.push_back(cur_index);
                }
        }

        log_d("input vertex cnt: {}, output vertex cnt: {}, output estimated index cnt: {}, vertex elements cnt: {}, uv sets cnt: {}, material groups: {}",
                        vertices_cnt,
                        oVertexIndex.Size(),
                        index_size,
                        elements_cnt, uv_sets_cnt,
                        mMatIndices.size());

        oCtx.total_triangles_cnt += polygon_cnt;
        ++oCtx.mesh_cnt;
        oCtx.shape_cnt += static_cast<uint32_t>(mMatIndices.size());
        remaped_vertices_cnt = oVertexIndex.Size();

        // --- Build per-material shapes and concatenate index buffer ---
        for (auto & [mat_idx, vGroupIdx] : mMatIndices) {

                BoundingBox oGroupBBox;
                for (uint32_t vi : vGroupIdx) {
                        oGroupBBox.Concat(glm::vec3(
                                vVertices[vi * elements_cnt],
                                vVertices[vi * elements_cnt + 1],
                                vVertices[vi * elements_cnt + 2]));
                }

                uint32_t shape_start = std::visit([](auto & v) -> uint32_t { return v.size(); }, pModelMesh->oIndex);
                for (uint32_t vi : vGroupIdx) {
                        Pack(pModelMesh->oIndex, vi);
                }
                uint32_t shape_count = static_cast<uint32_t>(vGroupIdx.size());

                pModelMesh->vShapes.emplace_back(shape_start, shape_count, oGroupBBox,
                                                 static_cast<uint16_t>(mat_idx));
        }

        uint8_t buffer_ind = pModelMesh->vVertexBuffers.size();
        pModelMesh->vVertexBuffers.emplace_back(MeshData::VertexBuffer{std::move(vVertices), stride});

        uint16_t next_offset = 3;

        pModelMesh->vAttributes.emplace_back(MeshData::VertexAttribute{
                        "Position",
                        0,
                        3,
                        buffer_ind });
        if (!oCtx.skip_normals) {
                pModelMesh->vAttributes.emplace_back(MeshData::VertexAttribute{
                                "Normal",
                                static_cast<uint16_t>(3 * sizeof(float)),
                                3,
                                buffer_ind });
                next_offset = 6;
        }
        pModelMesh->vAttributes.emplace_back(MeshData::VertexAttribute{
                        "TexCoord0",
                        static_cast<uint16_t>(next_offset * sizeof(float)),
                        2,
                        buffer_ind,
                        uv_sets_cnt
                        });

        for (uint32_t cur_uv_set = 1; cur_uv_set < uv_sets_cnt; ++cur_uv_set) {
                next_offset += 2;

                static std::string sAttrName;

                sAttrName = fmt::format("TexCoord{}", cur_uv_set);

                pModelMesh->vAttributes.emplace_back(MeshData::VertexAttribute{
                                sAttrName,
                                static_cast<uint16_t>(next_offset * sizeof(float)),
                                2,
                                buffer_ind
                                });

        }

        // Combine all per-material shape bboxes into the mesh bbox
        for (auto & oShape : pModelMesh->vShapes) {
                pModelMesh->oBBox.Concat(oShape.oBBox);
        }

        return uSUCCESS;
}

static ret_code_t ImportAnimationClips(
                FbxScene      * pScene,
                ResourceStash & oResStash,
                ImportCtx     & oCtx) {

        int32_t stackCount = pScene->GetSrcObjectCount<FbxAnimStack>();
        if (stackCount == 0) {
                log_d("no animation stacks in scene");
                return uSUCCESS;
        }

        // Find first skeleton in stash to get joint names/count
        std::vector<std::string> vJointNames;
        for (auto & [name, pData] : oResStash.mResources) {
                if (auto * pSkel = std::get_if<Skeleton>(pData.get())) {
                        vJointNames.reserve(pSkel->vJoints.size());
                        for (auto & joint : pSkel->vJoints) {
                                vJointNames.push_back(joint.sName);
                        }
                        log_d("ImportAnimationClips: using skeleton '{}' with {} joints",
                                pSkel->sName, vJointNames.size());
                        break;
                }
        }

        if (vJointNames.empty()) {
                log_d("ImportAnimationClips: no skeleton found, skipping animation export");
                return uSUCCESS;
        }

        // Resolve FbxNode* for each joint by name
        std::vector<FbxNode *> vJointNodes;
        vJointNodes.reserve(vJointNames.size());
        for (auto & sName : vJointNames) {
                vJointNodes.push_back(pScene->FindNodeByName(sName.c_str()));
        }

        const float sampleRate = oCtx.anim_sample_rate;

        for (int32_t si = 0; si < stackCount; ++si) {

                FbxAnimStack * pStack = pScene->GetSrcObject<FbxAnimStack>(si);
                pScene->SetCurrentAnimationStack(pStack);

                FbxTimeSpan ts       = pStack->GetLocalTimeSpan();
                double      startSec = ts.GetStart().GetSecondDouble();
                double      stopSec  = ts.GetStop().GetSecondDouble();
                float       duration = static_cast<float>(stopSec - startSec);

                if (duration <= 0.0f) {
                        log_w("animation stack '{}' has zero/negative duration, skipped", pStack->GetName());
                        continue;
                }

                int32_t sampleCount = std::max(2, static_cast<int32_t>(duration * sampleRate) + 1);
                log_d("exporting animation stack '{}', duration: {}s, samples: {}", pStack->GetName(), duration, sampleCount);

                AnimClipData oClip;
                oClip.sName    = pStack->GetName();
                oClip.duration = duration;
                {
                        auto it = oCtx.mClipSettings.find(oClip.sName);
                        oClip.looping = (it != oCtx.mClipSettings.end()) ? it->second.loop : false;
                }

                // Read "events" custom property from the stack or its local time-node.
                // Format: "time:name[:value];time:name[:value];..."
                // e.g.  "0.40:footstep.left;0.95:footstep.right:1.0"
                auto ParseEventsProperty = [&](FbxObject * pObj) {
                        FbxProperty prop = pObj->FindProperty("events");
                        if (!prop.IsValid()) { return; }
                        if (prop.GetPropertyDataType().GetType() != eFbxString) { return; }
                        std::string sEvents = prop.Get<FbxString>().Buffer();
                        // Tokenise by ';'
                        size_t pos = 0;
                        while (pos < sEvents.size()) {
                                size_t end = sEvents.find(';', pos);
                                std::string token = sEvents.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
                                pos = (end == std::string::npos) ? sEvents.size() : end + 1;
                                if (token.empty()) { continue; }
                                // Parse "time:name[:value]"
                                size_t p1 = token.find(':');
                                if (p1 == std::string::npos) { continue; }
                                std::string sTime  = token.substr(0, p1);
                                std::string sRest  = token.substr(p1 + 1);
                                size_t p2 = sRest.find(':');
                                std::string sName  = (p2 == std::string::npos) ? sRest : sRest.substr(0, p2);
                                float       fVal   = (p2 == std::string::npos) ? 0.0f : std::stof(sRest.substr(p2 + 1));
                                try {
                                        AnimEventData ev;
                                        ev.time  = std::stof(sTime);
                                        ev.sName = sName;
                                        ev.value = fVal;
                                        oClip.vEvents.push_back(std::move(ev));
                                }
                                catch (...) {
                                        log_w("animation stack '{}': failed to parse event token: '{}'",
                                                oClip.sName, token);
                                }
                        }
                        if (!oClip.vEvents.empty()) {
                                std::sort(oClip.vEvents.begin(), oClip.vEvents.end(),
                                                [](const AnimEventData & a, const AnimEventData & b){ return a.time < b.time; });
                                log_d("stack '{}': {} animation event(s) found", oClip.sName, oClip.vEvents.size());
                        }
                };
                ParseEventsProperty(pStack);
                // Fallback: check "events" on the first joint node (root bone)
                if (oClip.vEvents.empty() && !vJointNodes.empty() && vJointNodes[0]) {
                        ParseEventsProperty(vJointNodes[0]);
                }

                for (uint16_t ji = 0; ji < static_cast<uint16_t>(vJointNodes.size()); ++ji) {

                        FbxNode * pJointNode = vJointNodes[ji];
                        if (!pJointNode) {
                                log_w("joint '{}' not found in scene, skipping channel", vJointNames[ji]);
                                continue;
                        }

                        AnimCurveChannel txCh, tyCh, tzCh;
                        AnimCurveChannel qxCh, qyCh, qzCh, qwCh;
                        AnimCurveChannel sxCh, syCh, szCh;

                        txCh.bone_index = tyCh.bone_index = tzCh.bone_index = ji;
                        qxCh.bone_index = qyCh.bone_index = qzCh.bone_index = qwCh.bone_index = ji;
                        sxCh.bone_index = syCh.bone_index = szCh.bone_index = ji;

                        txCh.target = 0; tyCh.target = 1; tzCh.target = 2;
                        qxCh.target = 3; qyCh.target = 4; qzCh.target = 5; qwCh.target = 6;
                        sxCh.target = 7; syCh.target = 8; szCh.target = 9;

                        txCh.vTimes.reserve(sampleCount); tyCh.vTimes.reserve(sampleCount); tzCh.vTimes.reserve(sampleCount);
                        qxCh.vTimes.reserve(sampleCount); qyCh.vTimes.reserve(sampleCount); qzCh.vTimes.reserve(sampleCount); qwCh.vTimes.reserve(sampleCount);
                        sxCh.vTimes.reserve(sampleCount); syCh.vTimes.reserve(sampleCount); szCh.vTimes.reserve(sampleCount);

                        for (int32_t s = 0; s < sampleCount; ++s) {

                                float     t = (sampleCount > 1) ? (float(s) / float(sampleCount - 1) * duration) : 0.0f;
                                FbxTime   fbxTime;
                                fbxTime.SetSecondDouble(startSec + t);

                                FbxAMatrix    localMat = pJointNode->EvaluateLocalTransform(fbxTime);
                                FbxVector4    pos      = localMat.GetT();
                                FbxQuaternion q        = localMat.GetQ();
                                FbxVector4    scale    = localMat.GetS();

                                txCh.vTimes.push_back(t); txCh.vValues.push_back(static_cast<float>(pos[0]));
                                tyCh.vTimes.push_back(t); tyCh.vValues.push_back(static_cast<float>(pos[1]));
                                tzCh.vTimes.push_back(t); tzCh.vValues.push_back(static_cast<float>(pos[2]));

                                qxCh.vTimes.push_back(t); qxCh.vValues.push_back(static_cast<float>(q[0]));
                                qyCh.vTimes.push_back(t); qyCh.vValues.push_back(static_cast<float>(q[1]));
                                qzCh.vTimes.push_back(t); qzCh.vValues.push_back(static_cast<float>(q[2]));
                                qwCh.vTimes.push_back(t); qwCh.vValues.push_back(static_cast<float>(q[3]));

                                sxCh.vTimes.push_back(t); sxCh.vValues.push_back(static_cast<float>(scale[0]));
                                syCh.vTimes.push_back(t); syCh.vValues.push_back(static_cast<float>(scale[1]));
                                szCh.vTimes.push_back(t); szCh.vValues.push_back(static_cast<float>(scale[2]));
                        }

                        oClip.vChannels.push_back(std::move(txCh));
                        oClip.vChannels.push_back(std::move(tyCh));
                        oClip.vChannels.push_back(std::move(tzCh));
                        oClip.vChannels.push_back(std::move(qxCh));
                        oClip.vChannels.push_back(std::move(qyCh));
                        oClip.vChannels.push_back(std::move(qzCh));
                        oClip.vChannels.push_back(std::move(qwCh));
                        oClip.vChannels.push_back(std::move(sxCh));
                        oClip.vChannels.push_back(std::move(syCh));
                        oClip.vChannels.push_back(std::move(szCh));
                }

                // FBX EvaluateLocalTransform flips quaternion hemisphere whenever its
                // internal euler wrap crosses ±180° — per-component runtime
                // interpolation would sweep limbs the long way round. Fix at the source.
                EnforceQuatContinuity(oClip.vChannels);

                oCtx.vAnimClips.emplace_back(std::move(oClip));
        }

        return uSUCCESS;
}

//TODO import sub meshes by materials
static ret_code_t ImportAttributes(FbxNode * pNode, NodeData & oNodeData, ImportCtx & oCtx, ResourceStash & oResStash) {

        FbxNodeAttribute::EType attribute_type;
        ret_code_t              res;

        auto * pAttribute = pNode->GetNodeAttribute();
        if (pAttribute == nullptr) {
                log_d("node '{}' does't contain attribute", pNode->GetName());
                return uSUCCESS;
        }

        attribute_type = pAttribute->GetAttributeType();

        if (attribute_type == FbxNodeAttribute::eMesh) {

                log_d("mesh node: '{}', name: '{}'", pNode->GetName(), pAttribute->GetName());

                ModelData       oModel;
                /**
                  need to store intermediate mapping between input and output vertices
                  for propper deformer per vertex data conversion
                 */
                std::unordered_map<uint32_t, std::unordered_set<uint32_t>> mRemapIndex;
                uint32_t        remaped_vertices_cnt;
                FbxMesh       * pMesh = (FbxMesh *) pNode->GetNodeAttribute();

                if (res = ImportMesh(pNode, &oModel.pMesh, oCtx, oResStash, mRemapIndex, remaped_vertices_cnt); res != uSUCCESS) {
                        return res;
                }

                if (!oCtx.skip_material) {
                        int mat_cnt = pNode->GetSrcObjectCount<FbxSurfaceMaterial>();
                        for (int mi = 0; mi < mat_cnt; ++mi) {
                                MaterialData * pMat = nullptr;
                                if (res = ImportMaterial(pNode, mi, &pMat, oCtx, oResStash); res != uSUCCESS) {
                                        return res;
                                }
                                oModel.vMaterials.push_back(pMat);
                        }
                }

                //import vertex deformers
                if (oCtx.import_blend_shapes) {
                        res = ImportBlendShapes(
                                        pMesh,
                                        oModel,
                                        mRemapIndex,
                                        remaped_vertices_cnt,
                                        oResStash,
                                        oCtx.sPackName);
                        if (res != uSUCCESS) { return res; }
                }
                if (oCtx.import_skin) {
                        res = ImportSkin(
                                        pNode,
                                        pMesh,
                                        oModel,
                                        mRemapIndex,
                                        remaped_vertices_cnt,
                                        oResStash,
                                        oCtx.sPackName);
                        if (res != uSUCCESS) { return res; }
                }

                //mRemapIndex.clear();

                oNodeData.vComponents.emplace_back(std::move(oModel));
        }
        else {

                log_d("empty node: '{}'", pNode->GetName());
        }

        return uSUCCESS;
}

static void ImportCustomProperty(FbxNode * pNode, NodeData & oNodeData, ImportCtx & oCtx) {

        if (!oCtx.import_info_prop) { return; }

        FbxProperty oProperty = pNode->GetFirstProperty();

        while(oProperty.IsValid()) {

                if (oProperty.GetFlag(FbxPropertyFlags::eUserDefined) &&
                    (oProperty.GetName() == "info") &&
                    (oProperty.GetPropertyDataType().GetType() == eFbxString) ) {

                        oNodeData.sInfo = oProperty.Get<FbxString>();
                        log_d("info: '{}'", oNodeData.sInfo);
                        break;
                }

                oProperty = pNode->GetNextProperty(oProperty);
        }
}

ret_code_t ImportNode(FbxNode * pNode, NodeData & oNodeData, ImportCtx & oCtx, ResourceStash & oResStash) {

        FbxAMatrix & mLocalTransform    = pNode->EvaluateLocalTransform();
        FbxDouble4 translation          = mLocalTransform.GetT();
        FbxDouble4 rotation             = mLocalTransform.GetR();
        FbxDouble4 scaling              = mLocalTransform.GetS();

        if (oCtx.flip_yz) {
                oNodeData.vTranslation.x = translation[0];
                oNodeData.vTranslation.y = - translation[2];
                oNodeData.vTranslation.z = translation[1];

                oNodeData.vRotation.x = rotation[0];
                oNodeData.vRotation.y = - rotation[2];
                oNodeData.vRotation.z = rotation[1];

                oNodeData.vScale.x = scaling[0];
                oNodeData.vScale.y = - scaling[2];
                oNodeData.vScale.z = scaling[1];
        }
        else {
                oNodeData.vTranslation.x = translation[0];
                oNodeData.vTranslation.y = translation[1];
                oNodeData.vTranslation.z = translation[2];

                oNodeData.vRotation.x = rotation[0];
                oNodeData.vRotation.y = rotation[1];
                oNodeData.vRotation.z = rotation[2];

                oNodeData.vScale.x = scaling[0];
                oNodeData.vScale.y = scaling[1];
                oNodeData.vScale.z = scaling[2];
        }

        if (oCtx.disable_nodes) {
                oNodeData.enabled = false;
        }
        oNodeData.sName = pNode->GetName();

        log_d("node translation: {}, {}, {}", oNodeData.vTranslation.x, oNodeData.vTranslation.y, oNodeData.vTranslation.z);
        log_d("node rotation:    {}, {}, {}", oNodeData.vRotation.x, oNodeData.vRotation.y, oNodeData.vRotation.z);
        log_d("node scaling:     {}, {}, {}", oNodeData.vScale.x, oNodeData.vScale.y, oNodeData.vScale.z);

        ret_code_t res = ImportAttributes(pNode, oNodeData, oCtx, oResStash);
        if (res != uSUCCESS) {
                return res;
        }

        ImportCustomProperty(pNode, oNodeData, oCtx);

        for(int32_t i = 0; i < pNode->GetChildCount(); ++i) {
                auto & oChildNodeData = oNodeData.vChildren.emplace_back(NodeData{});
                if (ret_code_t res = ImportNode(pNode->GetChild(i), oChildNodeData, oCtx, oResStash); res != uSUCCESS) {
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
