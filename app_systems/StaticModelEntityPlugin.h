
#ifndef APP_STATIC_MODEL_ENTITY_PLUGIN_H
#define APP_STATIC_MODEL_ENTITY_PLUGIN_H 1

#include <string_view>
#include <EntityTemplate_generated.h>
#include <StaticModel.h>
#include <EntityTemplatePlugin.h>

namespace SE {

template <>
struct EntityTemplatePlugin<StaticModel> {
        static void ApplyField(FlatBuffers::StaticModelT& obj,
                               std::string_view path,
                               const FlatBuffers::FieldOverride& fo);
};

} // namespace SE

#endif
