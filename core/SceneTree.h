#ifndef __SCENE_TREE_H__
#define __SCENE_TREE_H__ 1

#include <SceneNode.h>
#include <SceneTree_generated.h>

namespace SE {

template <class ... TGeom > class SceneTree : public ResourceHolder {

        public:

        //TODO rewrite on TGeom::Settings holder for each type of settings
        struct Settings {
                //StoreTexture2D::Settings;
                MeshSettings oMeshSettings;
        };

        private:

        using TSceneNode = SceneNode<TGeom...>;

        TSceneNode                              oRoot;
        std::unordered_map<StrID, TSceneNode *> mNamedNodes;

        void Load(const Settings & oSettings);
        void Load(const SE::FlatBuffers::Node * pRoot, const Settings & oSettings);
        ret_code_t LoadNode(const SE::FlatBuffers::Node * pSrcNode,
                            TSceneNode * pDstNode,
                            const Settings & oSettings);

        public:

        SceneTree(const std::string & sName, const rid_t new_rid, const Settings & oSettings = {});
        ~SceneTree() noexcept;

        TSceneNode * Create(const std::string_view sNewName = "");
        TSceneNode * Create(TSceneNode * pParent, const std::string_view sNewName = "");
        //TSceneNode * CloneNode(TSceneNode * pNode);
        TSceneNode * Find(const std::string_view sName) const;
        TSceneNode * Find(const StrID sid) const;
        TSceneNode * GetRoot() const;
        //Apply (visitor, recursive = false)
        //Apply (visitor, Node, recursive = false)
        bool         Destroy(TSceneNode * pNode);
        bool         Destroy(const std::string_view sName);
        void         Print();
        bool         UpdateNodeName(TSceneNode * pNode,
                                    const std::string_view sNewName,
                                    const std::string_view sNewFullName);
        void         Draw() const;

};


} //namespace SE

#include <SceneTree.tcc>

#endif
