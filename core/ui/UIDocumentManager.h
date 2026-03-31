#ifndef __UI_DOCUMENT_MANAGER_H__
#define __UI_DOCUMENT_MANAGER_H__ 1

#ifdef SE_UI_ENABLED

#include <string>
#include <vector>
#include <ui/UITypes.h>

namespace Rml {
class Context;
class ElementDocument;
}

namespace SE {

class UIEventRouter;

class UIDocumentManager {

        struct DocEntry {
                Rml::ElementDocument * pDoc{nullptr};
                int                    refcount{0};
                std::string            sPath;
        };

        std::vector<DocEntry>  vDocs;    // slot index+1 = UIDocumentId; slot 0 unused
        Rml::Context         * pContext{nullptr};
        UIEventRouter        * pRouter {nullptr};

public:
        UIDocumentManager() = default;

        void Init(Rml::Context * ctx) { pContext = ctx; }
        void SetEventRouter(UIEventRouter * r) { pRouter = r; }

        UIDocumentId           Load      (const std::string & path);
        void                   Release   (UIDocumentId id);
        Rml::ElementDocument * Get       (UIDocumentId id) const;
        const std::string &    GetPath   (UIDocumentId id) const;

        // Hot-reload: reload .rml (close+reopen) or .rcss (ReloadStyleSheet).
        // No-op if no document with this path is currently loaded.
        void                   ReloadPath(const std::string & path);
};

} // namespace SE

#endif // SE_UI_ENABLED
#endif // __UI_DOCUMENT_MANAGER_H__
