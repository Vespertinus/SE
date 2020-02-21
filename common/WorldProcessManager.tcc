
namespace SE {


WorldProcessManager::WorldProcessManager(allocator_type oNewAlloc) : oAlloc(oNewAlloc), vProcesses(oNewAlloc) {

}

WorldProcessManager::~WorldProcessManager() noexcept {

}

template <class T, class ... TArgs> std::shared_ptr<T> WorldProcessManager::Create(TArgs && ... oArgs) {

        auto pItem = std::allocate_shared<T>(oAlloc, std::forward<TArgs>(oArgs)...);

        return pItem;
}

template <class T, class ... TArgs> std::shared_ptr<T> WorldProcessManager::CreateAndLink(TArgs && ... oArgs) {

        auto pItem = std::allocate_shared<T>(oAlloc, std::forward<TArgs>(oArgs)...);

        log_d("'{}' added", typeid(T).name());

        vProcesses.emplace_back(pItem);

        return pItem;
}

uint32_t WorldProcessManager::RemoveProcess(std::shared_ptr<WorldProcess> pProcess) {

        auto it = std::find_if(
                        vProcesses.begin(),
                        vProcesses.end(),
                        [&pProcess](std::shared_ptr<WorldProcess> const & pCurProcess) {

                                return pProcess.get() == pCurProcess.get();
                        });

        if (it != vProcesses.end()) {
                *it = vProcesses.back();
                vProcesses.pop_back();

                log_d("'{}' removed", typeid(T).name());
                return 1;
        }

        return 0;
}

void WorldProcessManager::Update(const float dt) {

        /*
        for (auto pProcess : vProcesses) {
                pProcess->OnUpdate(dt);
        }
        */

        std::shared_ptr<WorldProcess> pProcess;
        uint32_t index = 0;

        while (index < vProcesses.size()) {

                pProcess = vProcesses[index];

                if (pProcess->GetState() == WorldProcess::State::WAITING) {
                        pProcess->OnInit();
                }

                if (pProcess->GetState() == WorldProcess::State::RUNNING) {
                        pProcess->OnUpdate(dt);
                }

                if (pProcess->IsDead()) {

                        switch(pCurProcess->GetState()) {

                                case WorldProcess::State::SUCCEEDED:
                                        {
                                                pCurProcess->OnSuccess();
                                                auto pChild = pCurProcess->ReleaseChild();
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
