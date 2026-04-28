
#ifndef APP_TRIGGER_VOLUME_ENTITY_PLUGIN_H
#define APP_TRIGGER_VOLUME_ENTITY_PLUGIN_H 1

#include <string_view>
#include <EntityTemplate_generated.h>
#include <TriggerVolume.h>
#include <EntityTemplatePlugin.h>

namespace SE {

template <>
struct EntityTemplatePlugin<TriggerVolume> {
        static void ApplyField(FlatBuffers::TriggerVolumeT& obj,
                               std::string_view path,
                               const FlatBuffers::FieldOverride& fo);
};

} // namespace SE

#endif
