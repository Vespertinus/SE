#pragma once

#include <string>
#include <unordered_map>

namespace SE::TOOLS {

struct ClipSettings {
        bool loop{false};
};

struct SceneImportManifest {
        bool        skip_normals    {false};
        bool        skip_material   {false};
        bool        flip_yz         {false};
        std::string cut_path        {};
        std::string replace         {};
        bool        to_scene        {false};
        bool        to_mesh         {false};
        bool        info_prop       {true};
        bool        blendshapes     {false};
        bool        disable_nodes   {false};
        bool        skin            {false};
        bool        animations      {false};
        float       anim_rate       {30.0f};
        bool        skeleton_inline {false};
        bool        inline_all      {false};

        std::unordered_map<std::string, ClipSettings> clips;
};

// Read manifest from a .sceneimport JSON sidecar.
// Returns false if file not found or parse error; out is unchanged on error.
bool ReadManifest(const std::string & path, SceneImportManifest & out);

// Write (or overwrite) a .sceneimport JSON sidecar.
bool WriteManifest(const std::string & path, const SceneImportManifest & m);

// Derive sidecar path: foo/bar.fbx → foo/bar.sceneimport
std::string SidecarPath(const std::string & input_path);

} // namespace SE::TOOLS
