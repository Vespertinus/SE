
namespace SE {

BasicController::BasicController(TSceneTree::TSceneNodeExact * pNewNode) : pNode(pNewNode), sName(pNode->GetFullName() + "|BasicController") {

}

BasicController::~BasicController() noexcept {

        Disable();
}

bool BasicController::keyPressed( const OIS::KeyEvent &ev) {

        //log_d("key: {}", ev.key);

        switch (ev.key) {

                case OIS::KC_W:
                        translation_speed.z -= speed;
                        break;
                case OIS::KC_S:	
                        translation_speed.z += speed;
                        break;
                case OIS::KC_D:	
                        translation_speed.x += speed;
                        break;
                case OIS::KC_A:
                        translation_speed.x -= speed;
                        break;
                case OIS::KC_R:
                        translation_speed.y += speed;
                        break;
                case OIS::KC_F:
                        translation_speed.y -= speed;
                        break;
                case OIS::KC_ESCAPE:
                        log_i("stop program");
                        exit(0);
                        //return false;
                        //TODO write correct exit

                case OIS::KC_Q:
                        rotation_speed.y += 15;
                        break;
                case OIS::KC_E:
                        rotation_speed.y -= 15;
                        break;
                case OIS::KC_Z:
                        speed *= 2;
                        break;
                case OIS::KC_X:
                        speed /= 2;
                        if (speed < 0) { speed = 0.001; }

                        break;
                default:
                        break;

        }
        return true;
}



bool BasicController::keyReleased( const OIS::KeyEvent &ev) {

        //log_d("key: {}", ev.key);

        switch (ev.key) {

                case OIS::KC_W:
                case OIS::KC_S:	
                        translation_speed.z = 0.0f;
                        break;
                case OIS::KC_D:	
                case OIS::KC_A:
                        translation_speed.x = 0.0f;
                        break;
                case OIS::KC_R:
                        translation_speed.y = 0.0f;
                        break;
                case OIS::KC_F:
                        translation_speed.y = 0.0f;
                        break;

                default:
                        break;

        }

        return true;
}

bool BasicController::mouseMoved( const OIS::MouseEvent &ev) {

        rotation_speed.x += (cursor_y - ev.state.Y.abs) * 0.05;
        rotation_speed.y += (cursor_x - ev.state.X.abs) * 0.05;

        cursor_x = ev.state.X.abs;
        cursor_y = ev.state.Y.abs;

        //FIXME after switching from OIS to SDL2
        OIS::MouseState &MutableMouseState = const_cast<OIS::MouseState &>(TInputManager::Instance().GetMouse()->getMouseState());
        const auto & screen_size = GetSystem<GraphicsState>().GetScreenSize();

        if (ev.state.X.abs >= screen_size.x - 1.0) {

                MutableMouseState.X.abs = 1;
                cursor_x                = 1;
        }

        if (ev.state.Y.abs >= screen_size.y - 1.0) {
                MutableMouseState.Y.abs = 1;
                cursor_y                = 1;
        }
        if (ev.state.X.abs == 0.0) {
                MutableMouseState.X.abs = screen_size.x;
                cursor_x                = screen_size.x;
        }

        if (ev.state.Y.abs == 0.0) {
                MutableMouseState.Y.abs = screen_size.y;
                cursor_y                = screen_size.y;
        }

        return true;
}

bool BasicController::mousePressed( const OIS::MouseEvent &ev, OIS::MouseButtonID id) {

        return true;
}

bool BasicController::mouseReleased( const OIS::MouseEvent &ev, OIS::MouseButtonID id) {

        return true;
}

void BasicController::Update(const Event & oEvent) {

        //So we can fix the first-person camera with the mantra "Pitch Locally, Yaw Globally"
        //https://gamedev.stackexchange.com/questions/136174/im-rotating-an-object-on-two-axes-so-why-does-it-keep-twisting-around-the-thir

        const auto & oEventData = oEvent.Get<EInputUpdate>();

        pNode->Rotate     (glm::vec3(0.0f, rotation_speed.y, 0.0f)); //yaw
        pNode->RotateLocal(glm::vec3(rotation_speed.x, 0.0f, 0.0f)); //pitch

        pNode->TranslateLocal(translation_speed * oEventData.last_frame_time);

        rotation_speed = {0.0f, 0.0f, 0.0f};
}

void BasicController::Enable() {

        TInputManager::Instance().AddKeyListener   (this, sName);
        TInputManager::Instance().AddMouseListener (this, sName);

        TEngine::Instance().Get<EventManager>().AddListener<EInputUpdate, &BasicController::Update>(this);
}

void BasicController::Disable() {

        TInputManager::Instance().RemoveKeyListener   (sName);
        TInputManager::Instance().RemoveMouseListener (sName);

        TEngine::Instance().Get<EventManager>().RemoveListener<EInputUpdate, &BasicController::Update>(this);
}

std::string BasicController::Str() const {

        return fmt::format("BasicController: speed: {}, delta trans: ({}, {}, {}), rot: ({}, {}, {})",
                        speed,
                        translation_speed.x,
                        translation_speed.y,
                        translation_speed.z,
                        rotation_speed.x,
                        rotation_speed.y,
                        rotation_speed.z);
}

}
