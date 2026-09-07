#pragma once

#include <string>
#include <vector>
#include <optional>
#include <unordered_map>
#include <array>

namespace SE::TOOLS {

struct SceneRef {
        std::string scene;   // path to .sesc file
        std::string node;    // slash-separated node path within that scene
};

struct AssetRef {
        enum class Kind { None, Path, Named, Scene } kind = Kind::None;
        std::string path;        // Kind::Path  — file path
        std::string named;       // Kind::Named — name without leading '@'
        SceneRef    scene_ref;   // Kind::Scene

        bool IsEmpty()    const { return kind == Kind::None;  }
        bool IsPath()     const { return kind == Kind::Path;  }
        bool IsNamed()    const { return kind == Kind::Named; }
        bool IsSceneRef() const { return kind == Kind::Scene; }

        static AssetRef FromPath(std::string p)    { AssetRef r; r.kind = Kind::Path;  r.path  = std::move(p); return r; }
        static AssetRef FromNamed(std::string n)   { AssetRef r; r.kind = Kind::Named; r.named = std::move(n); return r; }
        static AssetRef FromScene(SceneRef ref)    { AssetRef r; r.kind = Kind::Scene; r.scene_ref = std::move(ref); return r; }
};

struct ComponentSpec {
        std::unordered_map<std::string, AssetRef>              fields;
        std::unordered_map<std::string, std::vector<AssetRef>> array_fields;
};

struct NodeSpec {
        std::string name;
        AssetRef    from;   // copy base node (and its subtree) from another scene
        std::optional<std::array<float, 3>> translation;
        std::optional<std::array<float, 3>> rotation;
        std::optional<std::array<float, 3>> scale;
        std::unordered_map<std::string, ComponentSpec> components; // comp type → spec
        std::vector<std::string> remove_components;
        std::vector<NodeSpec>    children;
};

// Targets a single node inside a base_scene by slash-separated path
struct NodeOverride {
        std::string                                      node;              // e.g. "Root/Soldier"
        std::unordered_map<std::string, ComponentSpec>   components;
        std::vector<std::string>                         remove_components;
};

struct Manifest {
        int                                          version = 1;
        std::string                                  output;
        std::unordered_map<std::string, AssetRef>    assets;      // name → ref (path or scene)
                                                                  // Option A: start with an existing scene and apply targeted patches
        std::string                                  base_scene;  // path to source .sesc
        std::vector<NodeOverride>                    overrides;   // patches applied by node path
                                                                  // Option B: build a new scene from scratch (may be combined with base_scene)
        std::vector<NodeSpec>                        nodes;       // appended as children of root
};

} // namespace SE::TOOLS
