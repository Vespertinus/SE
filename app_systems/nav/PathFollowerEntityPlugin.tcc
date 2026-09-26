
#ifdef SE_IMPL

#include <nav/PathFollowerEntityPlugin.h>

#include <Logging.h>

namespace SE {

void EntityTemplatePlugin<PathFollower>::ApplyField(FlatBuffers::PathFollowerT& obj,
                                                    std::string_view path,
                                                    const FlatBuffers::FieldOverride& fo) {

        if (path == "arrival_radius" || path == "slow_radius" || path == "repath_interval") {
                if (auto* p = fo.value_as_Float()) {
                        const float v = p->value();
                        if (v < 0.f) {
                                log_e("PathFollower::ApplyField: {} must be >= 0", path);
                                return;
                        }
                        if (path == "arrival_radius") { obj.arrival_radius = v; }
                        else if (path == "slow_radius") { obj.slow_radius = v; }
                        else { obj.repath_interval = v; }
                        return;
                }
                log_e("PathFollower::ApplyField: {} expects a float value", path);
                return;
        }

        if (path == "accept_partial_paths") {
                if (auto* p = fo.value_as_Bool()) {
                        obj.accept_partial_paths = p->value();
                        return;
                }
                log_e("PathFollower::ApplyField: accept_partial_paths expects a bool value");
                return;
        }

        log_e("PathFollower::ApplyField: unknown field '{}'", path);
}

} // namespace SE

#endif // SE_IMPL
