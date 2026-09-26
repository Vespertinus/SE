
#include <Logging.h>

namespace SE {

// static
FlatBuffers::ComponentU EntityTemplateUtility::ComponentNameToEnum(std::string_view name) {

        // Kept in sync with the ComponentU union in misc/Component.fbs.
        if (name == "StaticModel")    return FlatBuffers::ComponentU::StaticModel;
        if (name == "AnimatedModel")  return FlatBuffers::ComponentU::AnimatedModel;
        if (name == "RigidBody")      return FlatBuffers::ComponentU::RigidBody;
        if (name == "AudioEmitter")   return FlatBuffers::ComponentU::AudioEmitter;
        if (name == "AudioListener")  return FlatBuffers::ComponentU::AudioListener;
        if (name == "SoundEmitter")   return FlatBuffers::ComponentU::SoundEmitter;
        if (name == "AppComponent")   return FlatBuffers::ComponentU::AppComponent;
        if (name == "Animator")       return FlatBuffers::ComponentU::Animator;
        if (name == "TriggerVolume")  return FlatBuffers::ComponentU::TriggerVolume;
        if (name == "StateMachine")   return FlatBuffers::ComponentU::StateMachine;
        if (name == "TerrainComponent") return FlatBuffers::ComponentU::TerrainComponent;
        if (name == "SpatialEntry")   return FlatBuffers::ComponentU::SpatialEntry;
        if (name == "SpawnPoint")     return FlatBuffers::ComponentU::SpawnPoint;
        if (name == "SpawnVolume")    return FlatBuffers::ComponentU::SpawnVolume;
        if (name == "Population")     return FlatBuffers::ComponentU::Population;
        if (name == "NavAgent")       return FlatBuffers::ComponentU::NavAgent;
        if (name == "PathFollower")   return FlatBuffers::ComponentU::PathFollower;
        if (name == "TriggerBinding") return FlatBuffers::ComponentU::TriggerBinding;
        if (name == "Blackboard")     return FlatBuffers::ComponentU::Blackboard;
        if (name == "BehaviorTree")   return FlatBuffers::ComponentU::BehaviorTree;
        return FlatBuffers::ComponentU::NONE;
}

// static
std::vector<PathSegment> EntityTemplateUtility::ParsePath(std::string_view path) {

        std::vector<PathSegment> segments;

        while (!path.empty()) {

                auto dot_pos = path.find('.');
                std::string_view token = path.substr(0, dot_pos);

                PathSegment seg;

                auto bracket_open = token.find('[');
                if (bracket_open != std::string_view::npos) {
                        auto bracket_close = token.find(']', bracket_open);
                        if (bracket_close == std::string_view::npos) {
                                // Fail the WHOLE path loudly — a truncated path would apply
                                // a prefix of the overrides and silently corrupt state.
                                log_e("EntityTemplateUtility::ParsePath: unclosed '[' in '{}'", path);
                                return {};
                        }
                        seg.name = std::string(token.substr(0, bracket_open));
                        std::string_view inner = token.substr(bracket_open + 1,
                                        bracket_close - bracket_open - 1);
                        bool is_numeric = !inner.empty();
                        for (char c : inner) {
                                if (c < '0' || c > '9') { is_numeric = false; break; }
                        }
                        if (is_numeric && !inner.empty()) {
                                seg.index = 0;
                                for (char c : inner) seg.index = seg.index * 10 + (c - '0');
                        } else if (!inner.empty()) {
                                seg.key = std::string(inner);
                        } else {
                                // "field[]" — an empty key never matches anything; loud failure.
                                log_e("EntityTemplateUtility::ParsePath: empty indexer in '{}'", path);
                                return {};
                        }
                } else {
                        seg.name = std::string(token);
                }

                segments.push_back(std::move(seg));

                if (dot_pos == std::string_view::npos) break;
                path = path.substr(dot_pos + 1);
        }

        return segments;
}

} // namespace SE
