
#ifndef APP_NAV_PATH_FOLLOWER_ENTITY_PLUGIN_H
#define APP_NAV_PATH_FOLLOWER_ENTITY_PLUGIN_H 1

#include <string_view>

// Xlib (reachable through graphics headers in mixed TUs) does `#define Bool
// int` — keep it out of the generated FieldValueU union member.
#pragma push_macro("Bool")
#undef Bool
#include <EntityTemplate_generated.h>
#pragma pop_macro("Bool")

#include <nav/PathFollower.h>
#include <EntityTemplatePlugin.h>

namespace SE {

// Per-variant field overrides for PathFollower in .setm templates:
//   arrival_radius / slow_radius / repath_interval (Float), accept_partial_paths (Boolean).
template <>
struct EntityTemplatePlugin<PathFollower> {
        static void ApplyField(FlatBuffers::PathFollowerT& obj,
                               std::string_view path,
                               const FlatBuffers::FieldOverride& fo);
};

} // namespace SE

#endif
