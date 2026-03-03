
namespace SE {

BasicController::BasicController(TSceneTree::TSceneNodeExact * pNewNode) : pNode(pNewNode), sName(pNode->GetFullName() + "|BasicController") {

}

BasicController::~BasicController() noexcept {

        Disable();
}

void BasicController::OnKeyDown(const Event & oEvent) {

        switch (oEvent.Get<EKeyDown>().key) {

                case SDLK_w:
                        translation_speed.z -= speed;
                        break;
                case SDLK_s:
                        translation_speed.z += speed;
                        break;
                case SDLK_d:
                        translation_speed.x += speed;
                        break;
                case SDLK_a:
                        translation_speed.x -= speed;
                        break;
                case SDLK_r:
                        translation_speed.y += speed;
                        break;
                case SDLK_f:
                        translation_speed.y -= speed;
                        break;
                case SDLK_q:
                        rotation_speed.y += 15;
                        break;
                case SDLK_e:
                        rotation_speed.y -= 15;
                        break;
                case SDLK_z:
                        speed *= 2;
                        break;
                case SDLK_x:
                        speed /= 2;
                        if (speed < 0) { speed = 0.001; }
                        break;
                default:
                        break;

        }
}

void BasicController::OnKeyUp(const Event & oEvent) {

        switch (oEvent.Get<EKeyUp>().key) {

                case SDLK_w:
                case SDLK_s:
                        translation_speed.z = 0.0f;
                        break;
                case SDLK_d:
                case SDLK_a:
                        translation_speed.x = 0.0f;
                        break;
                case SDLK_r:
                case SDLK_f:
                        translation_speed.y = 0.0f;
                        break;
                default:
                        break;

        }
}

void BasicController::OnMouseMove(const Event & oEvent) {

        auto & ev = oEvent.Get<EMouseMove>();
        rotation_speed.x += -ev.delta.y * 0.05f;
        rotation_speed.y += -ev.delta.x * 0.05f;
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

        auto & oEM = TEngine::Instance().Get<EventManager>();
        oEM.AddListener<EKeyDown,   &BasicController::OnKeyDown>  (this);
        oEM.AddListener<EKeyUp,     &BasicController::OnKeyUp>    (this);
        oEM.AddListener<EMouseMove, &BasicController::OnMouseMove>(this);

        oEM.AddListener<EInputUpdate, &BasicController::Update>(this);
}

void BasicController::Disable() {

        auto & oEM = TEngine::Instance().Get<EventManager>();
        oEM.RemoveListener<EKeyDown,   &BasicController::OnKeyDown>  (this);
        oEM.RemoveListener<EKeyUp,     &BasicController::OnKeyUp>    (this);
        oEM.RemoveListener<EMouseMove, &BasicController::OnMouseMove>(this);

        oEM.RemoveListener<EInputUpdate, &BasicController::Update>(this);
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

void BasicController::DrawDebug() const { ;; }

}
