
#ifndef APP_NAV_AGENT_ENTITY_PLUGIN_H
#define APP_NAV_AGENT_ENTITY_PLUGIN_H 1

#include <string_view>

// Xlib (reachable through graphics headers in mixed TUs) does `#define Bool
// int` — keep it out of the generated FieldValueU union member.
#pragma push_macro("Bool")
#undef Bool
#include <EntityTemplate_generated.h>
#pragma pop_macro("Bool")

#include <nav/NavAgent.h>
#include <EntityTemplatePlugin.h>

namespace SE {

// Per-variant field overrides for NavAgent in .setm templates:
//   agent_class (StringValue), radius / height (Float), avoidance (Boolean).
template <>
struct EntityTemplatePlugin<NavAgent> {
        static void ApplyField(FlatBuffers::NavAgentT& obj,
                               std::string_view path,
                               const FlatBuffers::FieldOverride& fo);
};

} // namespace SE

#endif
