
#ifndef APP_RIGID_BODY_ENTITY_PLUGIN_H
#define APP_RIGID_BODY_ENTITY_PLUGIN_H 1

#include <string_view>
#include <EntityTemplate_generated.h>
#include <RigidBody.h>
#include <EntityTemplatePlugin.h>

namespace SE {

template <>
struct EntityTemplatePlugin<RigidBody> {
        static void ApplyField(FlatBuffers::RigidBodyT& obj,
                               std::string_view path,
                               const FlatBuffers::FieldOverride& fo);
};

} // namespace SE

#endif
