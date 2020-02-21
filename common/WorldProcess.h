
#ifndef WORLD_PROCESS
#define WORLD_PROCESS

#include <experimental/memory_resource>

namespace SE {

class WorldProcess {

        friend class WorldProcessManager;

        public:

        using allocator_type = std::experimental::pmr::polymorphic_allocator<std::byte>;
        using TSharedProcess = std::shared_ptr<WorldProcess>;

        enum class State : uint8_t {
                WAITING         = 0,
                RUNNING         = 1,
                PAUSED          = 2,
                SUCCEEDED       = 3,
                FAILED          = 4,
                ABORTED         = 5,
                REMOVED         = 6
        };

        private:
        TSharedProcess pChild;
        State          cur_state{State::WAITING};

        public:

        virtual ~WorldProcess() noexcept;

        void            SetChild(TSharedProcess & pNewChild);
        TSharedProcess  ReleaseChild();
        TSharedProcess  GetChild();

        void            Succeed();
        void            Fail();
        void            Abort();
        void            Pause();
        void            UnPause();

        State           GetState() const;
        bool            IsAlive() const;
        bool            IsDead() const;
        bool            IsRemoved() const;
        bool            IsPaused() const;

        protected:

        virtual void    OnInit();
        virtual void    OnUpdate(const float dt) = 0;
        virtual void    OnSuccess() {};
        virtual void    OnFail() {};
        virtual void    OnAbort() {};

};


}

#endif
