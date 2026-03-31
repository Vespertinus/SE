#ifndef __UI_FONT_REGISTRY_H__
#define __UI_FONT_REGISTRY_H__ 1

#ifdef SE_UI_ENABLED

#include <string>
#include <vector>
#include <RmlUi/Core/Types.h>
#include <RmlUi/Core/StyleTypes.h>

namespace SE {

struct FontDesc {
        std::string                  sPath;     // relative to sResourceDir, e.g. "font/Lato-Regular.ttf"
        std::string                  sFamily;   // CSS family name, e.g. "Lato"
        Rml::Style::FontStyle        style    = Rml::Style::FontStyle::Normal;
        Rml::Style::FontWeight       weight   = Rml::Style::FontWeight::Normal;
        bool                         fallback = false;
};

// Owns all font byte buffers for the lifetime of the UISystem / RmlUi context.
// Fonts are registered globally with RmlUi and cannot be individually unloaded —
// only Rml::Shutdown() releases them.  Therefore this class intentionally does NOT
// participate in the ResourceManager lifecycle.
class UIFontRegistry {

        struct LoadedFont {
                std::vector<Rml::byte> vData;  // raw TTF/OTF bytes; must outlive RmlUi
                std::string            sPath;  // canonical path (for deduplication)
        };
        std::vector<LoadedFont> vFonts;

public:
        UIFontRegistry() = default;

        // Register a single font face from a full filesystem path.
        // Safe to call multiple times with the same path (deduplicated).
        bool RegisterFont(const FontDesc & desc, const std::string & sFullPath);

        // Load all font faces described in a compiled .sefl FlatBuffers binary.
        // sPath is the full filesystem path to the .sefl file.
        // sResourceDir is prepended to each face's relative path when opening the TTF.
        bool LoadConfig(const std::string & sPath, const std::string & sResourceDir);

        int LoadedCount() const { return static_cast<int>(vFonts.size()); }
};

} // namespace SE

#endif // SE_UI_ENABLED
#endif // __UI_FONT_REGISTRY_H__
