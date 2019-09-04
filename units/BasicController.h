#ifndef __BASIC_CONTROLLER_H__
#define __BASIC_CONTROLLER_H__ 1

#include <InputManager.h>

namespace SE {

class BasicController : public OIS::KeyListener, public OIS::MouseListener {

        //static const glm::vec3 rot_right{0, 0,  5};
        //static const glm::vec3 rot_left {0, 0, -5};

        TSceneTree::TSceneNodeExact   * pNode;
        std::string                     sName;
        glm::vec3                       translation_speed{0, 0, 0};
        glm::vec3                       rotation_speed   {0, 0, 0};
        /*speed per sec*/
        float                           speed{1};
        int32_t	                        cursor_x{0},
                                        cursor_y{0};

        public:

        BasicController(TSceneTree::TSceneNodeExact * pNewNode);
        ~BasicController() noexcept;

        bool keyPressed( const OIS::KeyEvent &ev);
        bool keyReleased( const OIS::KeyEvent &ev);

        bool mouseMoved( const OIS::MouseEvent &ev);
        bool mousePressed( const OIS::MouseEvent &ev, OIS::MouseButtonID id);
        bool mouseReleased( const OIS::MouseEvent &ev, OIS::MouseButtonID id);

        void                    Update(const Event & oEvent);
        void                    Enable();
        void                    Disable();
        void                    Print(const size_t indent);
        std::string             Str() const;
        void                    DrawDebug() const;
};


} //namespace SE


#endif

