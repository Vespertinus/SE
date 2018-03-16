
namespace SE  {


//template <class ... TGeom > SceneNode<TGeom ...>::SceneNode() : SceneNode(nullptr, "") {;;}

template <class ... TGeom > SceneNode<TGeom ...>::SceneNode(TSceneNode * pParentNode, const std::string_view sNewName, TSceneTree * pNewScene) :
        pParent(nullptr),
        sName(sNewName),
        pScene(pNewScene) {

        SetParent(pParentNode);
}

template <class ... TGeom > void SceneNode<TGeom ...>::
        SetPos(const glm::vec3 & vPos) {

        oTransform.SetPos(vPos);
        InvalidateChildren();
}

template <class ... TGeom > void SceneNode<TGeom ...>::
        SetRotation(const glm::vec3 & vDegreeAngles) {

        oTransform.SetRotation(vDegreeAngles);
        InvalidateChildren();
}

template <class ... TGeom > void SceneNode<TGeom ...>::
        SetScale(const glm::vec3 & new_scale) {

        oTransform.SetScale(new_scale);
        InvalidateChildren();
}


template <class ... TGeom > void SceneNode<TGeom ...>::
        SetParent(TSceneNode * pNewParent) {

        if (pNewParent) {
                pNewParent->AddChild(this);
        }
        else {
                oTransform.SetParent(nullptr);
        }
        RebuildFullName();
}

template <class ... TGeom > void SceneNode<TGeom ...>::
        AddChild(TSceneNode * pNode) {

        if (!pNode || pNode == this || pNode->pParent == this) {
                log_w("failed to add, node ptr = {:p} cur node = {:p}, parent = {:p}", (void *)pNode, (void *)this, (pNode) ? (void *)pNode->pParent : nullptr);
                return;
        }

        if (pParent && pParent == pNode) {
                log_w("failed to add, prevent cyclic link, parent = {:p}, node = {:p}", (void *)pParent, (void *)pNode);
                return;
        }

        TSceneNode * pOldParent = pNode->pParent;
        if (pOldParent) {

                pOldParent->RemoveChild(pNode);
        }

        pNode->pParent = this;
        pNode->oTransform.SetParent(&oTransform);
        pNode->RebuildFullName();

        vChildren.emplace_back(pNode);

        log_d("children cnt = {}", vChildren.size());
}


template <class ... TGeom >
        template <class T>
                void SceneNode<TGeom ...>::
                        AddRenderEntity(T oRenderEntity) {

        vRenderEntity.emplace_back(oRenderEntity);
        oRenderEntity->SetTransform(&oTransform); //TODO . ->

        log_d("render entity added, cnt = {}", vRenderEntity.size());
}


template <class ... TGeom > void SceneNode<TGeom ...>::
        InvalidateChildren() {

        for (auto & item : vChildren) {
                item->oTransform.Invalidate();
                item->InvalidateChildren();
        }
}

template <class ... TGeom > const std::string & SceneNode<TGeom ...>::
        GetName() const {

        return sName;
}

template <class ... TGeom > const std::string & SceneNode<TGeom ...>::
        GetFullName() const {

        return sFullName;
}


template <class ... TGeom > void SceneNode<TGeom ...>::
        RemoveChild(TSceneNode * pNode) {

        vChildren.erase(std::remove(vChildren.begin(), vChildren.end(), pNode));
}

template <class ... TGeom > void SceneNode<TGeom ...>::
        Print(const size_t indent) {

        log_d("{:>{}} '{}': entity cnt = {}", ">", indent, sFullName, vRenderEntity.size());
        oTransform.Print(indent);

        for (auto * item : vChildren) {
                item->Print(indent + 4);
        }
}

template <class ... TGeom > bool SceneNode<TGeom ...>::
        SetName(const std::string_view sNewName) {

        std::string sNewFullName;
        BuildFullName(sNewFullName, sNewName);

        bool res = pScene->UpdateNodeName(this, sNewName, sNewFullName);
        if (res) {
                sName     = sNewName;
                sFullName = sNewFullName;
        }

        return res;
}

template <class ... TGeom > SceneTree<TGeom...> * SceneNode<TGeom ...>::
        GetScene() const {

        return pScene;
}

//TODO rewrite on property based visitor, via enable_if + is_renderable etc
template <class ... TGeom > void SceneNode<TGeom ...>::
        Draw() const {

        if (vRenderEntity.size()) {
                const auto & world_mat = oTransform.GetWorld();
                glPushMatrix();
                glMultMatrixf(glm::value_ptr(world_mat));

                for (auto & oEntity : vRenderEntity) {

                        std::visit([](auto && arg) {
                                        arg->Draw();
                                        },
                                        oEntity);

                }

                glPopMatrix();
        }

        for (auto * pChild: vChildren) {
                pChild->Draw();
        }
}

template <class ... TGeom > void SceneNode<TGeom ...>::
        BuildFullName(std::string & sNewFullName, const std::string_view sNewName) {

        if (pParent) {
                sNewFullName = pParent->GetFullName() + "|" + sNewName.data();
        }
        else {
                sNewFullName = sNewName;
        }
}


template <class ... TGeom > void SceneNode<TGeom ...>::
        RebuildFullName() {

        BuildFullName(sFullName, sName);
}

template <class ... TGeom >
        template <class T>
                T * SceneNode<TGeom ...>::GetEntity(const size_t index) {

        if (index >= vRenderEntity.size()) {
                log_w("index '{}' exceed entity count '{}', node '{}'",
                                index,
                                vRenderEntity.size(),
                                sFullName);
                return nullptr;
        }

        return std::get<T *>(vRenderEntity[index]);
}


}
