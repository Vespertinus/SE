
//#include <MPTraits.h>
#include <experimental/type_traits>


namespace {

template <class T> using TCheck = decltype( &T::TargetTransformChanged );
template <class T> constexpr bool TEvaluateCond = std::experimental::is_detected_v<TCheck, T>;

}

namespace SE  {


template <class ... TComponents > SceneNode<TComponents ...>::
        SceneNode(const std::string_view sNewName, TSceneTree * pNewScene, const bool enabled) :
                pParent(nullptr),
                sName(sNewName),
                pScene(pNewScene),
                name_id(sNewName),
                user_flags(0),
                internal_flags(enabled ? STATE_ENABLED : 0) {

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

template <class ... TComponents > void SceneNode<TComponents ...>::
        SetWorldPos(const glm::vec3 & vWorldPos) {

        oTransform.SetWorldPos(vWorldPos);
        InvalidateChildren();
}

template <class ... TComponents > void SceneNode<TComponents ...>::
        SetWorldRotation(const glm::vec3 & vDegreeAngles) {

        oTransform.SetWorldRotation(vDegreeAngles);
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

template <class ... TComponents > void SceneNode<TComponents ...>::
        LookAt(const glm::vec3 & vLocalPoint, const glm::vec3 & vUp) {

        oTransform.LookAt(vLocalPoint, vUp);
        InvalidateChildren();
}

template <class ... TComponents > void SceneNode<TComponents ...>::
        WorldLookAt(const glm::vec3 & vWorldPoint, const glm::vec3 & vUp) {

        oTransform.WorldLookAt(vWorldPoint, vUp);
        InvalidateChildren();
}

template <class ... TComponents > void SceneNode<TComponents ...>::
        TranslateLocal(const glm::vec3 & vPos) {

        oTransform.TranslateLocal(vPos);
        InvalidateChildren();
}

template <class ... TComponents > void SceneNode<TComponents ...>::
        RotateLocal(const glm::vec3 & vDegreeAngles) {

        oTransform.RotateLocal(vDegreeAngles);
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


        //childs already notified
        if (oTransform.CDirty()) { return; }
        oTransform.SetCDirty();

        //TODO call self listeners
        for (auto & oListener : vListeners) {
                std::visit([this](auto * pComponent) {

                        if constexpr (TEvaluateCond< typename std::remove_pointer<decltype(pComponent)>::type >) {
                                pComponent->TargetTransformChanged(this);
                        }
                        else {
                                log_e("wrong component: '{}', node: '{}'", typeid(pComponent).name(), sName);
                                se_assert(0);
                        }
                },
                oListener);
        }

        for (auto & item : vChildren) {
                item->oTransform.Invalidate();
                item->InvalidateChildren();
        }
}

template <class ... TComponents >
        template <class TComponent>
                void SceneNode<TComponents ...>::AddListener(TComponent * pComponent) {

        static_assert(TEvaluateCond< TComponent >, "concreate Component must have 'TargetTransformChanged' method");//FIXME

        bool found = false;

        for (auto & oItem : vListeners) {

                std::visit([&found, pComponent](auto * pComponentItem) {

                        if constexpr (std::is_same_v<TComponent, std::decay<decltype(pComponentItem)>>) {
                                if (pComponentItem == pComponent) {
                                        found = true;
                                }
                        }
                },
                oItem);

                if (found) {
                        return;
                }
        }

        vListeners.emplace_back(pComponent);

        if (oTransform.CDirty()) {
                pComponent->TargetTransformChanged(this);
        }
}

template <class ... TComponents >
        template <class TComponent>
                void SceneNode<TComponents ...>::RemoveListener(TComponent * pComponent) {

        static_assert(TEvaluateCond< TComponent >, "concreate Component must have 'TargetTransformChanged' method");//FIXME

        if (vListeners.empty()) {
                return;
        }

        auto it = std::find_if(vListeners.begin(), vListeners.end(),
                        [pComponent](auto & oItem) {
                                auto pVal = std::get_if<TComponent *>(&oItem);

                                return pVal && (*pVal == pComponent);
                        });

        if (it != vListeners.end()) {
                if (*it != vListeners.back()) {
                        *it = vListeners.back();
                }
                vListeners.pop_back();
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
        else if (!(internal_flags & STATE_UNLINKED)) {
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
                name_id   = sNewName;
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

         for (auto & pChild : vChildren) {
                oHandler(*pChild);
         }
         for (auto & pChild : vChildren) {
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
                auto pComponent = std::make_unique<TComponent>(this, std::forward <TArgs>(oArgs)...);
                if (internal_flags & STATE_ENABLED) {
                        pComponent->Enable();
                }

                vComponents.emplace_back(std::move(pComponent));
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
        void SceneNode<TComponents...>::EnableRecursive() {

        for (auto & pChild : vChildren) {
                pChild->EnableRecursive();
        }
        Enable();
}

template <class ... TComponents>
        void SceneNode<TComponents...>::DisableRecursive() {

        for (auto & pChild : vChildren) {
                pChild->DisableRecursive();
        }
        Disable();
}

template <class ... TComponents>
        bool SceneNode<TComponents...>::Enabled() const {

        return internal_flags & STATE_ENABLED;
}

template <class ... TComponents>
        void SceneNode<TComponents...>::ToggleEnabled() {

        if (Enabled()) {
                Disable();
        }
        else {
                Enable();
        }
}

template <class ... TComponents>
        std::shared_ptr<SceneNode<TComponents...>> SceneNode<TComponents...>::GetShared() {
        return this->shared_from_this();
}

template <class ... TComponents>
        std::shared_ptr<SceneNode<TComponents...>> SceneNode<TComponents...>::FindChild(
                        const StrID target_name_id, bool recursive) const {

        TSceneNode      pResult;

        for (auto & pChild : vChildren) {

                if (pChild->name_id == target_name_id) {
                        pResult = pChild;
                        break;
                }

                if (recursive) {
                        pResult = pChild->FindChild(target_name_id, recursive);
                        if (pResult) {
                                break;
                        }
                }
        }

        return pResult;
}

template <class ... TComponents>
        void SceneNode<TComponents...>::DrawDebug() const {

        GetSystem<DebugRenderer>().DrawLocalAxes(oTransform);

        for (auto & oComponent : vComponents) {

                std::visit([](auto && arg) {
                        arg->DrawDebug();
                },
                oComponent);
        }
}

}

