
#ifndef __BOUNDING_BOX_H__
#define __BOUNDING_BOX_H__ 1

namespace SE {

enum IntersectionState {

        In,
        Out,
        Cross
};

class BoundingBox {

        glm::vec3       vMin;
        glm::vec3       vMax;

        public:
        BoundingBox();
        BoundingBox(const glm::vec3 & vNewMin, const glm::vec3 vNewMax);
        BoundingBox(std::vector<float> & vVertices, const uint8_t elem_size);

        bool                    operator ==(const BoundingBox & oRhs) const;
        bool                    operator !=(const BoundingBox & oRhs) const;

        void                    Concat(const glm::vec3 & vPoint);
        void                    Concat(const BoundingBox & oBBox);
        glm::vec3               Center() const;
        glm::vec3               Size() const;
        void                    Transform(const glm::mat4 & mTransform);
        BoundingBox             Transformed(const glm::mat4 & mTransform) const;
        IntersectionState       Intersect(const glm::vec3 & vPoint) const;
        IntersectionState       Intersect(const BoundingBox & oBBox) const;
        void                    Calc(std::vector<float> & vVertices, const uint8_t elem_size);
        void                    Calc(const float * pVertices, const uint32_t size, const uint8_t elem_size);
        const glm::vec3 &       Min() const;
        const glm::vec3 &       Max() const;
        void                    Set(const float * pMin, const float * pMax);
        std::string             Str() const;
        bool                    Ready() const;

};

} //namespace SE

#endif
