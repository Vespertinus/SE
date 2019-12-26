
#include <stdint.h>
#include <string_view>
#include <unordered_set>

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
                        - first material diffuse texture path
                        - for each polygon:
                                - vertex position (3f)
                                - normal (3f)
                                - texture uv0 set (2f)
*/
//TODO split mesh on shapes based on material count and links to polygons

namespace SE {
namespace TOOLS {

ret_code_t ImportNode(FbxNode * pNode, NodeData & oNodeData, ImportCtx & oCtx, ResourceStash & oResStash);


struct SkinVertInfo {
        glm::vec4       weights{0};
        uint8_t         indices[4]{0};
        uint8_t         cur_joint{0};

        uint8_t AddJoint(const uint8_t joint_id, const float weight, const uint32_t vert_id);
        void    Normalize();
};

uint8_t SkinVertInfo::AddJoint(const uint8_t joint_id, const float weight, const uint32_t vert_id) {

        /** exceed allowed joints cnt per vertex
            drop joint with smallest weight
            later re normalize weights
        */
        if (cur_joint >= 4) {
                float   min_w   = weights[0];
                uint8_t min_ind = 0;

                for (uint8_t i = 1; i < 4; ++i) {
                        if (weights[i] < min_w) {
                                min_w   = weights[i];
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
                                weights[min_ind]);

                weights[min_ind] = weight;
                indices[min_ind] = joint_id;
        }
        else {
                weights[cur_joint] = weight;
                indices[cur_joint] = joint_id;
        }

        return ++cur_joint;
}

void SkinVertInfo::Normalize() {

        if (cur_joint <= 4) { return; }

        float sum_w = weights.x + weights.y + weights.z + weights.w;
        weights *= 1 / sum_w;
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

        return ImportNode(pNode, oRootNode, oCtx, oResStash);
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
                std::unordered_map<std::string, uint8_t> & mJoints,
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

        mJoints.emplace(pRootNode->GetName(), 0);
        vJointFbxNodes.emplace_back(pRootNode);

        std::function<ret_code_t (FbxNode * pCurNode) > ProcessSkeletonNode;
        ProcessSkeletonNode = [pRootNode, &vJointFbxNodes, &mJoints, &ProcessSkeletonNode](FbxNode * pCurNode) {

                auto child_cnt = pCurNode->GetChildCount();
                for (auto i = 0; i < child_cnt; ++i) {
                        FbxNode * pChild = pCurNode->GetChild(i);

                        if (pChild->GetNodeAttribute()->GetAttributeType() != FbxNodeAttribute::eSkeleton) {
                                continue;
                        }

                        log_d("append joint node: '{}' to skeleton", pChild->GetName() );
                        mJoints.emplace(pChild->GetName(), vJointFbxNodes.size());
                        vJointFbxNodes.emplace_back(pChild);

                        if (auto res = ProcessSkeletonNode(pChild); res != uSUCCESS) {
                                return res;
                        }
                }

                return uSUCCESS;
        };

        ProcessSkeletonNode(pRootNode);

        log_d("build skeleton with {} joints", vJointFbxNodes.size());

        std::string sRootNodeName;

        if (auto * pParent = vJointFbxNodes[0]->GetParent()) {
                sRootNodeName = pParent->GetName();
        }
        else {
                sRootNodeName = vJointFbxNodes[0]->GetName();
        };

        //THINK duplication in differet fbx..
        //same with skeleton?
        std::string sShellName = fmt::format("cs:{}|pc:{}", StrID(sRootNodeName), sPackName);

        if (oResStash.GetResourceData(sShellName, &oModel.pSkin->pShell) ) {
                oModel.pSkin->pShell->sName     = sShellName;
                oModel.pSkin->pShell->sRootNode = sRootNodeName;
        }
        else {
                log_d("shell: '{}' already created", sShellName);
                return uSUCCESS;
        }

        //build skeleton name
        std::string sSkeletonBones;
        for (uint8_t i = 0; i < vJointFbxNodes.size(); ++i) {

                sSkeletonBones += vJointFbxNodes[i]->GetName();
        }
        std::string sSkeletonName = fmt::format("sk:{}:{}|pc:{}",
                        vJointFbxNodes.size(),
                        StrID(sSkeletonBones),
                        sPackName);

        if (!oResStash.GetResourceData(sSkeletonName, &oModel.pSkin->pShell->pSkeleton) ) {

                log_d("skeleton: '{}' already created", sSkeletonName);
                return uSUCCESS;
        }

        oModel.pSkin->pShell->pSkeleton->sName = sSkeletonName;
        oModel.pSkin->pShell->pSkeleton->vJoints.reserve(vJointFbxNodes.size());

        oModel.pSkin->pShell->pSkeleton->vJoints.emplace_back(JointData{
                        vJointFbxNodes[0]->GetName(),
                        BindPoseData{},
                        JointData::ROOT_PARENT_IND
                        });
        oModel.pSkin->pShell->pSkeleton->mBonesIndexes.emplace(vJointFbxNodes[0]->GetName(), 0);

        for (uint8_t i = 1; i < vJointFbxNodes.size(); ++i) {

                auto * pParent = vJointFbxNodes[i]->GetParent();
                se_assert(pParent);

                auto itParentInd = mJoints.find(pParent->GetName());
                if (itParentInd == mJoints.end()) {

                        log_e("joint '{}' parent ({}) outside joints hierarchy",
                                        vJointFbxNodes[i]->GetName(),
                                        pParent->GetName());
                        return uWRONG_INPUT_DATA;
                }

                JointData oCurJoint;
                oCurJoint.sName         = vJointFbxNodes[i]->GetName();
                oCurJoint.parent_index  = itParentInd->second;

                oModel.pSkin->pShell->pSkeleton->vJoints.emplace_back(std::move(oCurJoint));
                oModel.pSkin->pShell->pSkeleton->mBonesIndexes.emplace(vJointFbxNodes[i]->GetName(), i);
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
                log_e("unsupportes skinning type: '{}', only linear supported", pSkinDeformer->GetSkinningType());
                return uWRONG_INPUT_DATA;
        }

        uint8_t  joints_per_vertex{0};
        uint8_t  cur_joints_per_vertes{0};
        uint32_t cluster_cnt       = ((FbxSkin *)pMesh->GetDeformer(0, FbxDeformer::eSkin))->GetClusterCount();
        uint32_t cur_pos;
        std::unordered_map<std::string, uint8_t> mJoints;

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

                oModel.pSkin->oMeshBindPose.bind_pos   = glm::vec3(vBindPos[0], vBindPos[1], vBindPos[2]);
                oModel.pSkin->oMeshBindPose.bind_scale = glm::vec3(vBindScale[0], vBindScale[1], vBindScale[2]);
                oModel.pSkin->oMeshBindPose.bind_rot   = glm::vec4(qRot[0], qRot[1], qRot[2], qRot[3]);

                /*
                log_d("orig node: '{}' bind pose global transform pos: ({}, {}, {}), scale: ({}, {}, {})",
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
                        log_e("unsupported cluster link mode: '{}'", pCluster->GetLinkMode());
                        return uWRONG_INPUT_DATA;
                }


                uint32_t indices_cnt    = pCluster->GetControlPointIndicesCount();
                int32_t * pIndices      = pCluster->GetControlPointIndices();
                double  * pWeights      = pCluster->GetControlPointWeights();

                log_d("cluster({}) link node: '{}', vert indices cnt: {}",
                                i,
                                pCluster->GetLink()->GetName(),
                                indices_cnt);

                for (uint32_t j = 0; j < indices_cnt; ++j) {
                        se_assert(static_cast<uint32_t>(pIndices[j]) < vSkinInfo.size());
                        cur_joints_per_vertes = vSkinInfo[pIndices[j]].AddJoint(i, pWeights[j], pIndices[j]);
                        if (cur_joints_per_vertes > joints_per_vertex) {
                                joints_per_vertex = cur_joints_per_vertes;
                        }
                }

                auto itJointInd = mJoints.find(pCluster->GetLink()->GetName());
                if (itJointInd == mJoints.end()) {

                        log_e("joint '{}' not found in skeleton: '{}'",
                                        pCluster->GetLink()->GetName(),
                                        oModel.pSkin->pShell->pSkeleton->sName);
                        return uWRONG_INPUT_DATA;
                }



                if (pCluster->GetLink()->GetNodeAttribute()->GetAttributeType() != FbxNodeAttribute::eSkeleton) {

                        log_e("wrong joint node: '{}', attribute type != FbxNodeAttribute::eSkeleton",
                                        pCluster->GetLink()->GetName());
                        return uWRONG_INPUT_DATA;
                }

                auto itNode = oModel.pSkin->pShell->pSkeleton->mBonesIndexes.find(pCluster->GetLink()->GetName());
                if (itNode == oModel.pSkin->pShell->pSkeleton->mBonesIndexes.end()) {
                        log_e("cluster node '{}' not from skeleton: '{}', hierarchy, root node: '{}'",
                                        pCluster->GetLink()->GetName(),
                                        oModel.pSkin->pShell->pSkeleton->sName,
                                        oModel.pSkin->pShell->sRootNode);
                        return uWRONG_INPUT_DATA;
                }
                se_assert(itNode->second < oModel.pSkin->pShell->pSkeleton->vJoints.size());
                auto & oJointData = oModel.pSkin->pShell->pSkeleton->vJoints[itNode->second];

                if (!oJointData.bind_inited) {

                        oJointData.bind_inited = true;

                        FbxAMatrix pLinkTransformMatrix;
                        pCluster->GetTransformLinkMatrix(pLinkTransformMatrix);

                        /*
                        log_d("link node: '{}' bind pos: ({}, {}, {}), scale: ({}, {}, {})",
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

                        oJointData.oInvBindPose.bind_pos    = glm::vec3(vBindPos[0], vBindPos[1], vBindPos[2]);
                        oJointData.oInvBindPose.bind_scale  = glm::vec3(vBindScale[0], vBindScale[1], vBindScale[2]);
                        oJointData.oInvBindPose.bind_rot    = glm::vec4(qRot[0], qRot[1], qRot[2], qRot[3]);
                }

                //log_d("current node name: {}", pCluster->GetLink()->GetName());

                oModel.pSkin->vJointIndexes.emplace_back(itJointInd->second);
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
                                vSkinInfo[i].weights.x,
                                vSkinInfo[i].weights.y,
                                vSkinInfo[i].weights.z,
                                vSkinInfo[i].weights.w);
        }*/

        //remap vertices, compress and write into output buffer
        std::vector<float>      vWeightsBuffer(remaped_vertices_cnt * joints_per_vertex);
        std::vector<uint8_t>    vIndicesBuffer(remaped_vertices_cnt * joints_per_vertex);

        for (uint32_t i = 0; i < vSkinInfo.size(); ++i) {

                auto it = mRemapIndex.find(i);
                if (it == mRemapIndex.end()) {
                        log_w("vertex {} unused in skinning vertex index", i);
                        continue;
                }

                for (auto new_index : it->second) {

                        cur_pos = new_index * joints_per_vertex;
                        memcpy(&vWeightsBuffer[cur_pos], &vSkinInfo[i].weights[0], sizeof(float) * joints_per_vertex);
                        memcpy(&vIndicesBuffer[cur_pos], &vSkinInfo[i].indices[0], sizeof(uint8_t) * joints_per_vertex);
                }
        }

        log_d("skeleton: '{}', root node: '{}', cluster cnt: {}, joints_per_vertex: {}",
                        oModel.pSkin->pShell->pSkeleton->sName,
                        oModel.pSkin->pShell->sRootNode,
                        cluster_cnt,
                        joints_per_vertex);

        uint8_t stride = joints_per_vertex * sizeof(float);
        uint8_t weights_buffer_ind = oModel.pMesh->vVertexBuffers.size();
        oModel.pMesh->vVertexBuffers.emplace_back(MeshData::VertexBuffer{std::move(vWeightsBuffer), stride});
        //TODO write indices and weights in one buffer
        stride = joints_per_vertex * sizeof(uint8_t);
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

static ret_code_t ImportMaterial(FbxNode * pNode, MaterialData ** pMaterialData, ImportCtx & oCtx, ResourceStash & oResStash) {

        if (!(pNode->GetSrcObjectCount<FbxSurfaceMaterial>() > 0 && !oCtx.skip_material)) {
                return uSUCCESS;
        }

        FbxSurfaceMaterial * pMaterial = pNode->GetSrcObject<FbxSurfaceMaterial>(0);

        if(pMaterial) {
                //check by name in resource cache

                std::string sTextureName;
                auto oProperty = pMaterial->FindProperty(FbxSurfaceMaterial::sDiffuse);

                if (oProperty.IsValid() &&  oProperty.GetSrcObjectCount<FbxTexture>() > 0) {

                        if (FbxLayeredTexture * pLayeredTexture = oProperty.GetSrcObject<FbxLayeredTexture>(0);
                                        pLayeredTexture != nullptr &&
                                        pLayeredTexture->GetSrcObjectCount<FbxTexture>() > 0) {

                                FbxTexture * pTexture = pLayeredTexture->GetSrcObject<FbxTexture>(0);
                                if (pTexture != nullptr) {
                                        FbxFileTexture * pFileTexture = FbxCast<FbxFileTexture>(pTexture);
                                        sTextureName = pFileTexture->GetFileName();
                                        oCtx.FixPath(sTextureName);
                                        ++oCtx.textures_cnt;
                                }
                        }
                        else if (FbxTexture * pTexture = oProperty.GetSrcObject<FbxTexture>(0) ) {
                                FbxFileTexture * pFileTexture = FbxCast<FbxFileTexture>(pTexture);
                                sTextureName = pFileTexture->GetFileName();
                                oCtx.FixPath(sTextureName);
                                ++oCtx.textures_cnt;
                        }
                }

                /** create basic material with one texture */
                if (!sTextureName.empty()) {

                        log_d("diffuse texture: '{}'", sTextureName);

                        static std::string sDefaultShader = "shader_program/simple_tex.sesp";

                        std::string sMaterialName = pMaterial->GetName();
                        if (sMaterialName.empty()) {
                                sMaterialName = oCtx.sPackName + std::to_string(StrID(sTextureName + sDefaultShader));
                        }
                        else {
                                sMaterialName = oCtx.sPackName + sMaterialName;
                        }

                        bool created = oResStash.GetResourceData(sMaterialName, pMaterialData);
                        if (created) {
                                //TextureData by ptr for reusing
                                //currently only by material name
                                (*pMaterialData)->sShaderPath = sDefaultShader;
                                (*pMaterialData)->mTextures.emplace(
                                                TextureUnit::DIFFUSE,
                                                TextureData{std::move(sTextureName)} );

                                ++oCtx.material_cnt;
                        }
                }
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
        vVertexData.reserve(8);
        uint32_t                cur_index = 0;
        TPackVertexIndex        Pack;

        if ( pMesh->GetElementUVCount() == 0) {
                log_e("failed to get UV coordinates for mesh '{}'", pNode->GetName());
                return uWRONG_INPUT_DATA;
        }

        std::vector<float> vVertices;

        uint8_t stride               = ((oCtx.skip_normals) ? VERTEX_BASE_SIZE : VERTEX_SIZE) * sizeof(float);
        std::string sMeshName        = pMesh->GetName();

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
        FbxGeometryElementUV * pUV      = pMesh->GetElementUV(0);
        log_d("polygons: cnt = {}", polygon_cnt);

        uint32_t index_size = static_cast<uint32_t>(polygon_cnt) * 3;

        Pack = PackVertexIndexInit(index_size, pModelMesh->oIndex);

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

                                /*
                                log_d("new index = {}, pos ({}, {}, {}), normal ({}, {}, {}), uv ({}, {})",
                                                cur_index,
                                                vVertexData[0], vVertexData[1], vVertexData[2],
                                                (oCtx.skip_normals) ? 0 : vVertexData[3],
                                                (oCtx.skip_normals) ? 0 : vVertexData[4],
                                                (oCtx.skip_normals) ? 0 : vVertexData[5],
                                                vVertexData[6], vVertexData[7]);
                                                */

                                vVertices.insert(vVertices.end(), vVertexData.begin(), vVertexData.end());
                                ++oCtx.total_vertices_cnt;
                                mRemapIndex[vertex_ind].emplace(cur_index);
                        }/*
                        else {
                                log_d("old index = {}, pos ({}, {}, {}), normal ({}, {}, {}), uv ({}, {})",
                                                cur_index,
                                                vVertexData[0], vVertexData[1], vVertexData[2],
                                                (oCtx.skip_normals) ? 0 : vVertexData[3],
                                                (oCtx.skip_normals) ? 0 : vVertexData[4],
                                                (oCtx.skip_normals) ? 0 : vVertexData[5],
                                                vVertexData[6], vVertexData[7]);
                        }*/
                        Pack(pModelMesh->oIndex, cur_index);
/*
                        log_d("vertex {}, map to: {}, pos: ({}, {}, {}), normal: ({}, {}, {}), uv: ({}, {})",
                                        vertex_ind,
                                        cur_index,
                                        pControlPoints[vertex_ind][0],
                                        pControlPoints[vertex_ind][1],
                                        pControlPoints[vertex_ind][2],
                                        vVertexData[3],
                                        vVertexData[4],
                                        vVertexData[5],
                                        vVertexData[6],
                                        vVertexData[7]
                                        );
*/

                }
        }

        log_d("input vertex cnt: {}, output vertex cnt: {}, output estimated index cnt: {}",
                        vertices_cnt,
                        oVertexIndex.Size(),
                        index_size);

        oCtx.total_triangles_cnt += polygon_cnt;
        ++oCtx.mesh_cnt;
        remaped_vertices_cnt = oVertexIndex.Size();


        BoundingBox oBBox;
        oBBox.Calc(vVertices, (oCtx.skip_normals) ? VERTEX_BASE_SIZE : VERTEX_SIZE);
        pModelMesh->vShapes.emplace_back(0, index_size, std::move(oBBox));

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
                        buffer_ind });

        //currently only one shape per mesh
        //TODO support per material sub meshes;
        for (auto & oShape : pModelMesh->vShapes) {
                pModelMesh->oBBox.Concat(oShape.oBBox);
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

                if (res = ImportMaterial(pNode, &oModel.pMaterial, oCtx, oResStash); res != uSUCCESS) {
                        return res;
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

        if (oCtx.disable_nodes) {
                oNodeData.enabled = false;
        }
        oNodeData.sName = pNode->GetName();

        log_d("node translation: {}, {}, {}", oNodeData.translation.x, oNodeData.translation.y, oNodeData.translation.z);
        log_d("node rotation:    {}, {}, {}", oNodeData.rotation.x, oNodeData.rotation.y, oNodeData.rotation.z);
        log_d("node scaling:     {}, {}, {}", oNodeData.scale.x, oNodeData.scale.y, oNodeData.scale.z);

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
