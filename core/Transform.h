#ifndef __TRANSFORM_H__
#define __TRANSFORM_H__ 1

namespace SE {

/**
 front  -> -z
 up     -> +y
 right  -> +x
*/
static const glm::vec3 TransformUp   { 0.0f, 1.0f, 0.0f };
static const glm::vec3 TransformFront{ 0.0f, 0.0f, -1.0f };
static const glm::vec3 TransformRight{ 1.0f, 0.0f, 0.0f };

class Transform {

        Transform             * pParent;
        mutable glm::mat4       mTransform;
        mutable glm::mat4       mWorldTransform;
        glm::vec3               vTranslation;
        glm::vec3               vScale;
        glm::quat               qRotation;
        mutable uint8_t         local_dirty : 1,
                                world_dirty : 1,
                                childs_notified : 1;//THINK

        void              Recalc() const;
        void              RecalcWorld() const;

        public:


        Transform();
        Transform(const glm::vec3 & pos, const glm::vec3 & rotation, const glm::vec3 & new_scale);

        void              Set(const glm::vec3 & pos, const glm::vec3 & rotation, const glm::vec3 & new_scale);
        void              SetPos(const glm::vec3 & vPos);
        void              SetRotation(const glm::vec3 & vDegreeAngles);
        void              SetRotation(const glm::quat & qNewRotation);
        void              SetScale(const glm::vec3 & new_scale);
        void              SetWorldPos(const glm::vec3 & vWorldPos);
        void              SetWorldRotation(const glm::vec3 & vDegreeAngles);
        const glm::mat4 & Get() const;
        const glm::mat4 & GetWorld() const;
        const glm::mat4 & GetParentWorld() const;
        const glm::vec3 & GetPos() const;
        const glm::vec3   GetRotationDeg() const;
        const glm::quat & GetRotation() const;
        const glm::vec3 & GetScale() const;
        void              Invalidate();
        void              SetParent(Transform * pNode);
        void              Print(const size_t indent);
        void              Translate(const glm::vec3 & vPos);
        void              Rotate(const glm::vec3 & vDegreeAngles);
        void              Scale(const glm::vec3 & vNewScale);
        void              TranslateLocal(const glm::vec3 & vPos);
        void              RotateLocal(const glm::vec3 & vDegreeAngles);
        /** @brief scale around point in local space */
        void              ScaleWithPivot(const glm::vec3 & vPoint, const glm::vec3 & vNewScale);
        /** @brief rotate around point in local space */
        void              RotateAround(const glm::vec3 & vPoint, const glm::vec3 & vDegreeAngles);
        void              RotateAround(const glm::vec3 & vPoint, const glm::quat & qDeltaRotation);
        void              LookAt(const glm::vec3 & vLocalPoint, const glm::vec3 & vUp = TransformUp);
        void              WorldLookAt(const glm::vec3 & vWorldPoint, const glm::vec3 & vUp = TransformUp);
        std::tuple<
                glm::vec3,
                glm::vec3,
                glm::vec3>
                          GetWorldDecomposedEuler() const;
        std::tuple<
                glm::vec3,
                glm::quat,
                glm::vec3>
                          GetWorldDecomposedQuat() const;
        void SetCDirty();
        bool CDirty() const;
        glm::vec3         GetWorldPos() const;
        std::string       Str() const;
        std::string       StrDump(const size_t indent) const;
};

} //namespace SE
#endif
