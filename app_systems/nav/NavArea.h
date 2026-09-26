
#ifndef APP_NAV_NAV_AREA_H
#define APP_NAV_NAV_AREA_H 1

#include <string>

#include <glm/glm.hpp>

#include <Component_generated.h>

namespace SE {

// ---------------------------------------------------------------------------
// NavAreaDesc — programmatic construction (tests, tools).
// ---------------------------------------------------------------------------

struct NavAreaDesc {
        glm::vec3   size{0.f};   // full extents of the box, centered on the node
        std::string area;        // "" = exclusion volume (nav hole)
};

// ---------------------------------------------------------------------------
// NavArea — authoring marker consumed by tools/nav_bake.
//
// Inert at runtime by design: the offline bake reads these from the .sesc
// FlatBuffers and encodes areas/exclusions into the baked tile bytes; 
// the component only carries the data (debug draw will read it too).
//
// It exists as a runtime component at all because SceneTree instantiation
// hard-fails on component types absent from the app's component typelist
// (core/SceneTree.tcc LoadNode) — sub-levels authored with nav markers must
// remain loadable by any app that opts into the marker components.
//
// Standard component shape (SpatialEntry precedent): constructed from the
// FlatBuffers table during scene load, or from NavAreaDesc. Enable()/Disable()
// are no-ops; no subscriptions, no system dependency.
// ---------------------------------------------------------------------------

class NavArea {
public:
        using TSerialized = FlatBuffers::NavArea;

        NavArea(TSceneTree::TSceneNodeExact * pNode, const NavAreaDesc & desc);
        NavArea(TSceneTree::TSceneNodeExact * pNode, const FlatBuffers::NavArea * pFB);
        ~NavArea() noexcept;

        void Enable();
        void Disable();

        const glm::vec3 &  Size() const { return oDesc.size; }
        const std::string & Area() const { return oDesc.area; }
        bool               IsExclusion() const { return oDesc.area.empty(); }

        std::string Str() const;

private:
        TSceneTree::TSceneNodeExact * pNode = nullptr;
        NavAreaDesc                   oDesc;
};

} // namespace SE

#endif
