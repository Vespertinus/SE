
#ifndef __SCENE_NODE_H__
#define __SCENE_NODE_H__ 1

#include <vector>
#include <variant>

#include <Transform.h>

namespace SE {

template <class ... TGeom> class SceneNode {

        using TSceneNode = SceneNode<TGeom...>;
        using TVariant = std::variant<TGeom...>;

        template <class ... TArgs> friend class SceneTree;

        std::vector<TVariant>                   vRenderEntity;
        //std::vector<...>                      vOtherEntity; //TODO camera, dummy etc
        SceneNode<TGeom...>                   * pParent;
        std::vector<SceneNode<TGeom...> * >     vChildren;
        Transform                               oTransform;
        std::string                             sName;

        //SceneNode(); //THINK ???
        SceneNode(TSceneNode * pParentNode, const std::string_view sNewName);
        //~SceneNode() noexcept TODO

        void                    SetParent(TSceneNode * pNewParent);
        void                    InvalidateChildren();

        public:


        void                    Draw() const; //THINK maybe external;
        //void    Apply() const; only apply transformation..
        void                    SetPos(const glm::vec3 & vPos);
        void                    SetRotation(const glm::vec3 & vDegreeAngles);
        void                    SetScale(const float new_scale);
        //void     DrawBBox() const;
        uint32_t                GetGeomCnt() const;
        template <class T> void AddRenderEntity(T oRenderEntity);
        void                    AddChild(TSceneNode * pNode);
        void                    RemoveChild(TSceneNode * pNode);
        //TODO destroy all childs
        const std::string &     GetName() const;
        //void                    SetName();
        void                    Print(const size_t indent);

        //template <class T> void Apply(T & functor);
        /*      TODO
                SetRotation(glm::quaternion)
                enabled \ disabled
                id for search
                Add / Remove entity
                coordinate type local, global, ...
                get children vec..
        */
};

} //namespace SE

#include <SceneNode.tcc>

#endif
