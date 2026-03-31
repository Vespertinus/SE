#ifndef __UI_LOCALIZATION_H__
#define __UI_LOCALIZATION_H__ 1

#ifdef SE_UI_ENABLED

#include <string>
#include <unordered_map>
#include <vector>
#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Context.h>

namespace SE {

class UILocalization {

        std::unordered_map<std::string, std::string> mStrings;
        std::vector<Rml::DataModelHandle>            vHandles;
        std::string                                  sLocale;
        std::string                                  sBaseDir;

        static bool LoadBinary(const std::string & path,
                               std::unordered_map<std::string, std::string> & out);

public:
        // Load the default locale and store the resource directory base path.
        void Init           (const std::string & base_dir,
                             const std::string & default_locale);

        // Register the "loc" data model on a context.  Call once per UILayer context
        // after Init() and before any document using data-model="loc" is loaded.
        void RegisterContext(Rml::Context * pCtx);

        // Reload strings from <base_dir>/<locale>.selt and dirty all model handles.
        void SetLocale      (const std::string & locale);

        const std::string & GetLocale() const { return sLocale; }
};

} // namespace SE

#endif // SE_UI_ENABLED
#endif // __UI_LOCALIZATION_H__
