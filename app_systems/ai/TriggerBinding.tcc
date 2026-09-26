
#ifdef SE_IMPL

// Xlib (via graphics headers in mixed TUs) does `#define Bool int` —
// keep it out of this file; generated union members use Bool as a
// scope-qualified identifier (same dance as the entity plugins).
#pragma push_macro("Bool")
#undef Bool

#include <ai/TriggerBinding.h>

#include <Component_generated.h>
#include <Logging.h>

namespace SE {

namespace {

// Authored value extraction shared by both ctors.
bool ParseValue(const FlatBuffers::TriggerBindingDef* pDef, TriggerBinding::SDef& out) {

        using VU = FlatBuffers::BBValueU;

        switch (pDef->value_type()) {
                case VU::Float: {
                        const auto* p = pDef->value_as_Float();
                        if (!p) return false;
                        out.has_value  = true;
                        out.value_type = EBBType::Float;
                        out.f_value    = p->value();
                        return true;
                }
                case VU::Bool: {
                        const auto* p = pDef->value_as_Bool();
                        if (!p) return false;
                        out.has_value  = true;
                        out.value_type = EBBType::Bool;
                        out.b_value    = p->value();
                        return true;
                }
                case VU::Int: {
                        const auto* p = pDef->value_as_Int();
                        if (!p) return false;
                        out.has_value  = true;
                        out.value_type = EBBType::Int;
                        out.i_value    = p->value();
                        return true;
                }
                case VU::Vec3: {
                        const auto* p = pDef->value_as_Vec3();
                        if (!p) return false;
                        out.has_value  = true;
                        out.value_type = EBBType::Vec3;
                        out.v_value    = glm::vec3(p->x(), p->y(), p->z());
                        return true;
                }
                default:
                        return false;   // absent value: legal only with write_position
        }
}

} // namespace

TriggerBinding::TriggerBinding(TSceneTree::TSceneNodeExact* /*pNode*/, const Desc& desc)
        : vBindings(desc.bindings) {
}

TriggerBinding::TriggerBinding(TSceneTree::TSceneNodeExact* /*pNode*/,
                               const FlatBuffers::TriggerBinding* pFB) {

        if (!pFB || !pFB->bindings() || pFB->bindings()->size() == 0) {
                throw std::runtime_error("TriggerBinding: no bindings");
        }

        vBindings.reserve(pFB->bindings()->size());

        for (const auto* pDef : *pFB->bindings()) {
                if (!pDef || !pDef->on_event() || pDef->on_event()->size() == 0) {
                        throw std::runtime_error("TriggerBinding: binding with no on_event");
                }
                if (!pDef->key() || pDef->key()->size() == 0) {
                        throw std::runtime_error(
                                        fmt::format("TriggerBinding: binding '{}' has no key",
                                                    pDef->on_event()->str()));
                }

                SDef d;
                d.s_event      = pDef->on_event()->str();
                d.event_id     = StrID(d.s_event);
                d.global_target = pDef->target() == FlatBuffers::TriggerBBTarget::Global;
                d.s_key        = pDef->key()->str();
                d.key          = StrID(d.s_key);
                d.write_position = pDef->write_position();

                if (pDef->value_type() != FlatBuffers::BBValueU::NONE) {
                        if (!ParseValue(pDef, d)) {
                                throw std::runtime_error(fmt::format(
                                                "TriggerBinding: binding '{}'->'{}' has malformed value",
                                                d.s_event, d.s_key));
                        }
                }

                if (!d.has_value && !d.write_position) {
                        throw std::runtime_error(fmt::format(
                                        "TriggerBinding: binding '{}'->'{}' has neither value nor write_position",
                                        d.s_event, d.s_key));
                }

                vBindings.push_back(std::move(d));
        }
}

std::string TriggerBinding::Str() const {
        return fmt::format("TriggerBinding[bindings={}]", vBindings.size());
}

} // namespace SE

#endif // SE_IMPL

#pragma pop_macro("Bool")
