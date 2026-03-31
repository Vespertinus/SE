#ifdef SE_UI_ENABLED

#include <RmlUi/Core/Core.h>
#include <ui/UIFontRegistry.tcc>
#ifdef SE_UI_DEBUGGER
#include <RmlUi/Debugger/Debugger.h>
#include <InputCodes.h>
#endif
#ifdef SE_UI_HOT_RELOAD
#include <ui/UIFileWatcher.tcc>
#endif

namespace SE {

// Stable context names — index matches UILayer enum
static constexpr const char * UI_LAYER_NAMES[UI_LAYER_COUNT] = {
        "hud", "menu", "popup", "debug", "system"
};

UISystem::UISystem()
        : oFileInterface(GetSystem<Config>().sResourceDir) {

        Rml::SetFileInterface  (&oFileInterface);
        Rml::SetSystemInterface(&oSystemInterface);
        Rml::SetRenderInterface(&oRenderInterface);

        if (!Rml::Initialise()) {
                throw std::runtime_error("UISystem: Rml::Initialise() failed");
        }

#ifdef SE_UI_HOT_RELOAD
        oFileWatcher.WatchDir(GetSystem<Config>().sResourceDir + "ui");
#endif

        const auto & screen_size = GetSystem<GraphicsState>().GetScreenSize();
        const Rml::Vector2i rml_size(static_cast<int>(screen_size.x),
                                     static_cast<int>(screen_size.y));

        oRenderInterface.SetOrtho(static_cast<int>(screen_size.x),
                                  static_cast<int>(screen_size.y));

        // Create one RmlUi context per layer and wire up its sub-systems
        for (size_t i = 0; i < UI_LAYER_COUNT; ++i) {
                auto & layer = vLayers[i];
                layer.pContext = Rml::CreateContext(UI_LAYER_NAMES[i], rml_size);
                if (!layer.pContext) {
                        Rml::Shutdown();
                        throw std::runtime_error(
                                std::string("UISystem: Rml::CreateContext('")
                                + UI_LAYER_NAMES[i] + "') failed");
                }
                layer.oDocManager.Init(layer.pContext);
                layer.oDocManager.SetEventRouter(&oEventRouter);
                layer.oAnimController.Init(layer.oDocManager);
                layer.oScreenManager.Init(layer.oDocManager, layer.oAnimController);
                layer.oThemeManager.Init(layer.pContext);
        }

        // Load all font faces from the compiled binary font list.
        // Falls back gracefully if the file is absent (logs a warning only).
        oFontRegistry.LoadConfig(
                GetSystem<Config>().sResourceDir + "font/fonts.sefl",
                GetSystem<Config>().sResourceDir);

        // Localization — register "loc" data model on every layer context
        oLocalization.Init(GetSystem<Config>().sResourceDir + "locale/", "en");
        for (auto & layer : vLayers)
                oLocalization.RegisterContext(layer.pContext);

        // Build input translator with all layer contexts
        {
                std::vector<Rml::Context *> contexts;
                contexts.reserve(UI_LAYER_COUNT);
                for (auto & layer : vLayers)
                        contexts.push_back(layer.pContext);
                pInputTranslator = new UIInputTranslator(std::move(contexts));
        }

        auto & oEM = GetSystem<EventManager>();
        oEM.AddListener<EUpdate,           &UISystem::OnUpdate>          (this);
        oEM.AddListener<EPostRenderUpdate, &UISystem::OnPostRenderUpdate>(this);
#ifdef SE_UI_DEBUGGER
        Rml::Debugger::Initialise(vLayers[static_cast<size_t>(UILayer::DEBUG)].pContext);
        oEM.AddListener<EKeyDown, &UISystem::OnDebugKeyDown>(this);
#endif
}

UISystem::~UISystem() noexcept {

        auto & oEM = GetSystem<EventManager>();
        oEM.RemoveListener<EUpdate,           &UISystem::OnUpdate>          (this);
        oEM.RemoveListener<EPostRenderUpdate, &UISystem::OnPostRenderUpdate>(this);
#ifdef SE_UI_DEBUGGER
        oEM.RemoveListener<EKeyDown, &UISystem::OnDebugKeyDown>(this);
#endif

        delete pInputTranslator;
        pInputTranslator = nullptr;

        for (size_t i = 0; i < UI_LAYER_COUNT; ++i)
                if (vLayers[i].pContext)
                        Rml::RemoveContext(UI_LAYER_NAMES[i]);
        Rml::Shutdown();
}

void UISystem::SetDimensions(int width, int height) {

        oRenderInterface.SetOrtho(width, height);
        const Rml::Vector2i rml_size(width, height);
        for (auto & layer : vLayers)
                if (layer.pContext)
                        layer.pContext->SetDimensions(rml_size);
}

Rml::Context * UISystem::GetContext(UILayer layer) {

        return vLayers[static_cast<size_t>(layer)].pContext;
}

UIScreenManager & UISystem::GetScreenManager(UILayer layer) {

        return vLayers[static_cast<size_t>(layer)].oScreenManager;
}

UIDocumentManager & UISystem::GetDocumentManager(UILayer layer) {

        return vLayers[static_cast<size_t>(layer)].oDocManager;
}

UIThemeManager & UISystem::GetThemeManager(UILayer layer) {

        return vLayers[static_cast<size_t>(layer)].oThemeManager;
}

void UISystem::OnUpdate(const Event & oEvent) {

        const float dt = oEvent.Get<EUpdate>().last_frame_time;
        oSystemInterface.Tick(dt);

        // Scan layers for the lowest modal layer and block input to layers below it
        if (pInputTranslator) {
                int modal_layer = -1;
                for (int i = 0; i < static_cast<int>(UI_LAYER_COUNT); ++i) {
                        if (vLayers[static_cast<size_t>(i)].oScreenManager.HasModal()) {
                                modal_layer = i;
                                break;
                        }
                }
                pInputTranslator->SetModalMinLayer(modal_layer);
        }

#ifdef SE_UI_HOT_RELOAD
        {
                const auto changed = oFileWatcher.Poll();
                for (const auto & path : changed)
                        for (auto & layer : vLayers)
                                layer.oDocManager.ReloadPath(path);
        }
#endif

        for (auto & layer : vLayers) {
                layer.oScreenManager.Update(dt);
                if (layer.pContext) layer.pContext->Update();
        }
}

void UISystem::OnPostRenderUpdate(const Event & /* oEvent */) {

        // --- Save GL state ---
        GLint     last_scissor_box[4];   glGetIntegerv(GL_SCISSOR_BOX, last_scissor_box);
        GLenum    last_blend_src_rgb;    glGetIntegerv(GL_BLEND_SRC_RGB,   reinterpret_cast<GLint*>(&last_blend_src_rgb));
        GLenum    last_blend_dst_rgb;    glGetIntegerv(GL_BLEND_DST_RGB,   reinterpret_cast<GLint*>(&last_blend_dst_rgb));
        GLenum    last_blend_src_alpha;  glGetIntegerv(GL_BLEND_SRC_ALPHA, reinterpret_cast<GLint*>(&last_blend_src_alpha));
        GLenum    last_blend_dst_alpha;  glGetIntegerv(GL_BLEND_DST_ALPHA, reinterpret_cast<GLint*>(&last_blend_dst_alpha));
        GLenum    last_blend_eq_rgb;     glGetIntegerv(GL_BLEND_EQUATION_RGB,   reinterpret_cast<GLint*>(&last_blend_eq_rgb));
        GLenum    last_blend_eq_alpha;   glGetIntegerv(GL_BLEND_EQUATION_ALPHA, reinterpret_cast<GLint*>(&last_blend_eq_alpha));
        GLboolean last_blend            = glIsEnabled(GL_BLEND);
        GLboolean last_cull             = glIsEnabled(GL_CULL_FACE);
        GLboolean last_depth            = glIsEnabled(GL_DEPTH_TEST);
        GLboolean last_scissor          = glIsEnabled(GL_SCISSOR_TEST);
        GLboolean last_stencil          = glIsEnabled(GL_STENCIL_TEST);

        // Stencil state (front face only — RmlUi uses front-face stencil)
        GLint last_stencil_func, last_stencil_ref, last_stencil_mask;
        glGetIntegerv(GL_STENCIL_FUNC,        &last_stencil_func);
        glGetIntegerv(GL_STENCIL_REF,         &last_stencil_ref);
        glGetIntegerv(GL_STENCIL_VALUE_MASK,  &last_stencil_mask);
        GLint last_stencil_fail, last_stencil_zfail, last_stencil_zpass;
        glGetIntegerv(GL_STENCIL_FAIL,            &last_stencil_fail);
        glGetIntegerv(GL_STENCIL_PASS_DEPTH_FAIL, &last_stencil_zfail);
        glGetIntegerv(GL_STENCIL_PASS_DEPTH_PASS, &last_stencil_zpass);
        GLint last_stencil_writemask;
        glGetIntegerv(GL_STENCIL_WRITEMASK, &last_stencil_writemask);

        // --- Setup for RmlUi rendering ---
        glEnable(GL_BLEND);
        glBlendEquation(GL_FUNC_ADD);
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);   // premultiplied alpha
        glDisable(GL_CULL_FACE);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_SCISSOR_TEST);
        glDisable(GL_STENCIL_TEST);

        for (auto & layer : vLayers)
                if (layer.pContext) layer.pContext->Render();

        // --- Restore GL state ---
        glBlendEquationSeparate(last_blend_eq_rgb, last_blend_eq_alpha);
        glBlendFuncSeparate(last_blend_src_rgb, last_blend_dst_rgb,
                            last_blend_src_alpha, last_blend_dst_alpha);
        if (last_blend)   glEnable(GL_BLEND);        else glDisable(GL_BLEND);
        if (last_cull)    glEnable(GL_CULL_FACE);    else glDisable(GL_CULL_FACE);
        if (last_depth)   glEnable(GL_DEPTH_TEST);   else glDisable(GL_DEPTH_TEST);
        if (last_scissor) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
        if (last_stencil) glEnable(GL_STENCIL_TEST); else glDisable(GL_STENCIL_TEST);
        glScissor(last_scissor_box[0], last_scissor_box[1],
                  static_cast<GLsizei>(last_scissor_box[2]),
                  static_cast<GLsizei>(last_scissor_box[3]));
        glStencilFunc(static_cast<GLenum>(last_stencil_func),
                      last_stencil_ref,
                      static_cast<GLuint>(last_stencil_mask));
        glStencilOp(static_cast<GLenum>(last_stencil_fail),
                    static_cast<GLenum>(last_stencil_zfail),
                    static_cast<GLenum>(last_stencil_zpass));
        glStencilMask(static_cast<GLuint>(last_stencil_writemask));
}

void UISystem::SetLocale(const std::string & locale) {
        oLocalization.SetLocale(locale);
}

#ifdef SE_UI_DEBUGGER
void UISystem::OnDebugKeyDown(const Event & oEvent) {

        auto & ev = oEvent.Get<EKeyDown>();
        if (ev.key != Keys::F8) return;
        if (ev.mod & Keymods::CTRL) {
                debug_layer_idx = (debug_layer_idx + 1) % static_cast<int>(UI_LAYER_COUNT);
                Rml::Debugger::SetContext(
                        vLayers[static_cast<size_t>(debug_layer_idx)].pContext);
        } else {
                Rml::Debugger::SetVisible(!Rml::Debugger::IsVisible());
        }
}
#endif

} // namespace SE

#endif // SE_UI_ENABLED
