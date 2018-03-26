
#ifndef __SCENE_H__
#define __SCENE_H__ 1

namespace SE {
namespace TOOLS {

class Scene {

        SE::Camera      & oCamera;
        SE::TSceneTree  * pSceneTree;

        public:
        struct Settings {
                const std::string & sScenePath;
        };

        Scene(const Settings & oSettings, SE::Camera & oCurCamera);
        ~Scene() throw();

        void Process();
};


} //namespace TOOLS 
} //SE

#endif


