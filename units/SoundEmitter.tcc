
#include <GlobalTypes.h>
#include <SoundEmitter.h>
#include <SoundEventSystem.h>
#include <AudioSystem.h>
#include <GraphicsState.h>
#include <Logging.h>

namespace SE {

SoundEmitter::SoundEmitter(TSceneTree::TSceneNodeExact* pNewNode,
                           const SE::FlatBuffers::SoundEmitter* pFB)
        : pNode(pNewNode) {

        sCueName      = pFB->cue_id()->c_str();
        vCueHierarchy = SoundEventSystem::BuildHierarchy(sCueName);

        if (pFB->auto_play()) {
                Play();
        }

        pNode->AddListener(this);
}

SoundEmitter::SoundEmitter(TSceneTree::TSceneNodeExact* pNewNode, const SoundEmitterDesc& oDesc)
    : pNode(pNewNode), sCueName(oDesc.sCueName) {

        vCueHierarchy = SoundEventSystem::BuildHierarchy(sCueName);

        if (oDesc.auto_play) {
                Play();
        }

        pNode->AddListener(this);
}

SoundEmitter::~SoundEmitter() noexcept {
        pNode->RemoveListener(this);
        if (hVoice.IsValid()) {
                GetSystem<AudioSystem>().Stop(hVoice);
                hVoice = VoiceHandle{};
        }
        GetSystem<SoundEventSystem>().ReleaseEmitter(this);
}

void SoundEmitter::Enable() {
        if (hVoice.IsValid())
                GetSystem<AudioSystem>().Pause(hVoice, false);
}

void SoundEmitter::Disable() {
        if (hVoice.IsValid())
                GetSystem<AudioSystem>().Pause(hVoice, true);
}

void SoundEmitter::TargetTransformChanged(TSceneTree::TSceneNodeExact* /*pNode*/) {
        const float     dt   = GetSystem<AppClock>().Delta();
        const glm::vec3 vPos = pNode->GetTransform().GetWorldPos();

        if (!first_tick && dt > 0.0f)
                vVelocity = (vPos - vPrevPos) / dt;

        vPrevPos   = vPos;
        first_tick = false;

        if (!hVoice.IsValid()) return;
        GetSystem<AudioSystem>().PostUpdateEmitter(hVoice, vPos, vVelocity);
}

glm::vec3 SoundEmitter::GetPosition() const {
        return pNode->GetTransform().GetWorldPos();
}

std::string SoundEmitter::Str() const {
        return fmt::format("SoundEmitter[cue='{}' voice_gen={}]",
                        sCueName, hVoice.generation);
}

} // namespace SE
