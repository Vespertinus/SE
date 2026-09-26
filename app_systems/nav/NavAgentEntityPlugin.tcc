
#ifdef SE_IMPL

#include <nav/NavAgentEntityPlugin.h>

#include <EntityTemplateUtility.h>
#include <Logging.h>

namespace SE {

void EntityTemplatePlugin<NavAgent>::ApplyField(FlatBuffers::NavAgentT& obj,
                                                std::string_view path,
                                                const FlatBuffers::FieldOverride& fo) {

        if (path == "agent_class") {
                if (auto* p = fo.value_as_StringValue()) {
                        obj.agent_class = p->value()->str();
                        return;
                }
                log_e("NavAgent::ApplyField: agent_class expects a string value");
                return;
        }

        if (path == "radius" || path == "height") {
                if (auto* p = fo.value_as_Float()) {
                        const float v = p->value();
                        if (v < 0.f) {
                                log_e("NavAgent::ApplyField: {} must be >= 0", path);
                                return;
                        }
                        if (path == "radius") { obj.radius = v; } else { obj.height = v; }
                        return;
                }
                log_e("NavAgent::ApplyField: {} expects a float value", path);
                return;
        }

        if (path == "avoidance") {
                if (auto* p = fo.value_as_Bool()) {
                        obj.avoidance = p->value();
                        return;
                }
                log_e("NavAgent::ApplyField: avoidance expects a bool value");
                return;
        }

        log_e("NavAgent::ApplyField: unknown field '{}'", path);
}

} // namespace SE

#endif // SE_IMPL
