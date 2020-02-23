
#include <GlobalTypes.h>
#include <WorldProcess.h>
#include <BlinkProcess.h>

namespace SE {

BlinkProcess::BlinkProcess(TSceneTree::TSceneNodeWeak pNewTargetNode, const float new_flip_dt, allocator_type oAlloc) :
        pTargetNode(pNewTargetNode),
        flip_dt(new_flip_dt) {

}



void BlinkProcess::OnUpdate(const float dt) {

        cur_dt += dt;

        if (on) {

                if (cur_dt >= flip_dt) {

                        if (auto pNode = pTargetNode.lock()) {

                                pNode->Disable();
                                on = false;
                                cur_dt = 0.0f;
                        }
                        else {
                                log_w ("failed to get target node");
                                Fail();
                        }
                }
        }
        else {
                if (cur_dt >= flip_dt) {

                        if (auto pNode = pTargetNode.lock()) {

                                pNode->Enable();
                                on = true;
                                cur_dt = 0.0f;
                        }
                        else {
                                log_w ("failed to get target node");
                                Fail();
                        }
                }

        }
}

}
