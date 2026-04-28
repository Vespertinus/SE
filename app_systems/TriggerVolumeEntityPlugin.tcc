
namespace SE {

void EntityTemplatePlugin<TriggerVolume>::ApplyField(FlatBuffers::TriggerVolumeT& obj,
                                                      std::string_view path,
                                                      const FlatBuffers::FieldOverride& fo) {

        auto get_float = [&]() -> float {
                if (auto* p = fo.value_as_Float()) return p->value();
                log_e("TriggerVolume::ApplyField: '{}' expects Float value", path);
                return 0.f;
        };
        auto get_bool = [&]() -> bool {
                if (auto* p = fo.value_as_Bool()) return p->value() != 0;
                log_e("TriggerVolume::ApplyField: '{}' expects Bool value", path);
                return false;
        };
        auto get_uint = [&]() -> uint32_t {
                if (auto* p = fo.value_as_Int()) return static_cast<uint32_t>(p->value());
                log_e("TriggerVolume::ApplyField: '{}' expects Int value", path);
                return 0u;
        };
        auto get_string = [&]() -> std::string {
                if (auto* p = fo.value_as_StringValue()) return p->value()->str();
                log_e("TriggerVolume::ApplyField: '{}' expects StringValue", path);
                return {};
        };

        if (path == "on_enter_event") { obj.on_enter_event  = get_string(); return; }
        if (path == "on_exit_event")  { obj.on_exit_event   = get_string(); return; }
        if (path == "on_stay_event")  { obj.on_stay_event   = get_string(); return; }
        if (path == "stay_interval")  { obj.stay_interval   = get_float();  return; }
        if (path == "one_shot")       { obj.one_shot         = get_bool();   return; }
        if (path == "collision_layer"){ obj.collision_layer  = get_uint();   return; }
        if (path == "collision_mask") { obj.collision_mask   = get_uint();   return; }

        log_e("TriggerVolume::ApplyField: unknown field '{}'", path);
}

} // namespace SE
