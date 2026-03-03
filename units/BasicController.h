#ifndef __BASIC_CONTROLLER_H__
#define __BASIC_CONTROLLER_H__ 1

namespace SE {

class BasicController {

        //static const glm::vec3 rot_right{0, 0,  5};
        //static const glm::vec3 rot_left {0, 0, -5};

        TSceneTree::TSceneNodeExact   * pNode;
        std::string                     sName;
        glm::vec3                       translation_speed{0, 0, 0};
        glm::vec3                       rotation_speed   {0, 0, 0};
        /*speed per sec*/
        float                           speed{1};

        void OnKeyDown(const Event & oEvent);
        void OnKeyUp(const Event & oEvent);
        void OnMouseMove(const Event & oEvent);

        public:

        BasicController(TSceneTree::TSceneNodeExact * pNewNode);
        ~BasicController() noexcept;

        void                    Update(const Event & oEvent);
        void                    Enable();
        void                    Disable();
        void                    Print(const size_t indent);
        std::string             Str() const;
        void                    DrawDebug() const;
};


} //namespace SE


#endif

