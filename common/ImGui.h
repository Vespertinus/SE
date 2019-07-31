#ifndef __IMGUI_WRAPPER_H__
#define __IMGUI_WRAPPER_H__ 1

#include <InputManager.h>
#include <imgui.h>

namespace SE {
namespace HELPERS {

class ImGuiWrapper : public OIS::KeyListener, public OIS::MouseListener {


        ShaderProgram         * pShader;
        TTexture              * pFontTex;
        uint32_t                vao_id,
                                vbo_id,
                                index_array_id;

        void BuildFontTex();
        void InitGLData();
        void InitWndData();

        public:

        ImGuiWrapper();
        ~ImGuiWrapper();

        bool keyPressed( const OIS::KeyEvent &ev);
        bool keyReleased( const OIS::KeyEvent &ev);

        bool mouseMoved( const OIS::MouseEvent &ev);
        bool mousePressed( const OIS::MouseEvent &ev, OIS::MouseButtonID id);
        bool mouseReleased( const OIS::MouseEvent &ev, OIS::MouseButtonID id);

        void NewFrame(const Event &);
        void Render(const Event &);

};


} //namespace HELPERS
} //SE

#endif



