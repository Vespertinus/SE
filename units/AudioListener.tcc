
#include <GlobalTypes.h>
#include <AudioListener.h>
#include <AudioSystem.h>
#include <Logging.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace SE {

AudioListener::AudioListener(TSceneTree::TSceneNodeExact* pNewNode,
                             const SE::FlatBuffers::AudioListener* pFB)
        : AudioListener(pNewNode, pFB ? pFB->master_gain() : 1.0f) {}

AudioListener::AudioListener(TSceneTree::TSceneNodeExact* pNewNode,
                             float master_gain)
        : pNode(pNewNode), master_gain(master_gain) {

        // Apply initial master gain to the master bus
        GetSystem<AudioSystem>().SetBusGain(MixBusId::Master, master_gain);
        log_i("AudioListener: attached (master_gain={})", master_gain);

        pNode->AddListener(this);
}

AudioListener::~AudioListener() noexcept {
        pNode->RemoveListener(this);
}

void AudioListener::TargetTransformChanged(TSceneTree::TSceneNodeExact* pNode) {
        auto [vPos, qRot, vScale] = pNode->GetTransform().GetWorldDecomposedQuat();
        glm::vec3 vFwd = qRot * glm::vec3(0.0f, 0.0f, -1.0f);
        glm::vec3 vUp  = qRot * glm::vec3(0.0f, 1.0f,  0.0f);
        GetSystem<AudioSystem>().PostUpdateListener(vPos, vFwd, vUp, glm::vec3(0.0f));
}

std::string AudioListener::Str() const {
        return fmt::format("AudioListener[master_gain={}]", master_gain);
}

} // namespace SE
