#ifndef __SCENE_TREE_H__
#define __SCENE_TREE_H__ 1

#include <SceneNode.h>

namespace SE {

//TODO as resource.. load from file
template <class ... TGeom > class SceneTree {

        using TSceneNode = SceneNode<TGeom...>;

        TSceneNode oRoot;

        std::unordered_map<std::string_view, TSceneNode *> mNamedNodes;


        public:
        SceneTree(); //TODO rewrite for Resource interface
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

};


} //namespace SE

#include <SceneTree.tcc>

#endif
