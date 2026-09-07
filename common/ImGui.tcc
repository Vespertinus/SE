
static ImGuiKey ScancodeToImGuiKey(int sc) {
        if (sc >= 4  && sc <= 29) return (ImGuiKey)(ImGuiKey_A  + (sc - 4));
        if (sc >= 58 && sc <= 69) return (ImGuiKey)(ImGuiKey_F1 + (sc - 58));
        switch (sc) {
                case 40: return ImGuiKey_Enter;
                case 41: return ImGuiKey_Escape;
                case 42: return ImGuiKey_Backspace;
                case 43: return ImGuiKey_Tab;
                case 44: return ImGuiKey_Space;
                case 73: return ImGuiKey_Insert;
                case 74: return ImGuiKey_Home;
                case 75: return ImGuiKey_PageUp;
                case 76: return ImGuiKey_Delete;
                case 77: return ImGuiKey_End;
                case 78: return ImGuiKey_PageDown;
                case 79: return ImGuiKey_RightArrow;
                case 80: return ImGuiKey_LeftArrow;
                case 81: return ImGuiKey_DownArrow;
                case 82: return ImGuiKey_UpArrow;
                default: return ImGuiKey_None;
        }
}

namespace SE {
namespace HELPERS {

ImGuiWrapper::ImGuiWrapper() {

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;

        InitWndData();
        InitGLData();
        BuildFontTex();

        ImGui::StyleColorsDark();
        ImGui::GetStyle().WindowRounding = 7.0f;

        auto & oEM = GetSystem<EventManager>();
        oEM.AddListener<EKeyDown,         &ImGuiWrapper::OnKeyDown>        (this);
        oEM.AddListener<EKeyUp,           &ImGuiWrapper::OnKeyUp>          (this);
        oEM.AddListener<ETextInput,       &ImGuiWrapper::OnTextInput>      (this);
        oEM.AddListener<EMouseMove,       &ImGuiWrapper::OnMouseMove>      (this);
        oEM.AddListener<EMouseButtonDown, &ImGuiWrapper::OnMouseButtonDown>(this);
        oEM.AddListener<EMouseButtonUp,   &ImGuiWrapper::OnMouseButtonUp>  (this);

        oEM.AddListener<EFrameStart,       &ImGuiWrapper::NewFrame>(this);
        oEM.AddListener<EPostRenderUpdate, &ImGuiWrapper::Render>(this);
}

ImGuiWrapper::~ImGuiWrapper() {

        UnlockResource(hFontTex);
        ImGui::DestroyContext();

        glDeleteBuffers(1, &vbo_id);
        glDeleteBuffers(1, &index_array_id);
        glDeleteVertexArrays(1, &vao_id);

        auto & oEM = GetSystem<EventManager>();
        oEM.RemoveListener<EKeyDown,         &ImGuiWrapper::OnKeyDown>        (this);
        oEM.RemoveListener<EKeyUp,           &ImGuiWrapper::OnKeyUp>          (this);
        oEM.RemoveListener<ETextInput,       &ImGuiWrapper::OnTextInput>      (this);
        oEM.RemoveListener<EMouseMove,       &ImGuiWrapper::OnMouseMove>      (this);
        oEM.RemoveListener<EMouseButtonDown, &ImGuiWrapper::OnMouseButtonDown>(this);
        oEM.RemoveListener<EMouseButtonUp,   &ImGuiWrapper::OnMouseButtonUp>  (this);

        oEM.RemoveListener<EFrameStart,       &ImGuiWrapper::NewFrame>(this);
        oEM.RemoveListener<EPostRenderUpdate, &ImGuiWrapper::Render>(this);
}

void ImGuiWrapper::InitWndData() {

        ImGuiIO& io = ImGui::GetIO();
        io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;
        io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;
}

void ImGuiWrapper::InitGLData() {

        hShader = CreateResource<SE::ShaderProgram>(GetSystem<Config>().sResourceDir + "shader_program/imgui.sesp");

        uint32_t pos_location,
                 uv_location,
                 color_location;

        if (auto itLocation = mAttributeLocation.find("Position"); itLocation != mAttributeLocation.end()) {
                pos_location = itLocation->second;
        }
        else {
                throw(std::runtime_error("failed to find 'Position' attr"));
        }
        if (auto itLocation = mAttributeLocation.find("TexCoord0"); itLocation != mAttributeLocation.end()) {
                uv_location = itLocation->second;
        }
        else {
                throw(std::runtime_error("failed to find 'TexCoord0' attr"));
        }
        if (auto itLocation = mAttributeLocation.find("Color"); itLocation != mAttributeLocation.end()) {
                color_location = itLocation->second;
        }
        else {
                throw(std::runtime_error("failed to find 'Color' attr"));
        }

        glGenVertexArrays(1, &vao_id);
        glBindVertexArray(vao_id);

        glGenBuffers(1, &vbo_id);
        glGenBuffers(1, &index_array_id);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_array_id);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_id);

        glEnableVertexAttribArray(pos_location);
        glEnableVertexAttribArray(uv_location);
        glEnableVertexAttribArray(color_location);

        glVertexAttribPointer(pos_location, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), (GLvoid*)offsetof(ImDrawVert, pos));
        glVertexAttribPointer(uv_location, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), (GLvoid*)offsetof(ImDrawVert, uv));
        glVertexAttribPointer(color_location, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(ImDrawVert), (GLvoid*)offsetof(ImDrawVert, col));

        glBindVertexArray(0);
}

void ImGuiWrapper::BuildFontTex() {

        ImGuiIO       & io = ImGui::GetIO();
        uint8_t       * pPixels;
        int32_t         width,
                        height;

        io.Fonts->GetTexDataAsRGBA32(&pPixels, &width, &height);

        hFontTex = CreateResource<SE::TTexture>(
                        "ImGuiFontTexture",
                        TextureStock {
                                pPixels,
                                (uint32_t)(width * height * 4) /* RGBA ubyte channels*/,
                                GL_RGBA,
                                GL_RGBA8,
                                (uint16_t)width,
                                (uint16_t)height
                        },
                        StoreTexture2D::Settings(false));

        LockResource(hFontTex);
        io.Fonts->SetTexID((ImTextureID)(intptr_t)GetResource(hFontTex));
}

void ImGuiWrapper::Render(const Event & oEvent [[maybe_unused]]) {

        ImGui::Render();
        ImDrawData * pDrawData = ImGui::GetDrawData();
        ImGuiIO & io           = ImGui::GetIO();

        // No windows drawn this frame (DisplaySize is still set, so the
        // framebuffer-size check below does not catch it) — skip the whole
        // GL state save/draw/restore pass.
        if (pDrawData->CmdListsCount == 0) {
                return;
        }

        static StrID mat_id("MVPMatrix");
        int fb_width  = (int)(pDrawData->DisplaySize.x * io.DisplayFramebufferScale.x);
        int fb_height = (int)(pDrawData->DisplaySize.y * io.DisplayFramebufferScale.y);
        if (fb_width <= 0 || fb_height <= 0) {
                return;
        }

        pDrawData->ScaleClipRects(io.DisplayFramebufferScale);

        //save prev state
        //TODO move to GraphicsState
        GLint last_scissor_box[4]; glGetIntegerv(GL_SCISSOR_BOX, last_scissor_box);
        GLenum last_blend_src_rgb; glGetIntegerv(GL_BLEND_SRC_RGB, (GLint*)&last_blend_src_rgb);
        GLenum last_blend_dst_rgb; glGetIntegerv(GL_BLEND_DST_RGB, (GLint*)&last_blend_dst_rgb);
        GLenum last_blend_src_alpha; glGetIntegerv(GL_BLEND_SRC_ALPHA, (GLint*)&last_blend_src_alpha);
        GLenum last_blend_dst_alpha; glGetIntegerv(GL_BLEND_DST_ALPHA, (GLint*)&last_blend_dst_alpha);
        GLenum last_blend_equation_rgb; glGetIntegerv(GL_BLEND_EQUATION_RGB, (GLint*)&last_blend_equation_rgb);
        GLenum last_blend_equation_alpha; glGetIntegerv(GL_BLEND_EQUATION_ALPHA, (GLint*)&last_blend_equation_alpha);
        GLboolean last_enable_blend = glIsEnabled(GL_BLEND);
        GLboolean last_enable_cull_face = glIsEnabled(GL_CULL_FACE);
        GLboolean last_enable_depth_test = glIsEnabled(GL_DEPTH_TEST);
        GLboolean last_enable_scissor_test = glIsEnabled(GL_SCISSOR_TEST);

        glEnable(GL_BLEND);
        glBlendEquation(GL_FUNC_ADD);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_CULL_FACE);
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_SCISSOR_TEST);

        float L = pDrawData->DisplayPos.x;
        float R = pDrawData->DisplayPos.x + pDrawData->DisplaySize.x;
        float T = pDrawData->DisplayPos.y;
        float B = pDrawData->DisplayPos.y + pDrawData->DisplaySize.y;
        const glm::mat4 mOrtho = {
                { 2.0f/(R-L),   0.0f,         0.0f,   0.0f },
                { 0.0f,         2.0f/(T-B),   0.0f,   0.0f },
                { 0.0f,         0.0f,        -1.0f,   0.0f },
                { (R+L)/(L-R),  (T+B)/(B-T),  0.0f,   1.0f },
        };

        auto & oGraphicsState = GetSystem<GraphicsState>();

        oGraphicsState.SetVao(vao_id);
        oGraphicsState.SetShaderProgram(GetResource(hShader));
        oGraphicsState.SetVariable(mat_id, mOrtho);

        //gen buf to init gl
        ImVec2 pos = pDrawData->DisplayPos;
        for (int n = 0; n < pDrawData->CmdListsCount; ++n) {

                const ImDrawList* cmd_list = pDrawData->CmdLists[n];
                uint32_t index_offset = 0;

                glBindBuffer(GL_ARRAY_BUFFER, vbo_id);
                glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)cmd_list->VtxBuffer.Size * sizeof(ImDrawVert), (const GLvoid*)cmd_list->VtxBuffer.Data, GL_STREAM_DRAW);

                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_array_id);
                glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx), (const GLvoid*)cmd_list->IdxBuffer.Data, GL_STREAM_DRAW);

                for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++) {
                        const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
                        if (pcmd->UserCallback) {
                                // User callback (registered via ImDrawList::AddCallback)
                                pcmd->UserCallback(cmd_list, pcmd);
                        }
                        else {
                                ImVec4 clip_rect = ImVec4(pcmd->ClipRect.x - pos.x, pcmd->ClipRect.y - pos.y, pcmd->ClipRect.z - pos.x, pcmd->ClipRect.w - pos.y);
                                if (clip_rect.x < fb_width &&
                                    clip_rect.y < fb_height &&
                                    clip_rect.z >= 0.0f &&
                                    clip_rect.w >= 0.0f) {

                                        // Apply scissor/clipping rectangle
                                        glScissor((int)clip_rect.x,
                                                  (int)(fb_height - clip_rect.w),
                                                  (int)(clip_rect.z - clip_rect.x),
                                                  (int)(clip_rect.w - clip_rect.y));

                                        // Bind texture, Draw
                                        oGraphicsState.SetTexture(SE::TextureUnit::DIFFUSE, (TTexture *)(intptr_t)pcmd->GetTexID());
                                        oGraphicsState.Draw(
                                                        vao_id,
                                                        GL_TRIANGLES,
                                                        sizeof(ImDrawIdx) == 2 ?
                                                                VertexIndexType::SHORT :
                                                                VertexIndexType::INT,
                                                        index_offset,
                                                        pcmd->ElemCount);
                                }
                        }
                        index_offset += pcmd->ElemCount;
                }
        }

        //TODO move to GraphicsState
        glBlendEquationSeparate(last_blend_equation_rgb, last_blend_equation_alpha);
        glBlendFuncSeparate(last_blend_src_rgb, last_blend_dst_rgb, last_blend_src_alpha, last_blend_dst_alpha);
        if (last_enable_blend)
                glEnable(GL_BLEND);
        else
                glDisable(GL_BLEND);
        if (last_enable_cull_face)
                glEnable(GL_CULL_FACE);
        else
                glDisable(GL_CULL_FACE);
        if (last_enable_depth_test)
                glEnable(GL_DEPTH_TEST);
        else
                glDisable(GL_DEPTH_TEST);
        if (last_enable_scissor_test)
                glEnable(GL_SCISSOR_TEST);
        else
                glDisable(GL_SCISSOR_TEST);
        glScissor(last_scissor_box[0], last_scissor_box[1], (GLsizei)last_scissor_box[2], (GLsizei)last_scissor_box[3]);

}

void ImGuiWrapper::NewFrame(const Event & oEvent [[maybe_unused]]) {

        auto screen_size = GetSystem<GraphicsState>().GetScreenSize();

        ImGuiIO          & io           = ImGui::GetIO();
        IM_ASSERT(io.Fonts->IsBuilt());

        io.DisplaySize                  = ImVec2((float)screen_size.x, (float)screen_size.y);
        io.DisplayFramebufferScale      = ImVec2(screen_size.x > 0 ? 1 : 0, screen_size.y > 0 ? 1 : 0);

        io.DeltaTime = GetSystem<AppClock>().RawDelta();

        auto pos = GetSystem<InputManager>().GetMousePos();
        io.MousePos = ImVec2((float)pos.x, (float)pos.y);

        //TODO switch cursor

        ImGui::NewFrame();
}

void ImGuiWrapper::OnKeyDown(const Event & oEvent) {

        ImGuiIO & io = ImGui::GetIO();
        auto & ev = oEvent.Get<EKeyDown>();

        io.AddKeyEvent(ScancodeToImGuiKey(ev.scancode), true);
        io.AddKeyEvent(ImGuiMod_Shift, (ev.mod & Keymods::SHIFT) != 0);
        io.AddKeyEvent(ImGuiMod_Ctrl,  (ev.mod & Keymods::CTRL)  != 0);
        io.AddKeyEvent(ImGuiMod_Alt,   (ev.mod & Keymods::ALT)   != 0);
}

void ImGuiWrapper::OnKeyUp(const Event & oEvent) {

        ImGuiIO & io = ImGui::GetIO();
        auto & ev = oEvent.Get<EKeyUp>();

        io.AddKeyEvent(ScancodeToImGuiKey(ev.scancode), false);
        io.AddKeyEvent(ImGuiMod_Shift, (ev.mod & Keymods::SHIFT) != 0);
        io.AddKeyEvent(ImGuiMod_Ctrl,  (ev.mod & Keymods::CTRL)  != 0);
        io.AddKeyEvent(ImGuiMod_Alt,   (ev.mod & Keymods::ALT)   != 0);
}

void ImGuiWrapper::OnTextInput(const Event & oEvent) {

        ImGui::GetIO().AddInputCharactersUTF8(oEvent.Get<ETextInput>().text);
}

void ImGuiWrapper::OnMouseMove(const Event & oEvent) {

        ImGuiIO & io = ImGui::GetIO();
        auto & ev = oEvent.Get<EMouseMove>();

        if (io.WantSetMousePos) {
                GetSystem<InputManager>().SetMousePos({(int)io.MousePos.x, (int)io.MousePos.y});
        }

        io.MousePos = ImVec2((float)ev.pos.x, (float)ev.pos.y);
}

void ImGuiWrapper::OnMouseButtonDown(const Event & oEvent) {

        ImGuiIO & io = ImGui::GetIO();

        switch (oEvent.Get<EMouseButtonDown>().button) {
                case MouseB::LEFT:   io.MouseDown[0] = true; break;
                case MouseB::RIGHT:  io.MouseDown[1] = true; break;
                case MouseB::MIDDLE: io.MouseDown[2] = true; break;
                default: break;
        }
}

void ImGuiWrapper::OnMouseButtonUp(const Event & oEvent) {

        ImGuiIO & io = ImGui::GetIO();

        switch (oEvent.Get<EMouseButtonUp>().button) {
                case MouseB::LEFT:   io.MouseDown[0] = false; break;
                case MouseB::RIGHT:  io.MouseDown[1] = false; break;
                case MouseB::MIDDLE: io.MouseDown[2] = false; break;
                default: break;
        }
}

} //namespace HELPERS
} //SE
