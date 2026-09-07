#pragma once

#include "Manifest.h"

#include <SceneTree_generated.h>
#include <Component_generated.h>
#include <AnimationGraph_generated.h>
#include <AnimationSkeleton_generated.h>
#include <Mesh_generated.h>
#include <Material_generated.h>
#include <Audio_generated.h>
#include <AudioClip_generated.h>
#include <StateMachine_generated.h>

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace SE::TOOLS {

class SceneComposer; // forward declaration for applier signatures

using FieldApplier      = std::function<void(SE::FlatBuffers::ComponentT&,
                                             const AssetRef&,
                                             SceneComposer&)>;
using ArrayFieldApplier = std::function<void(SE::FlatBuffers::ComponentT&,
                                             const std::vector<AssetRef>&,
                                             SceneComposer&)>;

struct ComponentDef {
        SE::FlatBuffers::ComponentU                              type_tag;
        std::function<void(SE::FlatBuffers::ComponentUUnion&)>   make;
        std::unordered_map<std::string, FieldApplier>            fields;
        std::unordered_map<std::string, ArrayFieldApplier>       array_fields;
};

class SceneComposer {
public:
        explicit SceneComposer(const Manifest& manifest, bool print_scenes = false);

        bool Compose();

private:
        const Manifest& m_manifest;
        bool            m_print_scenes = false;

        std::unordered_map<std::string,
                std::unique_ptr<SE::FlatBuffers::SceneTreeT>> m_scene_cache;

        SE::FlatBuffers::SceneTreeT& LoadOrGetScene(const std::string& path);

        SE::FlatBuffers::NodeT* FindNode(SE::FlatBuffers::NodeT& root,
                        const std::string& slash_path);

        std::unique_ptr<SE::FlatBuffers::NodeT> BuildNode(const NodeSpec& spec);

        void ApplyComponentSpec(SE::FlatBuffers::NodeT& node,
                        const std::string&      comp_type,
                        const ComponentSpec&    spec);

        void RemoveComponent(SE::FlatBuffers::NodeT& node, const std::string& comp_type);

        // Generic holder resolver — specialised per holder type in SceneComposer.cpp
        template<typename THolder>
                std::unique_ptr<THolder> Resolve(const AssetRef& ref);

        // Resolve @named asset refs through manifest.assets
        const AssetRef& ResolveNamed(const std::string& name) const;

        static const std::unordered_map<std::string, ComponentDef>& GetRegistry();

        template<typename TComp, typename THolder>
                static FieldApplier MakeApplier(std::unique_ptr<THolder> TComp::* member);

        template<typename TComp, typename THolder>
                static ArrayFieldApplier MakeArrayApplier(std::vector<std::unique_ptr<THolder>> TComp::* member);

        void PrintSceneTree(const SE::FlatBuffers::SceneTreeT& tree, const std::string& path) const;
};

} // namespace SE::TOOLS
