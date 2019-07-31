
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
                Camera::Settings oCamSettings;
        };

        private:
        SE::Camera            * pCamera;
        SE::TSceneTree        * pSceneTree;
        Settings                oSettings;
        SE::HELPERS::ImGuiWrapper            oImGui;

        void ShowGUI();

        public:
        Scene(const Settings & oNewSettings);
        ~Scene() throw();

        void Process();
};


} //namespace TOOLS
} //SE

#endif


