
#ifdef SE_IMPL

#include <nav/NavArea.h>

#include <Logging.h>

#include <stdexcept>

namespace SE {

// ---------------------------------------------------------------------------

NavArea::NavArea(TSceneTree::TSceneNodeExact * pNewNode, const NavAreaDesc & desc)
        : pNode(pNewNode)
        , oDesc(desc) {

        if (oDesc.size.x <= 0.f || oDesc.size.y <= 0.f || oDesc.size.z <= 0.f) {
                throw std::runtime_error("NavArea: size components must be positive");
        }
}

NavArea::NavArea(TSceneTree::TSceneNodeExact * pNewNode, const FlatBuffers::NavArea * pFB)
        : pNode(pNewNode) {

        if (pFB->size() == nullptr) {
                throw std::runtime_error("NavArea: size is required");
        }
        if (pFB->area() != nullptr) {
                oDesc.area = pFB->area()->str();
        }

        oDesc.size = glm::vec3(pFB->size()->x(), pFB->size()->y(), pFB->size()->z());

        if (oDesc.size.x <= 0.f || oDesc.size.y <= 0.f || oDesc.size.z <= 0.f) {
                throw std::runtime_error("NavArea: size components must be positive");
        }
}

NavArea::~NavArea() noexcept {
        Disable();
}

// ---------------------------------------------------------------------------

void NavArea::Enable() {
        // inert — baked offline, no runtime registration
}

void NavArea::Disable() {
}

// ---------------------------------------------------------------------------

std::string NavArea::Str() const {
        return IsExclusion() ? "NavArea(exclude)" : "NavArea('" + oDesc.area + "')";
}

} // namespace SE

#endif
