
#ifdef SE_IMPL

#include <nav/NavLink.h>

#include <Logging.h>

#include <stdexcept>

namespace SE {

// ---------------------------------------------------------------------------

NavLink::NavLink(TSceneTree::TSceneNodeExact * pNewNode, const NavLinkDesc & desc)
        : pNode(pNewNode)
        , oDesc(desc) {

        if (oDesc.end_node.empty() && oDesc.end == glm::vec3(0.f)) {
                throw std::runtime_error("NavLink: end_node or a non-zero end offset is required");
        }
        if (oDesc.radius <= 0.f) {
                throw std::runtime_error("NavLink: radius must be positive");
        }
}

NavLink::NavLink(TSceneTree::TSceneNodeExact * pNewNode, const FlatBuffers::NavLink * pFB)
        : pNode(pNewNode) {

        if (pFB->end_node() != nullptr) {
                oDesc.end_node = pFB->end_node()->str();
        }
        if (pFB->area() != nullptr) {
                oDesc.area = pFB->area()->str();
        }
        if (pFB->end() != nullptr) {
                oDesc.end = glm::vec3(pFB->end()->x(), pFB->end()->y(), pFB->end()->z());
        }

        oDesc.radius        = pFB->radius();
        oDesc.bidirectional = pFB->bidirectional();

        if (oDesc.end_node.empty() && oDesc.end == glm::vec3(0.f)) {
                throw std::runtime_error("NavLink: end_node or a non-zero end offset is required");
        }
        if (oDesc.radius <= 0.f) {
                throw std::runtime_error("NavLink: radius must be positive");
        }
}

NavLink::~NavLink() noexcept {
        Disable();
}

// ---------------------------------------------------------------------------

void NavLink::Enable() {
        // inert — baked offline, no runtime registration
}

void NavLink::Disable() {
}

// ---------------------------------------------------------------------------

std::string NavLink::Str() const {
        return "NavLink('" + oDesc.end_node + "')";
}

} // namespace SE

#endif
