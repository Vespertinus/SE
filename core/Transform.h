#ifndef __TRANSFORM_H__
#define __TRANSFORM_H__ 1

namespace SE {

class Transform {

        Transform             * pParent;
        mutable glm::mat4       mTransform;
        mutable glm::mat4       mWorldTransform;
        glm::vec3               vTranslation;
        glm::vec3               vRotation;
        glm::vec3               vScale;
        mutable uint8_t         local_dirty : 1,
                                world_dirty : 1;

        void              Recalc() const;
        void              RecalcWorld() const;

        public:

        Transform();
        Transform(const glm::vec3 & pos, const glm::vec3 & rotation, const glm::vec3 & new_scale);

        void              Set(const glm::vec3 & pos, const glm::vec3 & rotation, const glm::vec3 & new_scale);
        void              SetPos(const glm::vec3 & vPos);
        void              SetRotation(const glm::vec3 & vDegreeAngles);
        void              SetScale(const glm::vec3 & new_scale);
        const glm::mat4 & Get() const;
        const glm::mat4 & GetWorld() const;
        void              Invalidate();
        void              SetParent(Transform * pNode);
        void              Print(const size_t indent);
};

} //namespace SE

#ifdef SE_IMPL
#include <Transform.tcc>
#endif

#endif
