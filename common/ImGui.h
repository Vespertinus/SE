#ifndef __IMGUI_WRAPPER_H__
#define __IMGUI_WRAPPER_H__ 1

#include <imgui.h>

namespace SE {
namespace HELPERS {

class ImGuiWrapper {


        ShaderProgram         * pShader;
        TTexture              * pFontTex;
        uint32_t                vao_id,
                                vbo_id,
                                index_array_id;

        void BuildFontTex();
        void InitGLData();
        void InitWndData();

        void OnKeyDown(const Event & oEvent);
        void OnKeyUp(const Event & oEvent);
        void OnTextInput(const Event & oEvent);
        void OnMouseMove(const Event & oEvent);
        void OnMouseButtonDown(const Event & oEvent);
        void OnMouseButtonUp(const Event & oEvent);

        public:

        ImGuiWrapper();
        ~ImGuiWrapper();

        void NewFrame(const Event &);
        void Render(const Event &);

};


} //namespace HELPERS
} //SE

#endif



