#ifndef SKELETON_H
#define SKELETON_H

#include <Component_generated.h>

/**
 THINK modern skeleton contains up to 800 joints
 uint8_t not enough
 */

namespace SE {

struct Joint {

        //StrID                           name_id;
        std::string     sName;
/*        glm::quat       bind_qrot;
        glm::vec3       bind_pos;
        glm::vec3       bind_scale;*/
        uint8_t         parent_ind{0};
};

//hold bones list and maping to bone nodes
class Skeleton : public ResourceHolder {

        std::vector<Joint> vJoints;

        public:
        Skeleton(const std::string & sName,
                 const rid_t new_rid,
                 const SE::FlatBuffers::Skeleton * pSkeleton);

        Skeleton(const std::string & sName,
                 const rid_t new_rid);

        uint8_t GetJointsCnt() const;
        const std::vector<Joint> & Joints() const;
        //Compare
};

class CharacterShell : public ResourceHolder {

        Skeleton      * pSkeleton{};
        std::vector<TSceneTree::TSceneNodeWeak> vJointNodes;

        public:
        CharacterShell(const std::string & sName,
                       const rid_t new_rid,
                       const SE::FlatBuffers::CharacterShell * pShell,
                       TSceneTree::TSceneNodeExact * pTargetNode);
        CharacterShell(
                       const std::string & sName,
                       const rid_t new_rid,
                       TSceneTree::TSceneNodeExact * pTargetNode );

        Skeleton * GetSkeleton() const;
        const std::vector<TSceneTree::TSceneNodeWeak> & JointNodes();
        /** TODO
         build from new root node
         re assign skeleton
         */
};

}

#endif
