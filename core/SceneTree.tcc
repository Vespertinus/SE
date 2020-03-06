
#include <fstream>
#include <MPTraits.h>
#include <MPUtil.h>

namespace SE {

template <class ... TComponents > SceneTree<TComponents ...>::SceneTree(
                const std::string & sName,
                const rid_t new_rid,
                bool empty) :
        ResourceHolder(new_rid, sName),
        pRoot(std::make_shared<NodeWrapper>("root", this, true)) {

        mNamedNodes.emplace(pRoot->GetName(), pRoot);
        pRoot->sFullName = "root"; //FIXME need only for root node, change API |SetName?
        mLocalNamedNodes.emplace(pRoot->GetName(), std::vector<TSceneNode>(1, pRoot) );

        if (!empty) {
                Load();
        }
}
template <class ... TComponents > SceneTree<TComponents ...>::~SceneTree() noexcept {

        //TODO remove all elements
}

template <class ... TComponents > typename SceneTree<TComponents...>::TSceneNode SceneTree<TComponents ...>::
         Create(TSceneNode pParent, const std::string_view sNewName, const bool enabled) {

         TSceneNode pNode = std::make_shared<NodeWrapper>(sNewName, this, enabled);
         if (auto ret = pParent->AddChild(pNode); ret != uSUCCESS) {
                 return nullptr;
         }

         log_d("name: '{}'", pNode->GetFullName());
         return pNode;
}

template <class ... TComponents > typename SceneTree<TComponents...>::TSceneNode SceneTree<TComponents ...>::
         Create(const std::string_view sNewName, const bool enabled) {

         return Create(pRoot, sNewName, enabled);
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
        if (SE::FlatBuffers::VerifySceneTreeBuffer(oVerifier) != true) {
                throw(std::runtime_error("failed to verify data in: " + sName));
        }

        auto * pSceneFB = SE::FlatBuffers::GetSceneTree(&vBuffer[0]);

        Load(pSceneFB->root());
}

template <class T> using TSerializedCheck = typename T::TSerialized;
template <class T> using THasSerialized   = typename std::experimental::is_detected<TSerializedCheck, T>::type;

template <class T> using TPostLoadCheck = decltype( &T::PostLoad );
template <class T> using THasPostLoad = typename std::experimental::is_detected<TPostLoadCheck, T>::type;
template <class T> constexpr bool THasPostLoadVal = std::experimental::is_detected_v<TPostLoadCheck, T>;

template <class TComponent> struct LoadWrapper {

        using TExactSerialized = typename TComponent::TSerialized;

        template <class TNode, class TPostLoadVec> ret_code_t Load(
                        const void * const pData,
                        TNode & pNode,
                        TPostLoadVec & vPostLoadComponents) const {

                const TExactSerialized * pSerialized = static_cast<const TExactSerialized *>(pData);
                auto res = pNode->template CreateComponent<TComponent>(pSerialized);

                if constexpr (THasPostLoadVal<TComponent>) {

                        if (res != uSUCCESS) { return res; }

                        auto * pComponent = pNode->template GetComponent<TComponent>();
                        se_assert(pComponent);

                        vPostLoadComponents.emplace_back(pComponent, pSerialized);
                }

                return res;
        }
};

template <class ... TComponents > void SceneTree<TComponents ...>::
        Load(const SE::FlatBuffers::Node * pRootFB) {


        using TFilteredTypes    = MP::FilteredTypelist<THasSerialized, TComponents...>;
        static_assert(!std::is_same<MP::TypelistWrapper<>, TFilteredTypes >::value, "not allowed empty componenents list");

        using TLoaderVariant            = typename MP::Typelist2WrappedTmplPack<
                std::variant,
                LoadWrapper,
                TFilteredTypes
                        >::Type;
        using TLoaderTuple              = typename MP::Typelist2WrappedTmplPack<
                std::tuple,
                LoadWrapper,
                TFilteredTypes
                        >::Type;

        using TLoaderMap                = std::map<FlatBuffers::ComponentU, TLoaderVariant>;

        using TPostLoadComponents       = MP::FilteredTypelistFromTL<THasPostLoad, TFilteredTypes>;
        using TPostLoadVariant          = typename MP::Typelist2WrappedTypeTmplPack<
                std::variant,
                std::add_pointer,
                TPostLoadComponents
                        >::Type;

        std::vector <std::pair<TPostLoadVariant, const void * const> > vPostLoadComponents;

        TLoaderTuple oLoaders;
        TLoaderMap mLoaders;

        auto InitMap = [&mLoaders](auto & oLoader) {

                using TExactSerialized = typename std::decay<decltype(oLoader)>::type::TExactSerialized;

                FlatBuffers::ComponentU key = FlatBuffers::ComponentUTraits<TExactSerialized>::enum_value;
                mLoaders.emplace(key, oLoader);
        };

        //init loaders map
        MP::TupleForEach(oLoaders, InitMap);

        if (auto res = LoadNode(pRootFB, nullptr, mLoaders, vPostLoadComponents); res != uSUCCESS) {
                throw (std::runtime_error("failed to load tree, reason: " +
                                          std::to_string(res) +
                                          ", from: " +
                                          sName));
        }

        for (auto & oEntry : vPostLoadComponents) {

                std::visit([&oEntry, this](auto * pComponent) {

                        using TExactSerialized = typename std::remove_pointer<decltype(pComponent)>::type::TSerialized;

                        const TExactSerialized * pSerialized = static_cast<const TExactSerialized *>(oEntry.second);

                        auto res = pComponent->PostLoad(pSerialized);

                        if (res != uSUCCESS) {

                                throw (std::runtime_error(fmt::format("failed to process post load component, loading tree",
                                                                sName)));
                        }
                },
                oEntry.first);
        }
}

template <class ... TComponents >
        template <class TMap, class TVec>
                ret_code_t SceneTree<TComponents ...>::
                        LoadNode(
                                        const SE::FlatBuffers::Node * pSrcNode,
                                        TSceneNode pParent,
                                        const TMap & mLoaders,
                                        TVec & vPostLoadComponents) {

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
                pDstNode = Create(pParent, sSrcName, pSrcNode->enabled());
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

                                res = std::visit([pCurComponentFB, &pDstNode, &vPostLoadComponents](auto & oLoader) {

                                                return oLoader.Load(pCurComponentFB->component(), pDstNode, vPostLoadComponents);


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
                res = LoadNode(pChildrenFB->Get(i), pDstNode, mLoaders, vPostLoadComponents);
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

template <class ... TComponents > void SceneTree<TComponents ...>::EnableAll() {

        pRoot->DepthFirstWalk([](auto & oNode) {

                        oNode.Enable();
                        return true;
        });

}

template <class ... TComponents > void SceneTree<TComponents ...>::DisableAll() {

        pRoot->DepthFirstWalk([](auto & oNode) {

                        oNode.Disable();
                        return true;
        });

}

} //namespace SE

