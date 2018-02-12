
namespace SE {

template <class ... TGeom > SceneTree<TGeom ...>::SceneTree() :
        oRoot(nullptr, "root") {

}
template <class ... TGeom > SceneTree<TGeom ...>::~SceneTree() noexcept {

        //TODO remove all elements
}

template <class ... TGeom > SceneNode<TGeom ...> * SceneTree<TGeom ...>::
         Create(TSceneNode * pParent, const std::string_view sNewName) {

         TSceneNode * pNode = new TSceneNode(pParent, sNewName);

         if (!sNewName.empty()) {

                 auto it = mNamedNodes.find(sNewName);
                 if (it != mNamedNodes.end()) {
                        log_e("duplication found, node with name: '{}' already exist", sNewName);
                        delete pNode;
                        return nullptr;
                 }
                 else {
                        mNamedNodes.emplace(pNode->GetName(), pNode);
                 }
         }

         log_d("name: '{}'", sNewName);
         return pNode;
}

template <class ... TGeom > SceneNode<TGeom ...> * SceneTree<TGeom ...>::
         Create(const std::string_view sNewName) {

         return Create(&oRoot, sNewName);
}

template <class ... TGeom > SceneNode<TGeom ...> * SceneTree<TGeom ...>::
        Find(const std::string_view sName) const {

        auto it = mNamedNodes.find(sName);
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

        oRoot.Print(0);
}



} //namespace SE
