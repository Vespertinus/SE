
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
                std::string sIblIrrPath; ///< irradiance cubemap .ktx  (empty = use renderer default)
                std::string sIblLdPath;  ///< prefiltered specular .ktx (empty = use renderer default)
                float ibl_scale    { 1.0f };  ///< IBL intensity multiplier
                float ibl_rotation { 0.0f };  ///< IBL yaw rotation in degrees (aligns env with scene)
        };

        private:
        SE::Camera                    * pCamera;
        SE::TSceneTree::TSceneNode      pCameraNode;
        SE::H<SE::TSceneTree>           hSceneTree;
        SE::TSceneTree                * pSceneTree { nullptr };
        Settings                        oSettings;
        SE::HELPERS::ImGuiWrapper       oImGui;
        bool                            toggle_controller{false};
        bool                            show_skeleton{false};

        void ShowGUI();
        void DrawSkeletonOverlay();
        void OnMouseButtonUp(const Event & oEvent);

        public:
        Scene(const Settings & oNewSettings);
        ~Scene() noexcept;

        void Process();


};


} //namespace TOOLS
} //SE

#endif


