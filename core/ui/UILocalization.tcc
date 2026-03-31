#ifdef SE_UI_ENABLED

#include <fstream>
#include <vector>
#include <flatbuffers/flatbuffers.h>
#include <UILocaleTable_generated.h>
#include <RmlUi/Core/DataModelHandle.h>
#include <ui/UILocalization.h>

namespace SE {

bool UILocalization::LoadBinary(const std::string & path,
                                std::unordered_map<std::string, std::string> & out) {
        std::ifstream f(path, std::ios::binary | std::ios::ate);
        if (!f) {
                log_w("UILocalization: cannot open '{}'", path);
                return false;
        }
        const auto sz = f.tellg();
        f.seekg(0);
        std::vector<uint8_t> buf(static_cast<size_t>(sz));
        if (!f.read(reinterpret_cast<char *>(buf.data()), sz)) {
                log_w("UILocalization: read error on '{}'", path);
                return false;
        }

        flatbuffers::Verifier v(buf.data(), buf.size());
        if (!SE::FlatBuffers::VerifyLocaleTableBuffer(v)) {
                log_w("UILocalization: invalid SELT buffer in '{}'", path);
                return false;
        }

        const auto * table = SE::FlatBuffers::GetLocaleTable(buf.data());
        if (!table->entries()) return true;

        for (const auto * entry : *table->entries()) {
                if (!entry->key() || !entry->value()) continue;
                out[entry->key()->c_str()] = entry->value()->c_str();
        }
        return true;
}

void UILocalization::Init(const std::string & base_dir,
                          const std::string & default_locale) {
        sBaseDir = base_dir;
        LoadBinary(sBaseDir + default_locale + ".selt", mStrings);
        sLocale = default_locale;
}

void UILocalization::RegisterContext(Rml::Context * pCtx) {
        if (!pCtx) return;
        Rml::DataModelConstructor ctor = pCtx->CreateDataModel("loc");
        if (!ctor) return;
        for (auto & [key, val] : mStrings) {
                ctor.BindFunc(key, [this, k = key](Rml::Variant & v) {
                        auto it = mStrings.find(k);
                        v = Rml::String(it != mStrings.end() ? it->second : "");
                });
        }
        vHandles.push_back(ctor.GetModelHandle());
}

void UILocalization::SetLocale(const std::string & locale) {
        if (locale == sLocale) return;
        LoadBinary(sBaseDir + locale + ".selt", mStrings);
        sLocale = locale;
        for (auto & h : vHandles) h.DirtyAllVariables();
}

} // namespace SE

#endif // SE_UI_ENABLED
