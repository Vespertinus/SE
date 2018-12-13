
#ifndef __SCENE_NODE_H__
#define __SCENE_NODE_H__ 1

#include <vector>
#include <variant>
#include <memory>

#include <Transform.h>

namespace SE {

template <class ... TArgs> class SceneTree;

template <class ... TComponents> class SceneNode : public std::enable_shared_from_this<SceneNode<TComponents...> > {

        using TSceneNodeExact   = SceneNode<TComponents...>;
        using TSceneNode        = std::shared_ptr<TSceneNodeExact>;
        //TODO rewrite on separate per component storage
        using TVariant          = std::variant<std::unique_ptr<TComponents> ...>;
        using TSceneTree        = SceneTree<TComponents...>;

        template <class ... TArgs> friend class SceneTree;

        static const uint8_t                    STATE_ENABLED  = 0x1;
        static const uint8_t                    STATE_UNLINKED = 0x2;

        std::vector<TVariant>                   vComponents;
        TSceneNodeExact                       * pParent;
        std::vector<TSceneNode>                 vChildren;
        Transform                               oTransform;
        std::string                             sName;
        std::string                             sFullName;
        std::string                             sCustomInfo; //TODO as custom info component
        TSceneTree                            * pScene;
        /** user state flags */
        uint8_t                                 user_flags;
        uint8_t                                 internal_flags;

        protected:
        SceneNode(const std::string_view sNewName,
                  TSceneTree * pNewScene);
        ~SceneNode() noexcept;

        ret_code_t              SetParent(TSceneNodeExact * pNewParent);
        void                    InvalidateChildren();
        void                    RebuildFullName();
        void                    BuildFullName(std::string & sNewFullName,
                                              const std::string_view sNewName,
                                              TSceneNodeExact * pNewParent);
        template <class THandler> void BreadtFirstWalkChild(THandler && oHandler);

        public:

        void                    SetPos(const glm::vec3 & vPos);
        void                    SetRotation(const glm::vec3 & vDegreeAngles);
        void                    SetRotation(const glm::quat & qNewRotation);
        void                    SetScale(const glm::vec3 & new_scale);
        void                    Translate(const glm::vec3 & vPos);
        void                    Rotate(const glm::vec3 & vDegreeAngles);
        void                    Scale(const glm::vec3 & new_scale);
        uint32_t                GetComponentsCnt() const;
        ret_code_t              AddChild(TSceneNode pNode);
        void                    RemoveChild(TSceneNode pNode);
        void                    Unlink();
        const std::string &     GetName() const;
        const std::string &     GetFullName() const;
        bool                    SetName(std::string_view sNewName);
        void                    Print(const size_t indent, bool recursive = true);
        TSceneTree            * GetScene() const;
        const Transform       & GetTransform() const;
        void                    SetCustomInfo(const std::string_view sInfo);
        const std::string     & GetCustomInfo() const;
        TSceneNode              GetShared() const;

        /*      TODO
                SetRotation(glm::quaternion)
                id for search
                Add / Remove entity
                coordinate type local, global, ...
                get children vec..

                unlink from scene
                SetScene -> move to another scene

                THINK
                unlink link to another scene
                node state when it unlinked and old scene deleted...? currently broken!!!

                component kind \ interface type
                weak_ptr to all external methods.. shared_ptr only inside SceneTree...?
                -- shared_ptr could be used external - atleast weak_ptr to shared, so,
                storing SceneNode outside allowed only as weak_ptr

                component OnDestroy ? or only Disable

                split remove \ add api on internal TSceneNodeExact and public using TSceneNode
                -- public only AddChild and Unlink?

                TODO UpdateNodeName on every link changes..
        */

        void                    RotateAround(const glm::vec3 & vPoint, const glm::vec3 & vDegreeAngles);
        void                    RotateAround(const glm::vec3 & vPoint, const glm::quat & qDeltaRotation);
        void                    SetFlags(const uint8_t state);
        uint8_t                 GetFlags() const;
        void                    ClearFlags();
        void                    ClearFlags(const uint8_t state);

        template <class THandler>       void BreadtFirstWalk(THandler && oHandler);
        template <class THandler>       void DepthFirstWalk(THandler && oHandler);
        template <class THandler, class TPostHandler>
                                        void DepthFirstWalkEx(
                                                        THandler && oHandler,
                                                        TPostHandler && oPostHandler);
        template <class ... THandler> void ForEachComponent(THandler && ... oHandler);
        //TODO return created component
        template <class TComponent, class ... TArgs>
                                        ret_code_t CreateComponent(TArgs && ... oArgs);
        template <class TComponent>     void DestroyComponent();
        template <class TComponent>     TComponent * GetComponent();
        template <class TComponent>     bool HasComponent() const;

        void                    Disable();
        void                    Enable();
};

} //namespace SE

#include <SceneNode.tcc>

#endif
