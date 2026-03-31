#ifndef __UI_DATA_MODEL_H__
#define __UI_DATA_MODEL_H__ 1

#ifdef SE_UI_ENABLED

#include <string>
#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Context.h>

namespace SE {

// Creates and binds a named data model on the given context.
// The setup_fn receives (DataModelConstructor&, T&) and should call
// constructor.Bind("field_name", &model.field) for each bound field.
// Returns a DataModelHandle whose DirtyVariable/DirtyAllVariables methods
// notify RmlUi that bound data has changed.
template <typename T, typename SetupFn>
Rml::DataModelHandle RegisterDataModel(Rml::Context      * ctx,
                                       const std::string & name,
                                       T                 & model,
                                       SetupFn          && setup_fn) {
        Rml::DataModelConstructor ctor = ctx->CreateDataModel(name);
        if (!ctor) return {};
        setup_fn(ctor, model);
        return ctor.GetModelHandle();
}

} // namespace SE

#endif // SE_UI_ENABLED
#endif // __UI_DATA_MODEL_H__
