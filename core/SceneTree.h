#ifndef __SCENE_TREE_H__
#define __SCENE_TREE_H__ 1

#include <SceneNode.h>
#include <SceneTree_generated.h>

namespace SE {

//TODO as resource.. load from file
template <class ... TGeom > class SceneTree : public ResourceHolder {

        using TSceneNode = SceneNode<TGeom...>;

        TSceneNode oRoot;

        std::unordered_map<std::string_view, TSceneNode *> mNamedNodes;

        void Load();
        void Load(const SE::FlatBuffers::Node * pRoot);
        ret_code_t LoadNode(const SE::FlatBuffers::Node * pSrcNode, TSceneNode * pDstNode);


        public:

        SceneTree(const std::string & sName, const rid_t new_rid);
        ~SceneTree() noexcept;

        TSceneNode * Create(const std::string_view sNewName = "");
        TSceneNode * Create(TSceneNode * pParent, const std::string_view sNewName = "");
        //TSceneNode * CloneNode(TSceneNode * pNode);
        TSceneNode * Find(const std::string_view sName) const;
        TSceneNode * GetRoot() const;
        //Apply (visitor, recursive = false)
        //Apply (visitor, Node, recursive = false)
        bool         Destroy(TSceneNode * pNode);
        bool         Destroy(const std::string_view sName);
        void         Print();
        bool         UpdateNodeName(TSceneNode * pNode, const std::string_view sOldName, const std::string_view sNewName);
        void         Draw() const;

};


} //namespace SE

#include <SceneTree.tcc>

#endif
