
#ifndef __SCENE_H__
#define __SCENE_H__ 1

#include <ImGui.h>

namespace SE {
namespace TOOLS {

class Scene {

        public:
        struct Settings {
                const std::string & sScenePath;
                bool  vdebug;
                bool  enable_all;
                //bool  ortho {false};
                Camera::Settings oCamSettings;
        };

        private:
        SE::Camera            * pCamera;
        SE::BasicController   * pController;
        SE::TSceneTree        * pSceneTree;
        Settings                oSettings;
        SE::HELPERS::ImGuiWrapper            oImGui;

        void ShowGUI();

        public:
        Scene(const Settings & oNewSettings);
        ~Scene() throw();

        void Process();
        void PostRender();
};


} //namespace TOOLS
} //SE

#endif


