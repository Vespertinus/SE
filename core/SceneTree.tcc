
#include <fstream>

namespace SE {

template <class ... TComponents > SceneTree<TComponents ...>::SceneTree(
                const std::string & sName,
                const rid_t new_rid) :
        ResourceHolder(new_rid, sName),
        pRoot(std::make_shared<NodeWrapper>("root", this)) {

        mNamedNodes.emplace(pRoot->GetName(), pRoot);
        mLocalNamedNodes.emplace(pRoot->GetName(), std::vector<TSceneNode>(1, pRoot) );

        Load();
}
template <class ... TComponents > SceneTree<TComponents ...>::~SceneTree() noexcept {

        //TODO remove all elements
}

template <class ... TComponents > typename SceneTree<TComponents...>::TSceneNode SceneTree<TComponents ...>::
         Create(TSceneNode pParent, const std::string_view sNewName) {

         TSceneNode pNode = std::make_shared<NodeWrapper>(sNewName, this);
         if (auto ret = pParent->AddChild(pNode); ret != uSUCCESS) {
                 return nullptr;
         }

         log_d("name: '{}'", pNode->GetFullName());
         return pNode;
}

template <class ... TComponents > typename SceneTree<TComponents...>::TSceneNode SceneTree<TComponents ...>::
         Create(const std::string_view sNewName) {

         return Create(pRoot, sNewName);
}

template <class ... TComponents > typename SceneTree<TComponents...>::TSceneNode SceneTree<TComponents ...>::
        FindFullName(const StrID sid) const {

        auto it = mNamedNodes.find(sid);
        if (it != mNamedNodes.end()) {
                return it->second;
        }
        return nullptr;
}
template <class ... TComponents > const std::vector<std::shared_ptr<SceneNode<TComponents ...>>> * SceneTree<TComponents ...>::
        FindLocalName(const StrID sid) const {

        auto it = mLocalNamedNodes.find(sid);
        if (it != mLocalNamedNodes.end()) {
                return &it->second;
        }
        return nullptr;
}

template <class ... TComponents > typename SceneTree<TComponents...>::TSceneNode SceneTree<TComponents ...>::
        GetRoot() {

        return pRoot;
}

template <class ... TComponents > void SceneTree<TComponents ...>::
        HandleNodeUnlink(TSceneNodeExact * pNode) {

        if (pNode == nullptr || pNode->GetScene() != this) {
                return;
        }

        if (pNode->GetName().empty()) { return; }

        mNamedNodes.erase(pNode->GetFullName());

        auto it = mLocalNamedNodes.find(pNode->GetName());
        if (it != mLocalNamedNodes.end()) {
                auto & vNames = it->second;

                log_d("CHECK: before size: {}", vNames.size());

                vNames.erase(std::remove_if(
                                        vNames.begin(),
                                        vNames.end(),
                                        [pNode](const auto & pCheckNode) {

                                                return pCheckNode.get() == pNode;
                                        }),
                              vNames.end());
                log_d("CHECK: after size: {}", vNames.size());
                if (vNames.size() == 0) {
                        mLocalNamedNodes.erase(it);
                }
        }
}

template <class ... TComponents > void SceneTree<TComponents ...>::
        Print() {

        if (gLogger->level() != spdlog::level::debug) { return; }

        pRoot->Print(0);
}


template <class ... TComponents > void SceneTree<TComponents ...>::
        Load() {

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

        Load(SE::FlatBuffers::GetNode(&vBuffer[0]));
}


template <class ... TComponents > void SceneTree<TComponents ...>::
        Load(const SE::FlatBuffers::Node * pRoot) {

        TLoaderTuple oLoaders;
        TLoaderMap mLoaders;

        auto InitMap = [&mLoaders](auto & oLoader) {

                using TExactSerialized = typename std::decay<decltype(oLoader)>::type::TExactSerialized;

                FlatBuffers::ComponentU key = FlatBuffers::ComponentUTraits<TExactSerialized>::enum_value;
                mLoaders.emplace(key, oLoader);
        };

        //init loaders map
        MP::TupleForEach(oLoaders, InitMap);

        if (auto res = LoadNode(pRoot, nullptr, mLoaders); res != uSUCCESS) {
                throw (std::runtime_error("failed to load tree, reason: " +
                                          std::to_string(res) +
                                          ", from: " +
                                          sName));
        }
}

template <class ... TComponents > ret_code_t SceneTree<TComponents ...>::
        LoadNode(const SE::FlatBuffers::Node * pSrcNode, TSceneNode pParent, const TLoaderMap & mLoaders) {

        TSceneNode pDstNode;
        auto * pNameFB = pSrcNode->name();
        std::string_view sSrcName = (pNameFB != nullptr) ? pNameFB->c_str() : "";

        if (!pParent) {
                pDstNode = pRoot;
                if (bool res = pDstNode->SetName(sSrcName); res != true) {
                        return uWRONG_INPUT_DATA;
                }
                log_d("root node name: {}", sSrcName);
        }
        else {
                pDstNode = Create(pParent, sSrcName);
                if (!pDstNode) {
                        log_e("failed to create node: '{}'", sSrcName);
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

        //___Start___ load components
        auto * pComponentsFB = pSrcNode->components();
        if (pComponentsFB) {

                ret_code_t res;

                size_t components_cnt = pComponentsFB->Length();
                for (size_t i = 0; i < components_cnt; ++i) {
                        auto * pCurComponentFB = pComponentsFB->Get(i);
                        auto it = mLoaders.find(pCurComponentFB->component_type());

                        if (it != mLoaders.end()) {

                                res = std::visit([pCurComponentFB, &pDstNode](auto & oLoader) {

                                                return oLoader.Load(pCurComponentFB->component(), pDstNode);


                                                },
                                                it->second);
                                if (res != uSUCCESS) {
                                        log_e("failed to load component, flatbuffers: '{}', node: '{}', tree: '{}'",
                                                        FlatBuffers::EnumNameComponentU(
                                                                pCurComponentFB->component_type()),
                                                        pDstNode->GetFullName(),
                                                        sName );
                                        return res;
                                }
                        }
                        else {

                                log_e("unknown component type: '{}', node: '{}', tree: '{}'",
                                                FlatBuffers::EnumNameComponentU(
                                                        pCurComponentFB->component_type()),
                                                pDstNode->GetFullName(),
                                                sName );
                                return uWRONG_INPUT_DATA;
                        }
                }

        }
        //___End_____ load components

        auto * pChildrenFB      = pSrcNode->children();
        if (pChildrenFB == nullptr) { return uSUCCESS; }
        size_t children_cnt     = pChildrenFB->Length();

        ret_code_t res = uSUCCESS;
        for (size_t i = 0; i < children_cnt; ++i) {
                res = LoadNode(pChildrenFB->Get(i), pDstNode, mLoaders);
                if (res != uSUCCESS) { return res; }
        }

        return uSUCCESS;
}

template <class ... TComponents > bool SceneTree<TComponents ...>::
        UpdateNodeName(TSceneNode pNode, const std::string_view sNewName, const std::string_view sNewFullName) {

        if (pNode->GetScene() != this) {
                log_w("node: {}, from other scene", pNode->GetFullName());
                return false;
        }

        StrID full_name_id(sNewFullName);
        StrID local_name_id(sNewName);

        if (!sNewName.empty()) {
                auto itCheckNode = mNamedNodes.find(full_name_id);
                if (itCheckNode != mNamedNodes.end()) {
                        log_w("failed to rename node from: '{}', to '{}', node already exist with same name",
                                        pNode->GetFullName(),
                                        sNewFullName);
                        return false;
                }
        }

        //THINK GetFullName check..
        if (!pNode->GetName().empty() && !pNode->GetFullName().empty()) {

                auto itCheckNode = mNamedNodes.find(pNode->GetFullName());
                if (itCheckNode == mNamedNodes.end() || itCheckNode->second != pNode) {
                        log_w("failed to find node by old name = '{}'", pNode->GetFullName());
                        return false;
                }

                auto itLocalCheckNode = mLocalNamedNodes.find(pNode->GetName());
                if (itLocalCheckNode == mLocalNamedNodes.end() ) {
                        log_w("failed to find node by old local name = '{}'", pNode->GetFullName());
                        return false;
                }

                if (auto itL = std::find(itLocalCheckNode->second.begin(), itLocalCheckNode->second.end(), pNode); itL != itLocalCheckNode->second.end() ) {
                        if (itLocalCheckNode->second.size() > 1) {
                                itLocalCheckNode->second.erase(itL);
                        }
                        else {
                                mLocalNamedNodes.erase(itLocalCheckNode);
                        }
                }
                else {
                        log_w("failed to find node by old local name = '{}', vec size = {}",
                                        pNode->GetFullName(),
                                        itLocalCheckNode->second.size());
                        return false;
                }

                mNamedNodes.erase(itCheckNode);
        }

        if (!sNewName.empty()) {
                mNamedNodes.emplace(full_name_id, pNode);

                auto itLocalCheckNode = mLocalNamedNodes.find(local_name_id);
                if (itLocalCheckNode == mLocalNamedNodes.end() ) {
                        mLocalNamedNodes.emplace(local_name_id, std::vector<TSceneNode>(1, pNode) );
                }
                else {
                        itLocalCheckNode->second.emplace_back(pNode);
                }
        }

        return true;
}

} //namespace SE

