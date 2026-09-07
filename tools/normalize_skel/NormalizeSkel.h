#pragma once

#include <AnimationSkeleton_generated.h>
#include <AnimationClip_generated.h>
#include <AnimationGraph_generated.h>
#include <SceneTree_generated.h>
#include <Component_generated.h>

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace SE::TOOLS::NormSkel {

// Maps bone name → canonical local bind quaternion (from the reference skeleton)
using CanonicalMap = std::unordered_map<std::string, glm::quat>;

// Bind positions indexed by bone_index (matches skeleton bone array order)
using BindPosVec = std::vector<glm::vec3>;

// Build canonical map from the reference skeleton file
CanonicalMap BuildCanonicalMap(const SE::FlatBuffers::SkeletonT& ref_skel);

// Op A: replace bind_rot with canonical and recompute inv_bind_matrix for every joint.
// Joints not found in canon are left at their original rotation.
// Joints must be topologically sorted (parent_index < bone_index).
void NormalizeSkeletonBones(SE::FlatBuffers::SkeletonT& skel, const CanonicalMap& canon);

// Op A: walk the scene and update every AnimatedModel's joints_inv_bind_pose
// to match the already-normalized skeleton identified by skel_path.
void UpdateSceneInvBinds(SE::FlatBuffers::SceneTreeT& scene,
                         const std::string& skel_path,
                         const SE::FlatBuffers::SkeletonT& normalized_skel);

// Op B: build per-bone-index correction map.
// correction[j] = q_canon[bone_name] * inverse(q_src[j])
// Only bones present in both the source skeleton and canonical are included.
// Must be called BEFORE NormalizeSkeletonBones (uses original src bind rotations).
std::unordered_map<uint16_t, glm::quat>
BuildCorrectionMap(const SE::FlatBuffers::SkeletonT& src_skel, const CanonicalMap& canon);

// Op B: apply per-bone correction to rotation channels (targets 3-6) and convert
// translation channels (targets 0-2) to delta-from-bind-pose format.
// Sets clip.delta_translations = true so the runtime adds deltas to the
// character's own bind pose instead of overwriting it.
void RetargetClip(SE::FlatBuffers::AnimationClipT& clip,
                  const std::unordered_map<uint16_t, glm::quat>& correction,
                  const BindPosVec& src_bind_pos);

// Op B: find all path-referenced .seak clips in the scene's Animator components,
// load each one, retarget it, and write it back.
// resource_root is prepended to the asset-relative paths stored in the scene.
void RetargetSceneClips(const SE::FlatBuffers::SceneTreeT& scene,
                        const std::unordered_map<uint16_t, glm::quat>& correction,
                        const BindPosVec& src_bind_pos,
                        const std::string& resource_root);

// I/O helpers — all throw std::runtime_error on failure
std::unique_ptr<SE::FlatBuffers::SkeletonT>      LoadSkeleton (const std::string& path);
std::unique_ptr<SE::FlatBuffers::SceneTreeT>     LoadScene    (const std::string& path);
std::unique_ptr<SE::FlatBuffers::AnimationClipT> LoadClip     (const std::string& path);
std::unique_ptr<SE::FlatBuffers::AnimationGraphT>LoadAnimGraph(const std::string& path);

void SaveSkeleton(const std::string& path, const SE::FlatBuffers::SkeletonT& skel);
void SaveScene   (const std::string& path, const SE::FlatBuffers::SceneTreeT& scene);
void SaveClip    (const std::string& path, const SE::FlatBuffers::AnimationClipT& clip);

} // namespace SE::TOOLS::NormSkel
