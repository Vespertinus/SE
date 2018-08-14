
#ifndef __SCENE_NODE_H__
#define __SCENE_NODE_H__ 1

#include <vector>
#include <variant>

#include <Transform.h>

namespace SE {

template <class ... TArgs> class SceneTree;

template <class ... TGeom> class SceneNode {

        using TSceneNode = SceneNode<TGeom...>;
        using TVariant   = std::variant<TGeom...>;
        using TSceneTree = SceneTree<TGeom...>;

        template <class ... TArgs> friend class SceneTree;

        std::vector<TVariant>                   vRenderEntity;
        //std::vector<...>                      vOtherEntity; //TODO camera, dummy etc
        SceneNode<TGeom...>                   * pParent;
        std::vector<SceneNode<TGeom...> * >     vChildren;
        Transform                               oTransform;
        std::string                             sName;
        std::string                             sFullName;
        std::string                             sCustomInfo;
        TSceneTree                            * pScene;

        //SceneNode(); //THINK ???
        SceneNode(TSceneNode * pParentNode, const std::string_view sNewName, TSceneTree * pNewScene);
        //~SceneNode() noexcept TODO

        void                    SetParent(TSceneNode * pNewParent);
        void                    InvalidateChildren();
        void                    RebuildFullName();
        void                    BuildFullName(std::string & sNewFullName,
                                              const std::string_view sNewName);
        template <class THandler> void BreadtFirstWalkChild(THandler && oHandler);

        public:


        void                    DrawRecursive() const; //THINK maybe external;
        void                    DrawSelf() const;
        //void    Apply() const; only apply transformation..
        void                    SetPos(const glm::vec3 & vPos);
        void                    SetRotation(const glm::vec3 & vDegreeAngles);
        void                    SetScale(const glm::vec3 & new_scale);
        void                    Translate(const glm::vec3 & vPos);
        void                    Rotate(const glm::vec3 & vDegreeAngles);
        void                    Scale(const glm::vec3 & new_scale);
        //void     DrawBBox() const;
        uint32_t                GetEntityCnt() const;
        template <class T> void AddRenderEntity(T oRenderEntity);
        void                    AddChild(TSceneNode * pNode);
        void                    RemoveChild(TSceneNode * pNode);
        //TODO destroy all childs
        const std::string &     GetName() const;
        const std::string &     GetFullName() const;
        bool                    SetName(std::string_view sNewName);
        void                    Print(const size_t indent, bool recursive = true);
        TSceneTree            * GetScene() const;
        template <class T>  T * GetEntity(const size_t index);
        const Transform       & GetTransform() const;
        void                    SetCustomInfo(const std::string_view sInfo);
        const std::string     & GetCustomInfo() const;

        /*      TODO
                SetRotation(glm::quaternion)
                enabled \ disabled
                id for search
                Add / Remove entity
                coordinate type local, global, ...
                get children vec..

                unlink from scene
                SetScene -> move to another scene
        */

        template <class THandler> void BreadtFirstWalk(THandler && oHandler);
        template <class THandler> void DepthFirstWalk(THandler && oHandler);
};

} //namespace SE

#include <SceneNode.tcc>

#endif
