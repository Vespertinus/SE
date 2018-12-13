
namespace SE  {


template <class ... TComponents > SceneNode<TComponents ...>::
        SceneNode(const std::string_view sNewName, TSceneTree * pNewScene) :
                pParent(nullptr),
                sName(sNewName),
                pScene(pNewScene),
                user_flags(0),
                internal_flags(STATE_ENABLED) {

        log_d("create node: '{}'", sName);
}

template <class ... TComponents> SceneNode<TComponents...>::~SceneNode() noexcept {

        //TODO check destruction with shared ptr and dependencies loops
        Disable();
        //remove children
        //THINK children in broken state?
        /*
        if (pScene) {
                pScene->HandleNodeUnlink(this->weak_from_this());
        }
        */
}

template <class ... TComponents > void SceneNode<TComponents ...>::
        SetPos(const glm::vec3 & vPos) {

        oTransform.SetPos(vPos);
        InvalidateChildren();
}

template <class ... TComponents > void SceneNode<TComponents ...>::
        SetRotation(const glm::vec3 & vDegreeAngles) {

        oTransform.SetRotation(vDegreeAngles);
        InvalidateChildren();
}

template <class ... TComponents > void SceneNode<TComponents ...>::
        SetRotation(const glm::quat & qNewRotation) {

        oTransform.SetRotation(qNewRotation);
        InvalidateChildren();
}

template <class ... TComponents > void SceneNode<TComponents ...>::
        SetScale(const glm::vec3 & new_scale) {

        oTransform.SetScale(new_scale);
        InvalidateChildren();
}

template <class ... TComponents > void SceneNode<TComponents ...>::Translate(const glm::vec3 & vPos) {

        oTransform.Translate(vPos);
        InvalidateChildren();
}

template <class ... TComponents > void SceneNode<TComponents ...>::Rotate(const glm::vec3 & vDegreeAngles) {

        oTransform.Rotate(vDegreeAngles);
        InvalidateChildren();
}

template <class ... TComponents > void SceneNode<TComponents ...>::Scale(const glm::vec3 & new_scale) {

        oTransform.Scale(new_scale);
        InvalidateChildren();
}

template <class ... TComponents > ret_code_t SceneNode<TComponents ...>::
        SetParent(TSceneNodeExact * pNewParent) {

        ret_code_t res = uSUCCESS;

        if (pNewParent) {
                res = pNewParent->AddChild(this->shared_from_this());
        }
        else {
                oTransform.SetParent(nullptr);
        }
        //RebuildFullName();
        //FIXME UpdateNodeName... currently does't update scene tree maps
        return res;
}

template <class ... TComponents > ret_code_t SceneNode<TComponents ...>::
        AddChild(TSceneNode pNode) {

        if (!pNode || pNode.get() == this || pNode->pParent == this) {
                log_w("failed to add, node ptr = {:p} cur node = {:p}, parent = {:p}", (void *)pNode.get(), (void *)this, (pNode) ? (void *)pNode->pParent : nullptr);
                return uWRONG_INPUT_DATA;
        }

        if (pParent && pParent == pNode.get()) {
                log_w("failed to add, prevent cyclic link, parent = {:p}, node = {:p}", (void *)pParent, (void *)pNode.get());
                return uWRONG_INPUT_DATA;
        }


        TSceneNodeExact * pOldParent = pNode->pParent;
        if (pOldParent) {

                pOldParent->RemoveChild(pNode);
        }
        else {
                pNode->pScene = pScene;
        }

        std::string sNewFullName;
        pNode->BuildFullName(sNewFullName, pNode->sName, this);
        bool res = pScene->UpdateNodeName(pNode, pNode->sName, sNewFullName);
        if (res) {
                pNode->sFullName = std::move(sNewFullName);
        }
        else {
                log_w("failed to add node: '{}' as a child of '{}', full name duplication",
                                pNode->GetFullName(),
                                GetFullName());
                //THINK...
                if (pOldParent) {
                        pOldParent->AddChild(pNode);
                }
                return uWRONG_INPUT_DATA;
        }


        pNode->pParent = this;
        pNode->oTransform.SetParent(&oTransform);
        //pNode->RebuildFullName();
        pNode->internal_flags ^= STATE_UNLINKED;

        vChildren.emplace_back(pNode);

        log_d("children cnt = {}", vChildren.size());
        return uSUCCESS;
}

template <class ... TComponents > void SceneNode<TComponents ...>::
        InvalidateChildren() {

        for (auto & item : vChildren) {
                item->oTransform.Invalidate();
                item->InvalidateChildren();
        }
}

template <class ... TComponents > const std::string & SceneNode<TComponents ...>::
        GetName() const {

        return sName;
}

template <class ... TComponents > const std::string & SceneNode<TComponents ...>::
        GetFullName() const {

        return sFullName;
}


template <class ... TComponents > void SceneNode<TComponents ...>::
        RemoveChild(TSceneNode pNode) {

        //FIXME check that it's child

        pNode->pParent = nullptr;
        pNode->oTransform.SetParent(nullptr);

        pScene->HandleNodeUnlink(pNode.get());
        pNode->pScene = nullptr;
        pNode->internal_flags |= STATE_UNLINKED;

        //FIXME rewrite on switching last and cur
        vChildren.erase(std::remove(vChildren.begin(), vChildren.end(), pNode));
}

template <class ... TComponents > void SceneNode<TComponents ...>::
        Unlink() {

        log_d("name: '{}', components: {}", sFullName, vComponents.size());

        if (pParent) {
                pParent->RemoveChild(this->shared_from_this());
        }
        else if (!(internal_flags && STATE_UNLINKED)) {
                log_e("call Unlink on node without parent, and not unlinked previously, name: '{}'", sName);
        }
}

template <class ... TComponents > void SceneNode<TComponents ...>::
        Print(const size_t indent, bool recursive) {

        log_d_clean("{:>{}} '{}': components cnt = {}", ">", indent, sFullName, vComponents.size());
        oTransform.Print(indent + 2);
        if (!sCustomInfo.empty()) {
                log_d_clean("{:>{}} info: '{}'", ">", indent + 2, sCustomInfo);
        }
        for (auto & oComponent : vComponents) {

                std::visit([indent](auto && arg) {
                        log_d_clean("{:>{}} component: {}", ">", indent + 2, arg->Str());
                },
                oComponent);
        }

        if (recursive) {
                for (auto & pItem : vChildren) {
                        pItem->Print(indent + 4);
                }
        }
}

template <class ... TComponents > bool SceneNode<TComponents ...>::
        SetName(const std::string_view sNewName) {

        if (!pScene) { return false; }

        std::string sNewFullName;
        BuildFullName(sNewFullName, sNewName, pParent);

        bool res = pScene->UpdateNodeName(this->shared_from_this(), sNewName, sNewFullName);
        if (res) {
                sName     = sNewName;
                sFullName = std::move(sNewFullName);
        }

        return res;
}

template <class ... TComponents > SceneTree<TComponents...> * SceneNode<TComponents ...>::
        GetScene() const {

        return pScene;
}

template <class ... TComponents > void SceneNode<TComponents ...>::
        BuildFullName(std::string & sNewFullName, const std::string_view sNewName, TSceneNodeExact * pNewParent) {

        if (pNewParent) {
                sNewFullName = pNewParent->GetFullName() + "|" + sNewName.data();
        }
        else {
                sNewFullName = sNewName;
        }

        //TODO  use UpdateNodeName!!!
}


template <class ... TComponents > void SceneNode<TComponents ...>::
        RebuildFullName() {

        BuildFullName(sFullName, sName, pParent);
}

template <class ... TComponents > uint32_t SceneNode<TComponents ...>::GetComponentsCnt() const {

        return vComponents.size();
}


template <class ... TComponents > const Transform & SceneNode<TComponents ...>::
        GetTransform() const {

        return oTransform;
}

template <class ... TComponents >
        template <class THandler>
                void SceneNode<TComponents ...>::DepthFirstWalk(THandler && oHandler) {

         bool res = oHandler(*this);
         if (res == false) { return; }

         for (auto & pChild : vChildren) {
                pChild->DepthFirstWalk(oHandler);
         }
}

template <class ... TComponents >
        template <class THandler, class TPostHandler>
                void SceneNode<TComponents ...>::DepthFirstWalkEx(THandler && oHandler, TPostHandler && oPostHandler) {


         bool res = oHandler(*this);
         if (res == false) {
                 return;
         }

         for (auto & pChild : vChildren) {
                pChild->DepthFirstWalkEx(oHandler, oPostHandler);
         }
         oPostHandler(*this);
}

template <class ... TComponents >
        template <class THandler>
                void SceneNode<TComponents ...>::BreadtFirstWalk(THandler && oHandler) {

         bool res = oHandler(*this);
         if (res == false) { return; }

         BreadtFirstWalkChild(oHandler);

}

template <class ... TComponents >
        template <class THandler>
                void SceneNode<TComponents ...>::BreadtFirstWalkChild(THandler && oHandler) {

         for (auto pChild : vChildren) {
                oHandler(*pChild);
         }
         for (auto pChild : vChildren) {
                pChild->BreadtFirstWalkChild(oHandler);
         }
}

template <class ... TComponents >
        template <class ... THandler>
                void SceneNode<TComponents ...>::ForEachComponent(THandler && ... oHandlers) {

        for (auto & oComponent : vComponents) {
                MP::Visit(oComponent, oHandlers...);
        }
}

template <class ... TComponents >
        void SceneNode<TComponents ...>::SetCustomInfo(const std::string_view sInfo) {

        sCustomInfo = sInfo;
}

template <class ... TComponents >
        const std::string & SceneNode<TComponents ...>::GetCustomInfo() const {

        return sCustomInfo;

}

template <class ... TComponents >
        void SceneNode<TComponents ...>::RotateAround(const glm::vec3 & vPoint, const glm::vec3 & vDegreeAngles) {

        oTransform.RotateAround(vPoint, vDegreeAngles);
        InvalidateChildren();
}

template <class ... TComponents >
        void SceneNode<TComponents ...>::RotateAround(const glm::vec3 & vPoint, const glm::quat & qDeltaRotation) {
        oTransform.RotateAround(vPoint, qDeltaRotation);
        InvalidateChildren();
}

template <class ... TComponents >
        void  SceneNode<TComponents ...>::SetFlags(const uint8_t state) {

        user_flags |= state;
}

template <class ... TComponents >
        uint8_t SceneNode<TComponents ...>::GetFlags() const {

        return user_flags;
}

template <class ... TComponents >
        void SceneNode<TComponents ...>::ClearFlags() {

        user_flags = 0;
}

template <class ... TComponents >
        void SceneNode<TComponents ...>::ClearFlags(const uint8_t state) {

        user_flags ^= state;
}

template <class ... TComponents>
        template <class TComponent, class ... TArgs>
                ret_code_t SceneNode<TComponents...>::CreateComponent(TArgs && ... oArgs) {

        if (HasComponent<TComponent>()) {
                log_w("obj: '{}', component '{}' already exist",
                                sName,
                                typeid(TComponent).name());
                return uWRONG_INPUT_DATA;
        }

        try {
                vComponents.emplace_back(std::make_unique<TComponent>(this, std::forward <TArgs...>(oArgs...)));
        }
        catch(std::exception & ex) {
                log_e("got exception, description = '{}', name: '{}'", ex.what(), sName);
                return uWRONG_INPUT_DATA;
        }
        catch(...) {
                log_e("got unknown exception, name: '{}'", sName);
                return uWRONG_INPUT_DATA;
        }

        return uSUCCESS;
}

template <class ... TComponents>
        template <class TComponent>
                void SceneNode<TComponents...>::DestroyComponent() {

        for (auto it = vComponents.begin(); it != vComponents.end(); ++it) {

                if (std::holds_alternative<std::unique_ptr<TComponent>>(*it)) {
                        vComponents.erase(it);
                        break;
                }
        }
}

template <class ... TComponents>
        template <class TComponent> bool SceneNode<TComponents...>::HasComponent() const {

        bool result = false;

        for (auto & oItem : vComponents) {
                if (std::holds_alternative<std::unique_ptr<TComponent>>(oItem)) {
                        result = true;
                        break;
                }
        }

        return result;
}

template <class ... TComponents>
        template <class TComponent> TComponent * SceneNode<TComponents...>::GetComponent() {

        //TODO return shared_ptr<Component> from Node;

        for (auto & oItem : vComponents) {
                if (std::holds_alternative<std::unique_ptr<TComponent>>(oItem)) {

                        return std::get<std::unique_ptr<TComponent>>(oItem).get();
                }
        }

        log_w("obj: '{}', failed to find component '{}'",
                        sName,
                        typeid(TComponent).name());
        return nullptr;
}

template <class ... TComponents>
        void SceneNode<TComponents...>::Disable() {

        if (!(internal_flags & STATE_ENABLED)) { return; }
        internal_flags ^= STATE_ENABLED;

        for (auto & oItem : vComponents) {
                std::visit([](auto & oComponent) {
                        oComponent->Disable();
                },
                oItem);
        }
}

template <class ... TComponents>
        void SceneNode<TComponents...>::Enable() {

        if (internal_flags & STATE_ENABLED) { return; }
        internal_flags |= STATE_ENABLED;

        for (auto & oItem : vComponents) {
                std::visit([](auto & oComponent) {
                        oComponent->Enable();
                },
                oItem);
        }
}


template <class ... TComponents>
        std::shared_ptr<SceneNode<TComponents...>> SceneNode<TComponents...>::GetShared() const {
        return this->shared_from_this();
}

}

