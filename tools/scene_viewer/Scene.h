
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
        };

        private:
        SE::Camera            & oCamera;
        SE::TSceneTree        * pSceneTree;
        Settings                oSettings;
        SE::HELPERS::ImGuiWrapper            oImGui;

        void ShowGUI();

        public:
        Scene(const Settings & oNewSettings, SE::Camera & oCurCamera);
        ~Scene() throw();

        void Process();
};


} //namespace TOOLS
} //SE

#endif


