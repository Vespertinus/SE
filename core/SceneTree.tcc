
#include <fstream>

namespace SE {

template <class ... TGeom > SceneTree<TGeom ...>::SceneTree(
                const std::string & sName,
                const rid_t new_rid,
                const Settings & oSettings) :
        ResourceHolder(new_rid, sName),
        oRoot(nullptr, "root", this) {

        mNamedNodes.emplace(oRoot.GetName(), &oRoot);

        Load(oSettings);
}
template <class ... TGeom > SceneTree<TGeom ...>::~SceneTree() noexcept {

        //TODO remove all elements
}

template <class ... TGeom > SceneNode<TGeom ...> * SceneTree<TGeom ...>::
         Create(TSceneNode * pParent, const std::string_view sNewName) {

         TSceneNode * pNode = new TSceneNode(pParent, sNewName, this);

         if (!sNewName.empty()) {
                 StrID sid(pNode->GetFullName());

                 auto it = mNamedNodes.find(sid);
                 if (it != mNamedNodes.end()) {
                        log_e("duplication found, node with name: '{}' already exist", pNode->GetFullName() );
                        delete pNode;
                        return nullptr;
                 }
                 else {
                        mNamedNodes.emplace(sid, pNode);
                 }
         }

         log_d("name: '{}'", pNode->GetFullName());
         return pNode;
}

template <class ... TGeom > SceneNode<TGeom ...> * SceneTree<TGeom ...>::
         Create(const std::string_view sNewName) {

         return Create(&oRoot, sNewName);
}

template <class ... TGeom > SceneNode<TGeom ...> * SceneTree<TGeom ...>::
        Find(const StrID sid) const {

        auto it = mNamedNodes.find(sid);
        if (it != mNamedNodes.end()) {
                return it->second;
        }
        return nullptr;
}

template <class ... TGeom > SceneNode<TGeom ...> * SceneTree<TGeom ...>::
        GetRoot() const {

        return &oRoot;
}

template <class ... TGeom > bool SceneTree<TGeom ...>::
        Destroy(TSceneNode * pNode) {

        if (pNode == &oRoot) { return false; }

        TSceneNode * pOldParent = &oRoot;

        if (pNode->pParent) {
                pOldParent = pNode->pParent;
                pNode->SetParent(nullptr);

        }
        for (auto * pItem : pNode->vChildren) {
                pOldParent->AddChild(pItem);
        }

        delete(pNode);

        return true;
}

template <class ... TGeom > bool SceneTree<TGeom ...>::
        Destroy(const std::string_view sName) {

        if (sName.empty()) {
                log_w("failed to find node, reason: empty name");
                return false;
        }

        auto it = mNamedNodes.find(sName);
        if (it != mNamedNodes.end()) {
                TSceneNode * pNode = it->second;
                mNamedNodes.erase(it);
                return Destroy(pNode);
        }
        else {
                log_w("failed to find node: '{}'", sName);
        }
        return false;
}


template <class ... TGeom > void SceneTree<TGeom ...>::
        Print() {

        if (gLogger->level() != spdlog::level::debug) { return; }

        oRoot.Print(0);
}


template <class ... TGeom > void SceneTree<TGeom ...>::
        Load(const Settings & oSettings) {

        static const size_t max_file_size = 1024 * 1024 * 100;

        auto file_size = boost::filesystem::file_size(sName);
        if (file_size > max_file_size) {
                throw(std::runtime_error(
                                        "too big file size, allowed max = " +
                                        std::to_string(max_file_size) +
                                        ", got " +
                                        std::to_string(file_size) +
                                        " bytes, file: " +
                                        sName));
        }

        std::vector<char> vBuffer(file_size);
        log_d("buffer size: {}", vBuffer.size());

        //TODO rewrite on os wrappers that in linux case call mmap
        {
                std::ifstream oInput(sName, std::ios::binary | std::ios::in);
                if(!oInput.is_open()) {
                        throw(std::runtime_error("failed to open file: " + sName));
                }
                oInput.read(&vBuffer[0], file_size);
        }
        flatbuffers::Verifier oVerifier(reinterpret_cast<uint8_t *>(&vBuffer[0]), file_size);
        if (SE::FlatBuffers::VerifyNodeBuffer(oVerifier) != true) {
                throw(std::runtime_error("failed to verify data in: " + sName));
        }

        Load(SE::FlatBuffers::GetNode(&vBuffer[0]), oSettings);
}


template <class ... TGeom > void SceneTree<TGeom ...>::
        Load(const SE::FlatBuffers::Node * pRoot, const Settings & oSettings) {

        if (auto res = LoadNode(pRoot, nullptr, oSettings); res != uSUCCESS) {
                throw (std::runtime_error("failed to load tree, reason: " +
                                          std::to_string(res) +
                                          ", from: " +
                                          sName));
        }
}

template <class ... TGeom > ret_code_t SceneTree<TGeom ...>::
        LoadNode(const SE::FlatBuffers::Node * pSrcNode, TSceneNode * pParent, const Settings & oSettings) {

        TSceneNode * pDstNode;
        auto * pNameFB = pSrcNode->name();
        std::string_view sSrcName = (pNameFB != nullptr) ? pNameFB->c_str() : "";

        if (pParent == nullptr) {
                pDstNode = &oRoot;
                if (bool res = pDstNode->SetName(sSrcName); res != true) {
                        return uWRONG_INPUT_DATA;
                }
                log_d("root node name: {}", sSrcName);
        }
        else {
                pDstNode = Create(pParent, sSrcName);
                if (pDstNode == nullptr) {
                        return uWRONG_INPUT_DATA;
                }
        }


        auto * pTranslationFB   = pSrcNode->translation();
        auto * pRotationFB      = pSrcNode->rotation();
        auto * pScaleFB         = pSrcNode->scale();

        pDstNode->SetPos     (glm::vec3(pTranslationFB->x(), pTranslationFB->y(), pTranslationFB->z()));
        pDstNode->SetRotation(glm::vec3(pRotationFB->x(), pRotationFB->y(), pRotationFB->z()));
        pDstNode->SetScale   (glm::vec3(pScaleFB->x(), pScaleFB->y(), pScaleFB->z()));

        if (pSrcNode->info() != nullptr) {
                pDstNode->SetCustomInfo(pSrcNode->info()->c_str());
        }

        auto * pEntityFB          = pSrcNode->render_entity();
        if (pEntityFB != nullptr) {
                //TODO support instances from maya
                //they does not write mesh name, only node name
                //may be compare Mesh pointes..

                size_t entity_cnt     = pEntityFB->Length();
                for (size_t i = 0; i < entity_cnt; ++i) {
                        auto * pCurEntity = pEntityFB->Get(i);
                        //FIXME currently without instances..
                        auto * pMesh = CreateResource<TMesh>(sName + ":" + pDstNode->GetFullName() + ":" + pCurEntity->name()->c_str(), pCurEntity->data(), oSettings.oMeshSettings);
                        pDstNode->AddRenderEntity(pMesh);
                }
        }

        auto * pChildrenFB      = pSrcNode->children();
        if (pChildrenFB == nullptr) { return uSUCCESS; }
        size_t children_cnt     = pChildrenFB->Length();

        for (size_t i = 0; i < children_cnt; ++i) {
                LoadNode(pChildrenFB->Get(i), pDstNode, oSettings);
        }

        return uSUCCESS;
}

template <class ... TGeom > bool SceneTree<TGeom ...>::
        UpdateNodeName(TSceneNode * pNode, const std::string_view sNewName, const std::string_view sNewFullName) {

        if (pNode->GetScene() != this) {
                log_w("node: {}, from other scene", pNode->GetFullName());
                return false;
        }

        StrID oFullNameID(sNewFullName);

        if (!sNewName.empty()) {
                auto itCheckNode = mNamedNodes.find(oFullNameID);
                if (itCheckNode != mNamedNodes.end()) {
                        log_w("failed to rename node from: '{}', to '{}', node already exist with same name",
                                        pNode->GetFullName(),
                                        sNewFullName);
                        return false;
                }
        }

        if (!pNode->GetName().empty()) {

                auto itCheckNode = mNamedNodes.find(pNode->GetFullName());
                if (itCheckNode == mNamedNodes.end() || itCheckNode->second != pNode) {
                        log_w("failed to find node by old name = '{}'", pNode->GetFullName());
                        return false;
                }
                mNamedNodes.erase(itCheckNode);
        }

        if (!sNewName.empty()) {
                mNamedNodes.emplace(oFullNameID, pNode);
        }

        return true;
}

template <class ... TGeom > void SceneTree<TGeom ...>::
        Draw() const {

        oRoot.Draw();
}

} //namespace SE

