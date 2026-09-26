
#ifndef APP_NAV_NAV_LINK_H
#define APP_NAV_NAV_LINK_H 1

#include <string>

#include <glm/glm.hpp>

#include <Component_generated.h>

namespace SE {

// ---------------------------------------------------------------------------
// NavLinkDesc — programmatic construction (tests, tools).
// ---------------------------------------------------------------------------

struct NavLinkDesc {
        std::string end_node;             // target node name (same sub-level)
        glm::vec3   end{0.f};             // fallback end point, node-local offset
        float       radius        = 0.5f;
        std::string area;
        bool        bidirectional = true;
};

// ---------------------------------------------------------------------------
// NavLink — authored off-mesh connection starting at the node
// position (jump-down, ladder, door).
//
// Inert marker, exactly like NavArea: tools/nav_bake embeds the traversable
// connection polys into the owning group's tile bytes at bake time; this
// component carries the authored record (debug renderer reads it).
// The bake resolves `end_node` to the sibling node's position; `end` is the
// fallback node-local offset. At least one must resolve — the bake validates.
//
// Standard component shape (SpatialEntry precedent); Enable()/Disable() are
// no-ops.
// ---------------------------------------------------------------------------

class NavLink {
public:
        using TSerialized = FlatBuffers::NavLink;

        NavLink(TSceneTree::TSceneNodeExact * pNode, const NavLinkDesc & desc);
        NavLink(TSceneTree::TSceneNodeExact * pNode, const FlatBuffers::NavLink * pFB);
        ~NavLink() noexcept;

        void Enable();
        void Disable();

        const std::string & EndNode() const { return oDesc.end_node; }
        const glm::vec3 &   End() const { return oDesc.end; }
        float               Radius() const { return oDesc.radius; }
        const std::string & Area() const { return oDesc.area; }
        bool                Bidirectional() const { return oDesc.bidirectional; }

        std::string Str() const;

private:
        TSceneTree::TSceneNodeExact * pNode = nullptr;
        NavLinkDesc                   oDesc;
};

} // namespace SE

#endif
