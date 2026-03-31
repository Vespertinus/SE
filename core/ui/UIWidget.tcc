#ifdef SE_UI_ENABLED

#include <cstdio>
#include <fstream>
#include <string>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/DataModelHandle.h>
#include <ui/UIWidget.h>
#include <ui/UISystem.h>
#include <CommonEvents.h>

namespace SE {

namespace {

std::string GenerateModelName() {
        static int counter = 0;
        return "w" + std::to_string(counter++);
}

// Replace the value of the first data-model="..." attribute in rml with new_name.
// If no such attribute is found, rml is left unchanged.
void PatchModelName(std::string & rml, const std::string & new_name) {
        const std::string prefix = "data-model=\"";
        const auto pos_start = rml.find(prefix);
        if (pos_start == std::string::npos) return;
        const auto val_start = pos_start + prefix.size();
        const auto val_end   = rml.find('"', val_start);
        if (val_end == std::string::npos) return;
        rml.replace(val_start, val_end - val_start, new_name);
}

} // anonymous namespace

UIWidget::UIWidget(TSceneTree::TSceneNodeExact *                    pNewNode,
                   std::string                                       path,
                   UILayer                                           layer,
                   std::function<void(Rml::DataModelConstructor &)> setup_fn,
                   glm::vec3                                         offset)
        : pNode   (pNewNode)
        , sPath   (std::move(path))
        , eLayer  (layer)
        , vOffset (offset)
        , fnSetup (std::move(setup_fn)) {

        sModelName = GenerateModelName();

        Rml::Context * pCtx = GetSystem<UISystem>().GetContext(eLayer);

        Rml::DataModelConstructor ctor = pCtx->CreateDataModel(sModelName);
        if (ctor) {
                fnSetup(ctor);
                hModel = ctor.GetModelHandle();
        }

        // Read RML from disk, patch the data-model attribute with the generated name,
        // then load from memory. sPath is passed as source_url so relative hrefs
        // (e.g. theme.rcss) are resolved through UIFileInterface as normal.
        const std::string full_path = GetSystem<Config>().sResourceDir + sPath;
        std::ifstream f(full_path);
        if (!f) {
                log_w("UIWidget: cannot open '{}'", full_path);
                return;
        }
        std::string content((std::istreambuf_iterator<char>(f)),
                             std::istreambuf_iterator<char>());
        PatchModelName(content, sModelName);

        pDoc = pCtx->LoadDocumentFromMemory(content, sPath);
        // Document stays hidden until Enable() is called.
}

UIWidget::~UIWidget() noexcept {

        if (position_subscribed) {
                pNode->RemoveListener(this);
                GetSystem<EventManager>().RemoveListener<ECameraChanged,
                        &UIWidget::OnCameraChanged>(this);
        }

        if (pDoc) {
                pDoc->Close();
                pDoc = nullptr;
        }

        if (!sModelName.empty()) {
                GetSystem<UISystem>().GetContext(eLayer)->RemoveDataModel(sModelName);
                sModelName.clear();
        }
        hModel = {};
}

void UIWidget::Enable() {

        if (pDoc) pDoc->Show();
        pNode->AddListener(this);
        GetSystem<EventManager>().AddListener<ECameraChanged,
                &UIWidget::OnCameraChanged>(this);
        position_subscribed = true;
        UpdatePosition();
}

void UIWidget::Disable() {

        if (position_subscribed) {
                pNode->RemoveListener(this);
                GetSystem<EventManager>().RemoveListener<ECameraChanged,
                        &UIWidget::OnCameraChanged>(this);
                position_subscribed = false;
        }
        if (pDoc) pDoc->Hide();
}

void UIWidget::Dirty() {

        if (hModel) hModel.DirtyAllVariables();
}

void UIWidget::UpdatePosition() {

        Camera * pCam = GetSystem<TRenderer>().GetCamera();
        if (!pCam || !pDoc) return;

        const glm::vec3 worldPos = pNode->GetTransform().GetWorldPos() + vOffset;
        const glm::vec4 clip     = pCam->GetWorldMVP() * glm::vec4(worldPos, 1.0f);

        // Behind the near plane — hide and bail out
        if (clip.w <= 0.0f || clip.z < 0.0f) {
                pDoc->Hide();
                return;
        }
        pDoc->Show();

        const glm::vec2 sz = glm::vec2(GetSystem<GraphicsState>().GetScreenSize());
        const float sx = ( clip.x / clip.w * 0.5f + 0.5f) * sz.x;
        const float sy = (-clip.y / clip.w * 0.5f + 0.5f) * sz.y;  // Y-flip

        pDoc->SetProperty("left", std::to_string(sx) + "px");
        pDoc->SetProperty("top",  std::to_string(sy) + "px");
}

void UIWidget::TargetTransformChanged(TSceneTree::TSceneNodeExact * /* pTargetNode */) {

        UpdatePosition();
}

void UIWidget::OnCameraChanged(const Event & /* oEvent */) {

        UpdatePosition();
}

} // namespace SE

#endif // SE_UI_ENABLED
