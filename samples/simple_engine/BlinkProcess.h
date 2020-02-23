
#ifndef BLINK_PROCESS
#define BLINK_PROCESS

namespace SE {

class BlinkProcess : public WorldProcess {

        TSceneTree::TSceneNodeWeak      pTargetNode;
        const float                     flip_dt;
        float                           cur_dt{0};
        bool                            on{true};

        public:

        BlinkProcess(TSceneTree::TSceneNodeWeak pNewTargetNode, const float new_flip_dt, allocator_type oAlloc);
        void OnUpdate(const float dt) override;
};

}

#endif
