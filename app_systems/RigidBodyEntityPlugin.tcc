
namespace SE {

void EntityTemplatePlugin<RigidBody>::ApplyField(FlatBuffers::RigidBodyT& obj,
                                                  std::string_view path,
                                                  const FlatBuffers::FieldOverride& fo) {

        auto get_float = [&]() -> float {
                if (auto* p = fo.value_as_Float()) return p->value();
                log_e("RigidBody::ApplyField: '{}' expects Float value", path);
                return 0.f;
        };
        auto get_bool = [&]() -> bool {
                if (auto* p = fo.value_as_Bool()) return p->value() != 0;
                log_e("RigidBody::ApplyField: '{}' expects Bool value", path);
                return false;
        };

        if (path == "mass")            { obj.mass            = get_float(); return; }
        if (path == "friction")        { obj.friction        = get_float(); return; }
        if (path == "restitution")     { obj.restitution     = get_float(); return; }
        if (path == "linear_damping")  { obj.linear_damping  = get_float(); return; }
        if (path == "angular_damping") { obj.angular_damping = get_float(); return; }
        if (path == "gravity_scale")   { obj.gravity_scale   = get_float(); return; }
        if (path == "is_static")       { obj.is_static       = get_bool();  return; }
        if (path == "is_kinematic")    { obj.is_kinematic    = get_bool();  return; }
        if (path == "is_trigger")      { obj.is_trigger      = get_bool();  return; }

        auto get_uint = [&]() -> uint32_t {
                if (auto* p = fo.value_as_Int()) return static_cast<uint32_t>(p->value());
                log_e("RigidBody::ApplyField: '{}' expects Int value", path);
                return 0u;
        };

        if (path == "collision_layer") { obj.collision_layer = get_uint(); return; }
        if (path == "collision_mask")  { obj.collision_mask  = get_uint(); return; }

        log_e("RigidBody::ApplyField: unknown field '{}'", path);
}

} // namespace SE
