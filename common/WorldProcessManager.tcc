
namespace SE {


WorldProcessManager::WorldProcessManager(allocator_type oNewAlloc) : oAlloc(oNewAlloc), vProcesses(oNewAlloc) {

        TEngine::Instance().Get<EventManager>().AddListener<EUpdate, &WorldProcessManager::Update>(this);
}

WorldProcessManager::~WorldProcessManager() noexcept {

        TEngine::Instance().Get<EventManager>().RemoveListener<EUpdate, &WorldProcessManager::Update>(this);
}

template <class T, class ... TArgs> std::shared_ptr<T> WorldProcessManager::Create(TArgs && ... oArgs) {

        auto pItem = std::allocate_shared<T>(oAlloc, std::forward<TArgs>(oArgs)...);

        log_d("'{}' created", typeid(T).name());

        return pItem;
}

template <class T, class ... TArgs> std::shared_ptr<T> WorldProcessManager::CreateAndLink(TArgs && ... oArgs) {

        auto pItem = std::allocate_shared<T>(oAlloc, std::forward<TArgs>(oArgs)...);

        log_d("'{}' added", typeid(T).name());

        vProcesses.emplace_back(pItem);

        return pItem;
}

void WorldProcessManager::Update(const Event & oEvent) {

        const auto & oEventData = oEvent.Get<EUpdate>();
        std::shared_ptr<WorldProcess> pProcess;
        uint32_t index = 0;

        while (index < vProcesses.size()) {

                pProcess = vProcesses[index];

                if (pProcess->GetState() == WorldProcess::State::WAITING) {
                        pProcess->OnInit();
                }

                if (pProcess->GetState() == WorldProcess::State::RUNNING) {
                        pProcess->OnUpdate(oEventData.last_frame_time);
                }

                if (pProcess->IsDead()) {

                        switch(pProcess->GetState()) {

                                case WorldProcess::State::SUCCEEDED:
                                        {
                                                pProcess->OnSuccess();
                                                auto pChild = pProcess->ReleaseChild();
                                                if (pChild) {
                                                        LinkProcess(pChild);
                                                }
                                                break;
                                        }

                                case WorldProcess::State::FAILED:
                                        {

                                                pProcess->OnFail();
                                                break;
                                        }
                                case WorldProcess::State::ABORTED:
                                        {
                                                pProcess->OnAbort();
                                                break;
                                        }
                                default:
                                        se_assert(0);
                        }

                        if (UnlinkProcess(index)) {
                                continue;
                        }

                }

                ++index;
        }
}

void WorldProcessManager::LinkProcess(std::shared_ptr<WorldProcess> & pProcess) {

        vProcesses.emplace_back(pProcess);
}

void WorldProcessManager::Clean() {

        vProcesses.clear();
}

void WorldProcessManager::Abort() {

        uint32_t index = 0;

        while (index < vProcesses.size()) {

                if (vProcesses[index]->IsAlive()) {

                        vProcesses[index]->Abort();
                        vProcesses[index]->OnAbort();

                        if (UnlinkProcess(index)) {
                                continue;
                        }
                }

                ++index;
        }
}

bool WorldProcessManager::UnlinkProcess(const uint32_t index) {

        if (index == vProcesses.size() - 1) {
                vProcesses.pop_back();
        }
        else {
                vProcesses[index] = vProcesses.back();
                vProcesses.pop_back();
                return true;
        }

        return false;
}

uint32_t WorldProcessManager::Count() const {

        return vProcesses.size();
}


}
