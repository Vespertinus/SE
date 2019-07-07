
namespace SE {

BasicController::BasicController(TSceneTree::TSceneNodeExact * pNewNode, bool enabled) : pNode(pNewNode), sName(pNode->GetFullName() + "|BasicController") {

        if (enabled) {
                Enable();
        }
}

BasicController::~BasicController() noexcept {

        Disable();
}

bool BasicController::keyPressed( const OIS::KeyEvent &ev) {

        log_d("key: {}", ev.key);

        switch (ev.key) {

                case OIS::KC_W:
                        //w_x_delta = speed * 1.35 * sin (*rot_z / 180.0 * M_PI);
                        //w_y_delta = speed * 1.35 * cos (*rot_z / 180.0 * M_PI);

                        //*delta_x += w_x_delta;
                        //*delta_y += w_y_delta;
                        translation_speed.z -= speed;
                        break;
                case OIS::KC_S:	
                        //s_x_delta = speed * 1.35 * sin (*rot_z / 180.0 * M_PI);
                        //s_y_delta = speed * 1.35 * cos (*rot_z / 180.0 * M_PI);

                        //*delta_x -= s_x_delta;
                        //*delta_y -= s_y_delta;
                        translation_speed.z += speed;
                        break;
                case OIS::KC_D:	
                        //d_x_delta = speed * 1.35 * sin ( (*rot_z + 90.0) / 180.0 * M_PI);
                        //d_y_delta = speed * 1.35 * cos ( (*rot_z + 90.0) / 180.0 * M_PI);

                        //*delta_x += d_x_delta;
                        //*delta_y += d_y_delta;
                        translation_speed.x += speed;
                        break;
                case OIS::KC_A:
                        //a_x_delta = speed * 1.35 * sin ( (*rot_z - 90.0) / 180.0 * M_PI);
                        //a_y_delta = speed * 1.35 * cos ( (*rot_z - 90.0) / 180.0 * M_PI);

                        //*delta_x += a_x_delta;
                        //*delta_y += a_y_delta;
                        translation_speed.x -= speed;
                        break;
                case OIS::KC_R:
                        //*delta_z	+=	speed;
                        translation_speed.y += speed;
                        break;
                case OIS::KC_F:
                        //*delta_z	-=	speed;
                        translation_speed.y -= speed;
                        break;
                case OIS::KC_ESCAPE:
                        log_i("stop program");
                        exit(0);
                        //return false;
                        //TODO write correct exit

                case OIS::KC_Q:
                        //*rot_z -= 5;
                        //if (*rot_z < 0.0)   *rot_z = 359.0;
                        rotation_speed.y += 15;
                        break;
                case OIS::KC_E:
                        //*rot_z += 5;
                        //if (*rot_z > 359.0) *rot_z = 0.0;
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

        log_d("key: {}", ev.key);

        switch (ev.key) {

                case OIS::KC_W:
                case OIS::KC_S:	
                        /**delta_x -= w_x_delta;
                        *delta_y -= w_y_delta;
                        w_x_delta = 0;
                        w_y_delta = 0;*/
                        translation_speed.z = 0.0f;
                        break;
                        /**delta_x += s_x_delta;
                        *delta_y += s_y_delta;
                        s_x_delta = 0;
                        s_y_delta = 0;*/
                        //break;
                case OIS::KC_D:	
                case OIS::KC_A:
                        /**delta_x -= d_x_delta;
                        *delta_y -= d_y_delta;
                        d_x_delta = 0;
                        d_y_delta = 0;*/
                        //break;
                        /**delta_x -= a_x_delta;
                        *delta_y -= a_y_delta;
                        a_x_delta = 0;
                        a_y_delta = 0;*/
                        translation_speed.x = 0.0f;
                        break;
                case OIS::KC_R:
                        //*delta_z	-=	speed;
                        translation_speed.y = 0.0f;
                        break;
                case OIS::KC_F:
                        //*delta_z	+=	speed;
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
        const auto & screen_size = TGraphicsState::Instance().GetScreenSize();

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

void BasicController::Update(const FrameState & oFrame) {

        //So we can fix the first-person camera with the mantra "Pitch Locally, Yaw Globally"
        //https://gamedev.stackexchange.com/questions/136174/im-rotating-an-object-on-two-axes-so-why-does-it-keep-twisting-around-the-thir

        auto translation_delta = translation_speed * oFrame.last_frame_time;
        //auto rotation_delta = rotation_speed * oFrame.last_frame_time;
        auto rotation_delta = rotation_speed;
        log_d("last_frame_time: {}", oFrame.last_frame_time);
        log_d("translation delta: ({}, {}, {})", translation_delta.x, translation_delta.y, translation_delta.z);
        log_d("rotation_delta: ({}, {}, {})", rotation_delta.x, rotation_delta.y, rotation_delta.z);

        log_d("angles before: ({}, {}, {})",
                        pNode->GetTransform().GetRotationDeg().x,
                        pNode->GetTransform().GetRotationDeg().y,
                        pNode->GetTransform().GetRotationDeg().z);

        //pNode->Rotate(rotation_speed * oFrame.last_frame_time);
        //pNode->Rotate(rotation_delta);

        //glm::vec3 vRotationDeg = pNode->GetTransform().GetRotationDeg() + rotation_delta;
        //glm::quat qNewRotation(glm::radians(vRotationDeg));
        //pNode->SetRotation(qNewRotation);

        /*
        glm::quat qYawDelta(glm::radians(glm::vec3(0.0f, rotation_delta.y, 0.0f)));
        glm::quat qPitchDelta(glm::radians(glm::vec3(rotation_delta.x, 0.0f, 0.0f)));

        glm::quat qNewRotation  = glm::normalize(qYawDelta * pNode->GetTransform().GetRotation());
        qNewRotation            = glm::normalize(qNewRotation * qPitchDelta);
        pNode->SetRotation(qNewRotation);
        */

        pNode->Rotate     (glm::vec3(0.0f, rotation_delta.y, 0.0f)); //yaw
        pNode->RotateLocal(glm::vec3(rotation_delta.x, 0.0f, 0.0f)); //pitch


        //glm::quat qDeltaRot(glm::radians(rotation_delta));
        //pNode->SetRotation(glm::normalize(pNode->GetTransform().GetRotation() * qDeltaRot));
        log_d("angles after: ({}, {}, {})",
                        pNode->GetTransform().GetRotationDeg().x,
                        pNode->GetTransform().GetRotationDeg().y,
                        pNode->GetTransform().GetRotationDeg().z);

        pNode->TranslateLocal(translation_speed * oFrame.last_frame_time);
        //auto vCurRotation = pNode->GetTransform().GetRotation();
        //pNode->Translate(vCurRotation * (translation_speed * oFrame.last_frame_time));
        //auto vDelta = vCurRotation * (translation_speed * oFrame.last_frame_time);
        //log_d("updated translation_delta: ({}, {}, {})", vDelta.x, vDelta.y, vDelta.z);

        rotation_speed = {0.0f, 0.0f, 0.0f};
        //translation_speed = {0,0,0};
}

void BasicController::Enable() {

        TInputManager::Instance().AddKeyListener   (this, sName);
        TInputManager::Instance().AddMouseListener (this, sName);
}

void BasicController::Disable() {

        TInputManager::Instance().RemoveKeyListener   (sName);
        TInputManager::Instance().RemoveMouseListener (sName);
}

std::string BasicController::Str() const {

        return fmt::format("BasicController:");
}

}
