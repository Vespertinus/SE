#include "ManifestReader.h"

#include <Logging.h>
#include <nlohmann/json.hpp>
#include <fstream>

namespace SE::TOOLS {

using TJson = nlohmann::json;

static AssetRef ParseAssetRef(const TJson& j) {
        if (j.is_string()) {
                std::string s = j.get<std::string>();
                if (!s.empty() && s[0] == '@')
                        return AssetRef::FromNamed(s.substr(1));
                return AssetRef::FromPath(s);
        }
        if (j.is_object() && j.contains("scene") && j.contains("node")) {
                SceneRef ref;
                ref.scene = j["scene"].get<std::string>();
                ref.node  = j["node"].get<std::string>();
                return AssetRef::FromScene(std::move(ref));
        }
        return {};
}

static ComponentSpec ParseComponentSpec(const TJson& j) {
        ComponentSpec spec;
        for (auto& [key, val] : j.items()) {
                if (val.is_array()) {
                        for (const auto& item : val)
                                spec.array_fields[key].push_back(ParseAssetRef(item));
                } else {
                        spec.fields[key] = ParseAssetRef(val);
                }
        }
        return spec;
}

static NodeSpec ParseNodeSpec(const TJson& j) {
        
        NodeSpec spec;

        if (j.contains("name"))  spec.name = j["name"].get<std::string>();
        if (j.contains("from"))  spec.from = ParseAssetRef(j["from"]);

        auto read_vec3 = [&](const char* key, std::optional<std::array<float,3>>& out) {
                if (!j.contains(key)) return;
                const auto& a = j[key];
                if (a.is_array() && a.size() == 3)
                        out = { a[0].get<float>(), a[1].get<float>(), a[2].get<float>() };
        };
        read_vec3("translation", spec.translation);
        read_vec3("rotation",    spec.rotation);
        read_vec3("scale",       spec.scale);

        if (j.contains("components")) {
                for (auto& [comp_type, comp_j] : j["components"].items())
                        spec.components[comp_type] = ParseComponentSpec(comp_j);
        }

        if (j.contains("remove_components")) {
                for (const auto& rc : j["remove_components"])
                        spec.remove_components.push_back(rc.get<std::string>());
        }

        if (j.contains("children")) {
                for (const auto& child_j : j["children"])
                        spec.children.push_back(ParseNodeSpec(child_j));
        }

        return spec;
}

bool ReadManifest(const std::string& path, Manifest& out) {
        
        std::ifstream ifs(path);
        if (!ifs) {
                log_e("compose: cannot open manifest '{}'", path);
                return false;
        }

        try {
                TJson j;
                ifs >> j;

                if (j.contains("version"))    out.version    = j["version"].get<int>();
                if (j.contains("output"))     out.output     = j["output"].get<std::string>();
                if (j.contains("base_scene")) out.base_scene = j["base_scene"].get<std::string>();

                if (j.contains("assets")) {
                        for (auto& [name, ref_j] : j["assets"].items())
                                out.assets[name] = ParseAssetRef(ref_j);
                }

                if (j.contains("overrides")) {
                        for (const auto& ov_j : j["overrides"]) {
                                NodeOverride ov;
                                if (ov_j.contains("node")) ov.node = ov_j["node"].get<std::string>();
                                if (ov_j.contains("components")) {
                                        for (auto& [comp_type, comp_j] : ov_j["components"].items())
                                                ov.components[comp_type] = ParseComponentSpec(comp_j);
                                }
                                if (ov_j.contains("remove_components")) {
                                        for (const auto& rc : ov_j["remove_components"])
                                                ov.remove_components.push_back(rc.get<std::string>());
                                }
                                out.overrides.push_back(std::move(ov));
                        }
                }

                if (j.contains("nodes")) {
                        for (const auto& node_j : j["nodes"])
                                out.nodes.push_back(ParseNodeSpec(node_j));
                }
        }
        catch (const std::exception& e) {
                log_e("compose: parse error in '{}': {}", path, e.what());
                return false;
        }

        log_i("compose: loaded manifest '{}' ({} root node(s))", path, out.nodes.size());
        return true;
}

} // namespace SE::TOOLS
