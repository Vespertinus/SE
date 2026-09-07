#ifndef __SCENE_TREE_H__
#define __SCENE_TREE_H__ 1

#include <SceneNode.h>
#include <SceneTree_generated.h>

namespace SE {

template <class ... TComponents > class SceneTree : public ResourceHolder {

        public:

        using TSceneNodeExact   = SceneNode<TComponents...>;
        using TSceneNode        = std::shared_ptr<TSceneNodeExact>;
        using TSceneNodeWeak    = std::weak_ptr<TSceneNodeExact>;

        private:

        struct NodeWrapper : public TSceneNodeExact {

                template <class ... TArgs> NodeWrapper(TArgs && ... oArgs) : TSceneNodeExact(oArgs...) { ;; }
        };


        std::string                             sLoadPath;
        TSceneNode                              pRoot;
        std::unordered_map<StrID, TSceneNode>   mNamedNodes;
        std::unordered_map<
                StrID,
                std::vector<TSceneNode>
                        >                       mLocalNamedNodes;

        void Load();
        // Reads + verifies a scene file, returning its bytes. The returned FlatBuffer
        // pointers are valid only while the buffer is alive. Throws on error.
        std::vector<char> ReadSceneFile(const std::string & sFilePath);
        // Builds the component loaders, loads the FB hierarchy under pParent
        // (nullptr → replace this tree's root), runs the PostLoad pass, and returns
        // the instance-root node that was created/updated.
        TSceneNode LoadFB(const SE::FlatBuffers::Node * pRootFB, TSceneNode pParent);
        template <class TMap, class TVec> ret_code_t LoadNode(
                        const SE::FlatBuffers::Node * pSrcNode,
                        TSceneNode pParent,
                        const TMap & mLoaders,
                        TVec & vPostLoadComponents,
                        TSceneNode * pOutCreated = nullptr);

        public:

        SceneTree(const std::string & sName, const rid_t new_rid, bool empty = false, std::string sFilePath = {});
        ~SceneTree() noexcept;

        TSceneNode      Create(const std::string_view sNewName = "", const bool enabled = true);
        TSceneNode      Create(TSceneNode pParent, const std::string_view sNewName = "", const bool enabled = true);

        // Instantiate a prefab: load sFilePath's node hierarchy directly into THIS tree
        // as a child of pParent (nullptr → scene root). Returns the created instance-root
        // node, or nullptr on failure. No separate TSceneTree resource is created.
        TSceneNode      Instantiate(const std::string & sFilePath, TSceneNode pParent = nullptr);
        //TSceneNode * CloneNode(TSceneNode * pNode);
        TSceneNode      FindFullName(const StrID sid) const;
        const std::vector <TSceneNode> *
                        FindLocalName(const StrID sid) const;
        TSceneNode      GetRoot();
        //TODO Destroy recursive
        void            HandleNodeUnlink(TSceneNodeExact * pNode);
        //HandleNodeAdd ??? call in AddChild
        void            Print();
        bool            UpdateNodeName(TSceneNode pNode,
                                       const std::string_view sNewName,
                                       const std::string_view sNewFullName);
        //void         Draw() const;//???component based.. -> SFINAE check per component method Draw and DrawDDebug
        void            EnableAll();
        void            DisableAll();
};


} //namespace SE

#include <SceneTree.tcc>

#endif
