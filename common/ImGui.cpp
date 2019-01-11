

// Internal include
#include <Global.h>
#include <GlobalTypes.h>
//SE wrapper over imgui
#include <ImGui.h>


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
}

ImGuiWrapper::~ImGuiWrapper() {

        ImGuiIO& io = ImGui::GetIO();
        io.Fonts->TexID = 0;
        ImGui::DestroyContext();

        glDeleteBuffers(1, &vbo_id);
        glDeleteBuffers(1, &index_array_id);
        glDeleteVertexArrays(1, &vao_id);
}

void ImGuiWrapper::InitWndData() {

        ImGuiIO& io = ImGui::GetIO();
        io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;         // We can honor GetMouseCursor() values (optional)
        io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;          // We can honor io.WantSetMousePos requests (optional, rarely used)

        //fill keys mapping
        io.KeyMap[ImGuiKey_Tab]         = OIS::KC_TAB;
        io.KeyMap[ImGuiKey_LeftArrow]   = OIS::KC_LEFT;
        io.KeyMap[ImGuiKey_RightArrow]  = OIS::KC_RIGHT;
        io.KeyMap[ImGuiKey_UpArrow]     = OIS::KC_UP;
        io.KeyMap[ImGuiKey_DownArrow]   = OIS::KC_DOWN;
        io.KeyMap[ImGuiKey_PageUp]      = OIS::KC_PGUP;
        io.KeyMap[ImGuiKey_PageDown]    = OIS::KC_PGDOWN;
        io.KeyMap[ImGuiKey_Home]        = OIS::KC_HOME;
        io.KeyMap[ImGuiKey_End]         = OIS::KC_END;
        io.KeyMap[ImGuiKey_Insert]      = OIS::KC_INSERT;
        io.KeyMap[ImGuiKey_Delete]      = OIS::KC_DELETE;
        io.KeyMap[ImGuiKey_Backspace]   = OIS::KC_BACK;
        io.KeyMap[ImGuiKey_Space]       = OIS::KC_SPACE;
        io.KeyMap[ImGuiKey_Enter]       = OIS::KC_RETURN;
        io.KeyMap[ImGuiKey_Escape]      = OIS::KC_ESCAPE;
        io.KeyMap[ImGuiKey_A]           = OIS::KC_A;
        io.KeyMap[ImGuiKey_C]           = OIS::KC_C;
        io.KeyMap[ImGuiKey_V]           = OIS::KC_V;
        io.KeyMap[ImGuiKey_X]           = OIS::KC_X;
        io.KeyMap[ImGuiKey_Y]           = OIS::KC_Y;
        io.KeyMap[ImGuiKey_Z]           = OIS::KC_Z;
}

void ImGuiWrapper::InitGLData() {

        pShader  = CreateResource<SE::ShaderProgram>(GetSystem<Config>().sResourceDir + "shader_program/imgui.sesp");

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

        glVertexAttribPointer(pos_location, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), (GLvoid*)IM_OFFSETOF(ImDrawVert, pos));
        glVertexAttribPointer(uv_location, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), (GLvoid*)IM_OFFSETOF(ImDrawVert, uv));
        glVertexAttribPointer(color_location, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(ImDrawVert), (GLvoid*)IM_OFFSETOF(ImDrawVert, col));

        glBindVertexArray(0);
}

void ImGuiWrapper::BuildFontTex() {

        ImGuiIO       & io = ImGui::GetIO();
        uint8_t       * pPixels;
        int32_t         width,
                        height;

        io.Fonts->GetTexDataAsRGBA32(&pPixels, &width, &height);

        pFontTex = CreateResource<SE::TTexture>(
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

        io.Fonts->TexID = (ImTextureID)pFontTex;
}

void ImGuiWrapper::Render() {

        ImGui::Render();
        ImDrawData * pDrawData = ImGui::GetDrawData();
        ImGuiIO & io           = ImGui::GetIO();

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

        TGraphicsState::Instance().SetVao(vao_id);
        TGraphicsState::Instance().SetShaderProgram(pShader);
        TGraphicsState::Instance().SetVariable(mat_id, mOrtho);

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
                                        TGraphicsState::Instance().SetTexture(SE::TextureUnit::DIFFUSE, (TTexture *)pcmd->TextureId);
                                        TGraphicsState::Instance().Draw(
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

void ImGuiWrapper::NewFrame() {

        const glm::uvec2 & screen_size  = TGraphicsState::Instance().GetScreenSize();
        ImGuiIO          & io           = ImGui::GetIO();
        IM_ASSERT(io.Fonts->IsBuilt());

        io.DisplaySize                  = ImVec2((float)screen_size.x, (float)screen_size.y);
        io.DisplayFramebufferScale      = ImVec2(screen_size.x > 0 ? 1 : 0, screen_size.y > 0 ? 1 : 0);

        io.DeltaTime = TGraphicsState::Instance().GetLastFrameTime();

        const auto & oMouseState = TInputManager::Instance().GetMouse()->getMouseState();
        io.MousePos = ImVec2((float)oMouseState.X.abs, (float)oMouseState.Y.abs);

        //TODO switch cursor

        ImGui::NewFrame();
}

bool ImGuiWrapper::mouseMoved( const OIS::MouseEvent &ev) {

        ImGuiIO & io = ImGui::GetIO();


        if (io.WantSetMousePos) {
                OIS::MouseState & MutableMouseState = const_cast<OIS::MouseState &>(TInputManager::Instance().GetMouse()->getMouseState());
                MutableMouseState.X.abs = io.MousePos.x;
                MutableMouseState.Y.abs = io.MousePos.y;
        }

        io.MousePos = ImVec2((float)ev.state.X.abs, (float)ev.state.Y.abs);

        return true;
}

bool ImGuiWrapper::mousePressed( const OIS::MouseEvent &ev, OIS::MouseButtonID id) {

        ImGuiIO & io = ImGui::GetIO();

        switch (id) {

                case OIS::MB_Left:
                        io.MouseDown[0] = true;
                        break;
                case OIS::MB_Right:
                        io.MouseDown[1] = true;
                        break;
                case OIS::MB_Middle:
                        io.MouseDown[2] = true;
                        break;
                default:
                        break;
        }

        return true;
}

bool ImGuiWrapper::mouseReleased( const OIS::MouseEvent &ev, OIS::MouseButtonID id) {

        ImGuiIO & io = ImGui::GetIO();

        switch (id) {

                case OIS::MB_Left:
                        io.MouseDown[0] = false;
                        break;
                case OIS::MB_Right:
                        io.MouseDown[1] = false;
                        break;
                case OIS::MB_Middle:
                        io.MouseDown[2] = false;
                        break;
                default:
                        break;
        }

        return true;
}

bool ImGuiWrapper::keyPressed( const OIS::KeyEvent &ev) {

        ImGuiIO & io = ImGui::GetIO();

        IM_ASSERT(ev.key >= 0 && ev.key < IM_ARRAYSIZE(io.KeysDown));
        io.KeysDown[ev.key] = true;

        auto * pKeyboard = TInputManager::Instance().GetKeyboard();

        io.KeyShift =  (pKeyboard->isModifierDown(OIS::Keyboard::Shift) == true);
        io.KeyCtrl  =  (pKeyboard->isModifierDown(OIS::Keyboard::Ctrl)  == true);
        io.KeyAlt   =  (pKeyboard->isModifierDown(OIS::Keyboard::Alt)   == true);

        if (ev.text > 0 && ev.text < 0x10000) {
                io.AddInputCharacter((uint16_t)ev.text);
        }

        return true;
}

bool ImGuiWrapper::keyReleased( const OIS::KeyEvent &ev) {

        ImGuiIO & io = ImGui::GetIO();

        IM_ASSERT(ev.key >= 0 && ev.key < IM_ARRAYSIZE(io.KeysDown));
        io.KeysDown[ev.key] = false;

        auto * pKeyboard = TInputManager::Instance().GetKeyboard();

        io.KeyShift =  (pKeyboard->isModifierDown(OIS::Keyboard::Shift) == true);
        io.KeyCtrl  =  (pKeyboard->isModifierDown(OIS::Keyboard::Ctrl)  == true);
        io.KeyAlt   =  (pKeyboard->isModifierDown(OIS::Keyboard::Alt)   == true);

        return true;
}

} //namespace HELPERS
} //SE
