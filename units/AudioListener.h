
#ifndef __AUDIO_LISTENER_H__
#define __AUDIO_LISTENER_H__ 1

#include <string>
#include <Audio_generated.h>

namespace SE {

/**
 * AudioListener — marker component.
 * Attach to the camera/player node. Registers itself as a transform listener
 * and posts EvtUpdateListener to the audio thread whenever the node moves.
 */
class AudioListener {

        TSceneTree::TSceneNodeExact* pNode;
        float                        master_gain;

public:
        using TSerialized = FlatBuffers::AudioListener;

        AudioListener(TSceneTree::TSceneNodeExact* pNode, float master_gain = 1.0f);
        AudioListener(TSceneTree::TSceneNodeExact* pNode, const SE::FlatBuffers::AudioListener* pFB);
        ~AudioListener() noexcept;

        void Enable()  {}
        void Disable() {}

        void TargetTransformChanged(TSceneTree::TSceneNodeExact* pNode);

        float MasterGain() const { return master_gain; }

        std::string Str() const;
        void DrawDebug() const {}
};

} // namespace SE

#endif
