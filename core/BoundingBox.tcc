
namespace SE {

BoundingBox::BoundingBox() :
        vMin(glm::vec3(std::numeric_limits<float>::max())),
        vMax(glm::vec3(std::numeric_limits<float>::lowest())) { ;; }

BoundingBox::BoundingBox(const glm::vec3 & vNewMin, const glm::vec3 vNewMax) :
        vMin(vNewMin), vMax(vNewMax) { ;; }

BoundingBox::BoundingBox(std::vector<float> & vVertices, const uint8_t elem_size) {
        Calc(vVertices, elem_size);
}

//TODO epsilon comparision
bool BoundingBox::operator ==(const BoundingBox & oRhs) const {

        return (vMin == oRhs.vMin && vMax == oRhs.vMax);
}

bool BoundingBox::operator !=(const BoundingBox & oRhs) const {

        return (vMin != oRhs.vMin || vMax != oRhs.vMax);
}

void BoundingBox::Concat(const glm::vec3 & vPoint) {

        vMin.x = std::min(vPoint.x, vMin.x);
        vMin.y = std::min(vPoint.y, vMin.y);
        vMin.z = std::min(vPoint.z, vMin.z);

        vMax.x = std::max(vPoint.x, vMax.x);
        vMax.y = std::max(vPoint.y, vMax.y);
        vMax.z = std::max(vPoint.z, vMax.z);
}

void BoundingBox::Concat(const BoundingBox & oBBox) {

        vMin.x = std::min(oBBox.vMin.x, vMin.x);
        vMin.y = std::min(oBBox.vMin.y, vMin.y);
        vMin.z = std::min(oBBox.vMin.z, vMin.z);

        vMax.x = std::max(oBBox.vMax.x, vMax.x);
        vMax.y = std::max(oBBox.vMax.y, vMax.y);
        vMax.z = std::max(oBBox.vMax.z, vMax.z);
}

glm::vec3 BoundingBox::Center() const {

        return (vMax + vMin) * 0.5f;
}

glm::vec3 BoundingBox::Size() const {

        return vMax - vMin;
}

void BoundingBox::Transform(const glm::mat4 & mTransform) {

        *this = Transformed(mTransform);
}

BoundingBox BoundingBox::Transformed(const glm::mat4 & mTransform) const {

        return BoundingBox(mTransform * glm::vec4(vMin, 1.0), mTransform * glm::vec4(vMax, 1.0));
}

IntersectionState BoundingBox::Intersect(const glm::vec3 & vPoint) const {

        if (vPoint.x < vMin.x ||
            vPoint.x > vMax.x ||
            vPoint.y < vMin.y ||
            vPoint.y > vMax.y ||
            vPoint.z < vMin.z ||
            vPoint.z > vMax.z) {

                return IntersectionState::Out;
        }
        else {
                return IntersectionState::In;
        }
}

IntersectionState BoundingBox::Intersect(const BoundingBox & oBBox) const {

        if (oBBox.vMax.x < vMin.x ||
            oBBox.vMin.x > vMax.x ||
            oBBox.vMax.y < vMin.y ||
            oBBox.vMin.y > vMax.y ||
            oBBox.vMax.z < vMin.z ||
            oBBox.vMin.z > vMax.z) {
                return IntersectionState::Out;
        }
        else if (oBBox.vMin.x < vMin.x ||
                 oBBox.vMax.x > vMax.x ||
                 oBBox.vMin.y < vMin.y ||
                 oBBox.vMax.y > vMax.y ||
                 oBBox.vMin.z < vMin.z ||
                 oBBox.vMax.z > vMax.z) {
                return IntersectionState::Cross;
        }
        else {
                return IntersectionState::In;
        }
}

const glm::vec3 & BoundingBox::Min() const {
        return vMin;
}
const glm::vec3 & BoundingBox::Max() const {
        return vMax;
}

void BoundingBox::Calc(std::vector<float> & vVertices, const uint8_t elem_size) {

        Calc(&vVertices[0], vVertices.size(), elem_size);
}

void BoundingBox::Calc(const float * pVertices, const uint32_t size, const uint8_t elem_size) {

        vMin = glm::vec3(std::numeric_limits<float>::max());
        vMax = glm::vec3(std::numeric_limits<float>::lowest());

        for (uint32_t i = 0; i < size; i += elem_size) {
                vMin.x = std::min(pVertices[i    ], vMin.x);
                vMin.y = std::min(pVertices[i + 1], vMin.y);
                vMin.z = std::min(pVertices[i + 2], vMin.z);

                vMax.x = std::max(pVertices[i    ], vMax.x);
                vMax.y = std::max(pVertices[i + 1], vMax.y);
                vMax.z = std::max(pVertices[i + 2], vMax.z);
        }
}

std::string BoundingBox::Str() const {

        return fmt::format("min ({}, {}, {}), max({}, {}, {})",
                        vMin.x,
                        vMin.y,
                        vMin.z,
                        vMax.x,
                        vMax.y,
                        vMax.z);
}

bool BoundingBox::Ready() const {
        return vMin.x != std::numeric_limits<float>::max();
}

} //namespace SE

