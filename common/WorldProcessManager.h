
#ifndef WORLD_PROCESS_MANAGER
#define WORLD_PROCESS_MANAGER

#include <experimental/memory_resource>
#include <experimental/vector>


namespace SE {

class WorldProcess;

class WorldProcessManager {

        public:

        using allocator_type = std::experimental::pmr::polymorphic_allocator<std::byte>;

        private:
        allocator_type oAlloc;
        std::experimental::pmr::vector<std::shared_ptr<WorldProcess> > vProcesses;

        void LinkProcess(std::shared_ptr<WorldProcess> & pProcess);
        void UnlinkProcess(const uint32_t index);

        public:

        WorldProcessManager(allocator_type oNewAlloc = {});

        ~WorldProcessManager() noexcept;
        template <class T, class ... TArgs> std::shared_ptr<T> Create(TArgs && ... oArgs);
        template <class T, class ... TArgs> std::shared_ptr<T> CreateAndLink(TArgs && ... oArgs);
        //uint32_t Remove(std::shared_ptr<WorldProcess> pProcess); through cancel | Fail |Abort
        void Update(const float dt);
        //copy constructor
        //move constructor
        void Clean();
        void Abort();
        uint32_t Count() const;
};

}

#endif
