
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
        SE::Camera                    * pCamera;
        SE::TSceneTree::TSceneNode      pCameraNode;
        SE::H<SE::TSceneTree>           hSceneTree;
        SE::TSceneTree                * pSceneTree { nullptr };
        Settings                        oSettings;
        SE::HELPERS::ImGuiWrapper       oImGui;
        bool                            toggle_controller{false};

        void ShowGUI();
        void OnMouseButtonUp(const Event & oEvent);

        public:
        Scene(const Settings & oNewSettings);
        ~Scene() noexcept;

        void Process();


};


} //namespace TOOLS
} //SE

#endif


