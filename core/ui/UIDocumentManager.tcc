#ifdef SE_UI_ENABLED

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/ElementDocument.h>
#include <ui/UIEventRouter.h>
#include <Logging.h>

namespace SE {

UIDocumentId UIDocumentManager::Load(const std::string & path) {

        // Check for already-loaded doc with the same path
        for (size_t i = 0; i < vDocs.size(); ++i) {
                if (vDocs[i].pDoc && vDocs[i].sPath == path) {
                        ++vDocs[i].refcount;
                        return static_cast<UIDocumentId>(i + 1);
                }
        }

        Rml::ElementDocument * pDoc = pContext->LoadDocument(path);
        if (!pDoc) {
                log_w("UIDocumentManager: failed to load '{}'", path);
                return INVALID_UI_DOCUMENT;
        }

        // Find a free slot
        for (size_t i = 0; i < vDocs.size(); ++i) {
                if (!vDocs[i].pDoc) {
                        vDocs[i] = DocEntry{pDoc, 1, path};
                        if (pRouter) pRouter->Attach(pDoc);
                        return static_cast<UIDocumentId>(i + 1);
                }
        }

        // Append new slot
        vDocs.push_back(DocEntry{pDoc, 1, path});
        if (pRouter) pRouter->Attach(pDoc);
        return static_cast<UIDocumentId>(vDocs.size()); // 1-based
}

void UIDocumentManager::Release(UIDocumentId id) {

        if (id == INVALID_UI_DOCUMENT) return;
        const size_t idx = static_cast<size_t>(id) - 1;
        if (idx >= vDocs.size() || !vDocs[idx].pDoc) return;

        if (--vDocs[idx].refcount <= 0) {
                if (pRouter) pRouter->Detach(vDocs[idx].pDoc);
                vDocs[idx].pDoc->Close();
                vDocs[idx] = DocEntry{};
        }
}

Rml::ElementDocument * UIDocumentManager::Get(UIDocumentId id) const {

        if (id == INVALID_UI_DOCUMENT) return nullptr;
        const size_t idx = static_cast<size_t>(id) - 1;
        if (idx >= vDocs.size()) return nullptr;
        return vDocs[idx].pDoc;
}

const std::string & UIDocumentManager::GetPath(UIDocumentId id) const {

        static const std::string sEmpty;
        if (id == INVALID_UI_DOCUMENT) return sEmpty;
        const size_t idx = static_cast<size_t>(id) - 1;
        if (idx >= vDocs.size()) return sEmpty;
        return vDocs[idx].sPath;
}

void UIDocumentManager::ReloadPath(const std::string & path) {

        // Find the slot matching this path
        size_t idx = vDocs.size();
        for (size_t i = 0; i < vDocs.size(); ++i) {
                if (vDocs[i].pDoc && vDocs[i].sPath == path) {
                        idx = i;
                        break;
                }
        }
        if (idx == vDocs.size()) return;  // not loaded

        // .rcss: reload styles on all open documents (shared stylesheet)
        const bool is_rcss = path.size() >= 5 &&
                             path.compare(path.size() - 5, 5, ".rcss") == 0;
        if (is_rcss) {
                for (auto & e : vDocs) {
                        if (e.pDoc) e.pDoc->ReloadStyleSheet();
                }
                return;
        }

        // .rml: close and re-open the document in-place
        Rml::ElementDocument * pOld = vDocs[idx].pDoc;
        const bool was_visible = pOld->IsVisible();

        if (pRouter) pRouter->Detach(pOld);
        pOld->Close();
        pContext->Update();  // process the close immediately

        Rml::ElementDocument * pNew = pContext->LoadDocument(path);
        if (!pNew) {
                log_w("UIDocumentManager: hot-reload failed for '{}'", path);
                vDocs[idx] = DocEntry{};
                return;
        }

        vDocs[idx].pDoc = pNew;
        if (pRouter) pRouter->Attach(pNew);
        if (was_visible) pNew->Show();
        log_i("UIDocumentManager: hot-reloaded '{}'", path);
}

} // namespace SE

#endif // SE_UI_ENABLED
