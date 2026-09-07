#include "SceneImport.h"
#include <Logging.h>

#include <nlohmann/json.hpp>
#include <fstream>

namespace SE::TOOLS {

using TJson = nlohmann::json;

bool ReadManifest(const std::string & path, SceneImportManifest & out) {

        std::ifstream ifs(path);
        if (!ifs) {
                log_d("'{}' not found — using defaults", path);
                return false;
        }
        try {
                TJson j;
                ifs >> j;

                if (j.contains("skip_normals"))   out.skip_normals   = j["skip_normals"].get<bool>();
                if (j.contains("skip_material"))  out.skip_material  = j["skip_material"].get<bool>();
                if (j.contains("flip_yz"))        out.flip_yz        = j["flip_yz"].get<bool>();
                if (j.contains("cut_path"))       out.cut_path       = j["cut_path"].get<std::string>();
                if (j.contains("replace"))        out.replace        = j["replace"].get<std::string>();
                if (j.contains("to_scene"))       out.to_scene       = j["to_scene"].get<bool>();
                if (j.contains("to_mesh"))        out.to_mesh        = j["to_mesh"].get<bool>();
                if (j.contains("info_prop"))      out.info_prop      = j["info_prop"].get<bool>();
                if (j.contains("blendshapes"))    out.blendshapes    = j["blendshapes"].get<bool>();
                if (j.contains("disable_nodes"))  out.disable_nodes  = j["disable_nodes"].get<bool>();
                if (j.contains("skin"))           out.skin           = j["skin"].get<bool>();
                if (j.contains("animations"))     out.animations     = j["animations"].get<bool>();
                if (j.contains("anim_rate"))      out.anim_rate      = j["anim_rate"].get<float>();
                if (j.contains("skeleton_inline"))out.skeleton_inline= j["skeleton_inline"].get<bool>();
                if (j.contains("inline_all"))     out.inline_all     = j["inline_all"].get<bool>();

                if (j.contains("clips")) {
                        for (auto & [name, cs] : j["clips"].items()) {
                                ClipSettings oClip;
                                if (cs.contains("loop")) oClip.loop = cs["loop"].get<bool>();
                                out.clips[name] = oClip;
                        }
                }
        }
        catch (const std::exception & e) {
                log_e("parse error in '{}': {}", path, e.what());
                return false;
        }

        log_i("loaded '{}'", path);
        return true;
}

bool WriteManifest(const std::string & path, const SceneImportManifest & m) {

        TJson j;
        j["skip_normals"]    = m.skip_normals;
        j["skip_material"]   = m.skip_material;
        j["flip_yz"]         = m.flip_yz;
        j["cut_path"]        = m.cut_path;
        j["replace"]         = m.replace;
        j["to_scene"]        = m.to_scene;
        j["to_mesh"]         = m.to_mesh;
        j["info_prop"]       = m.info_prop;
        j["blendshapes"]     = m.blendshapes;
        j["disable_nodes"]   = m.disable_nodes;
        j["skin"]            = m.skin;
        j["animations"]      = m.animations;
        j["anim_rate"]       = m.anim_rate;
        j["skeleton_inline"] = m.skeleton_inline;
        j["inline_all"]      = m.inline_all;

        TJson clips = TJson::object();
        for (auto & [name, cs] : m.clips) {
                clips[name] = { {"loop", cs.loop} };
        }
        j["clips"] = clips;

        std::ofstream ofs(path);
        if (!ofs) {
                log_e("cannot write '{}'", path);
                return false;
        }
        ofs << j.dump(2) << "\n";
        log_i("wrote sidecar '{}'", path);
        return true;
}

std::string SidecarPath(const std::string & input_path) {

        const auto dot = input_path.rfind('.');
        const auto base = (dot != std::string::npos) ? input_path.substr(0, dot) : input_path;
        return base + ".sceneimport";
}

} // namespace SE::TOOLS
