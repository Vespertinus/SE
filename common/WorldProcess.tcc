
namespace SE {
        
WorldProcess::~WorldProcess() noexcept {

        if (pChild) {
                pChild->OnAbort();
        }
};

void WorldProcess::OnInit() {

        cur_state = State::RUNNING;
};

State WorldProcess::GetState() const {
        return cur_state;
}

bool WorldProcess::IsAlive() const {
        return (cur_state == State::RUNNING || cur_state == State::PAUSED);
}

bool WorldProcess::IsDead() const {
        return (cur_state == State::SUCCEEDED || cur_state == State::FAILED || cur_state == State::ABORTED);
}

bool WorldProcess::IsRemoved() const {
        return (cur_state == State::REMOVED);
}

bool WorldProcess::IsPaused() const {
        return (cur_state == State::PAUSED);
}

void WorldProcess::SetChild(TSharedProcess & pNewChild) {
        pChild = pNewChild;
}

TSharedProcess WorldProcess::GetChild() {
        return pChild;
}

TSharedProcess WorldProcess::ReleaseChild() {

        if (!pChild) { return {}; }

        TSharedProcess pRetChild = pChild;
        pChild.reset();

        return pRetChild;
}

void WorldProcess::Succeed() {
        
        se_assert(cur_state == State::RUNNING || cur_state == State::PAUSED);
        cur_state = State::RUNNING;
}

void WorldProcess::Fail() {

        //THINK allow in WAITING state
        se_assert(cur_state == State::RUNNING || cur_state == State::PAUSED);
        cur_state = State::FAILED;
}

void WorldProcess::Abort() {
        se_assert(cur_state == State::RUNNING || cur_state == State::PAUSED);
        cur_state = State::ABORTED;
}

void WorldProcess::Pause() {

        if (cur_state == State::RUNNING) {
        
                cur_state = State::PAUSED;
        }
        else {
                log_w("cant't pause process in state: {}, type: '{}'", static_cast<uint8_t>(cur_state), typeid(this).name());
        }
}

void WorldProcess::UnPause() {

        if (cur_state == State::PAUSED) {
        
                cur_state = State::RUNNING;
        }
        else {
                log_w("cant't unpause process in state: {}, type: '{}'", static_cast<uint8_t>(cur_state), typeid(this).name());
        }
}

}
