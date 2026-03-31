#ifdef SE_UI_ENABLED

#include <UIFontList_generated.h>

#include <flatbuffers/flatbuffers.h>
#include <RmlUi/Core/Core.h>
#include <fstream>

namespace SE {

bool UIFontRegistry::RegisterFont(const FontDesc & desc, const std::string & sFullPath) {

        // Deduplicate: skip if this TTF path is already loaded
        for (const auto & f : vFonts) {
                if (f.sPath == sFullPath) return true;
        }

        std::ifstream f(sFullPath, std::ios::binary | std::ios::ate);
        if (!f) {
                log_w("UIFontRegistry: cannot open '{}'", sFullPath);
                return false;
        }
        const auto sz = f.tellg();
        f.seekg(0);
        LoadedFont lf;
        lf.sPath = sFullPath;
        lf.vData.resize(static_cast<size_t>(sz));
        if (!f.read(reinterpret_cast<char *>(lf.vData.data()), sz)) {
                log_w("UIFontRegistry: read error on '{}'", sFullPath);
                return false;
        }

        const bool ok = Rml::LoadFontFace(
                Rml::Span<const Rml::byte>{lf.vData.data(), lf.vData.size()},
                desc.sFamily.c_str(),
                desc.style,
                desc.weight,
                desc.fallback);

        if (!ok) {
                log_w("UIFontRegistry: Rml::LoadFontFace failed for '{}' (family '{}')",
                      sFullPath, desc.sFamily);
                return false;
        }

        vFonts.push_back(std::move(lf));
        log_i("UIFontRegistry: registered '{}' as family '{}'", sFullPath, desc.sFamily);
        return true;
}

bool UIFontRegistry::LoadConfig(const std::string & sPath, const std::string & sResourceDir) {

        std::ifstream f(sPath, std::ios::binary | std::ios::ate);
        if (!f) {
                log_w("UIFontRegistry: cannot open '{}'", sPath);
                return false;
        }
        const auto sz = f.tellg();
        f.seekg(0);
        std::vector<uint8_t> buf(static_cast<size_t>(sz));
        if (!f.read(reinterpret_cast<char *>(buf.data()), sz)) {
                log_w("UIFontRegistry: read error on '{}'", sPath);
                return false;
        }

        flatbuffers::Verifier v(buf.data(), buf.size());
        if (!SE::FlatBuffers::VerifyUIFontListBuffer(v)) {
                log_w("UIFontRegistry: invalid SEFL buffer in '{}'", sPath);
                return false;
        }

        const auto * list = SE::FlatBuffers::GetUIFontList(buf.data());
        if (!list->faces()) {
                log_i("UIFontRegistry: '{}' loaded (empty)", sPath);
                return true;
        }

        int loaded = 0;
        for (const auto * face : *list->faces()) {
                if (!face->path() || !face->family()) continue;

                FontDesc desc;
                desc.sPath   = face->path()->c_str();
                desc.sFamily = face->family()->c_str();
                desc.style   = (face->style()  == SE::FlatBuffers::FontStyle_ITALIC)
                                ? Rml::Style::FontStyle::Italic
                                : Rml::Style::FontStyle::Normal;
                desc.weight  = (face->weight() == SE::FlatBuffers::FontWeight_BOLD)
                                ? Rml::Style::FontWeight::Bold
                                : Rml::Style::FontWeight::Normal;
                desc.fallback = face->fallback();

                const std::string sFullPath = sResourceDir + desc.sPath;
                if (RegisterFont(desc, sFullPath)) ++loaded;
        }

        log_i("UIFontRegistry: loaded {} font face(s) from '{}'", loaded, sPath);
        return true;
}

} // namespace SE

#endif // SE_UI_ENABLED
