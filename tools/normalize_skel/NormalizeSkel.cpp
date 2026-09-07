#include "NormalizeSkel.h"

#include <Logging.h>
#include <flatbuffers/flatbuffers.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <set>
#include <stdexcept>

namespace SE::TOOLS::NormSkel {

// ---------------------------------------------------------------------------
// File helpers
// ---------------------------------------------------------------------------

static std::vector<uint8_t> ReadFile(const std::string& path) {
        std::ifstream ifs(path, std::ios::binary | std::ios::ate);
        if (!ifs)
                throw std::runtime_error("normalize_skel: cannot open '" + path + "'");
        const auto size = static_cast<std::size_t>(ifs.tellg());
        ifs.seekg(0);
        std::vector<uint8_t> buf(size);
        ifs.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(size));
        return buf;
}

static void WriteFile(const std::string& path, const uint8_t* data, std::size_t size) {
        std::ofstream ofs(path, std::ios::binary | std::ios::trunc);
        if (!ofs)
                throw std::runtime_error("normalize_skel: cannot write '" + path + "'");
        ofs.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
}

std::unique_ptr<SE::FlatBuffers::SkeletonT> LoadSkeleton(const std::string& path) {
        auto buf = ReadFile(path);
        flatbuffers::Verifier v(buf.data(), buf.size());
        if (!SE::FlatBuffers::VerifySkeletonBuffer(v))
                throw std::runtime_error("normalize_skel: bad skeleton FlatBuffer in '" + path + "'");
        return std::unique_ptr<SE::FlatBuffers::SkeletonT>(
                        SE::FlatBuffers::GetSkeleton(buf.data())->UnPack());
}

std::unique_ptr<SE::FlatBuffers::SceneTreeT> LoadScene(const std::string& path) {
        auto buf = ReadFile(path);
        flatbuffers::Verifier v(buf.data(), buf.size());
        if (!SE::FlatBuffers::VerifySceneTreeBuffer(v))
                throw std::runtime_error("normalize_skel: bad scene FlatBuffer in '" + path + "'");
        return std::unique_ptr<SE::FlatBuffers::SceneTreeT>(
                        SE::FlatBuffers::GetSceneTree(buf.data())->UnPack());
}

std::unique_ptr<SE::FlatBuffers::AnimationClipT> LoadClip(const std::string& path) {
        auto buf = ReadFile(path);
        // .seak files written without file identifier — skip identifier check
        return std::unique_ptr<SE::FlatBuffers::AnimationClipT>(
                        SE::FlatBuffers::GetAnimationClip(buf.data())->UnPack());
}

std::unique_ptr<SE::FlatBuffers::AnimationGraphT> LoadAnimGraph(const std::string& path) {
        auto buf = ReadFile(path);
        flatbuffers::Verifier v(buf.data(), buf.size());
        if (!SE::FlatBuffers::VerifyAnimationGraphBuffer(v))
                throw std::runtime_error("normalize_skel: bad animation graph in '" + path + "'");
        return std::unique_ptr<SE::FlatBuffers::AnimationGraphT>(
                        SE::FlatBuffers::GetAnimationGraph(buf.data())->UnPack());
}

void SaveSkeleton(const std::string& path, const SE::FlatBuffers::SkeletonT& skel) {
        flatbuffers::FlatBufferBuilder builder(64 * 1024);
        auto offset = SE::FlatBuffers::Skeleton::Pack(builder, &skel);
        SE::FlatBuffers::FinishSkeletonBuffer(builder, offset);
        WriteFile(path, builder.GetBufferPointer(), builder.GetSize());
        log_i("normalize_skel: wrote skeleton '{}' ({} bones)", path, skel.bones.size());
}

void SaveScene(const std::string& path, const SE::FlatBuffers::SceneTreeT& scene) {
        flatbuffers::FlatBufferBuilder builder(1 << 20);
        auto offset = SE::FlatBuffers::SceneTree::Pack(builder, &scene);
        SE::FlatBuffers::FinishSceneTreeBuffer(builder, offset);
        WriteFile(path, builder.GetBufferPointer(), builder.GetSize());
        log_i("normalize_skel: wrote scene '{}'", path);
}

void SaveClip(const std::string& path, const SE::FlatBuffers::AnimationClipT& clip) {
        flatbuffers::FlatBufferBuilder builder(256 * 1024);
        auto offset = SE::FlatBuffers::AnimationClip::Pack(builder, &clip);
        SE::FlatBuffers::FinishAnimationClipBuffer(builder, offset);
        WriteFile(path, builder.GetBufferPointer(), builder.GetSize());
        log_i("normalize_skel: wrote clip '{}' ({} channels)", path, clip.channels.size());
}

// ---------------------------------------------------------------------------
// Canonical map
// ---------------------------------------------------------------------------

CanonicalMap BuildCanonicalMap(const SE::FlatBuffers::SkeletonT& ref_skel) {
        CanonicalMap result;
        for (const auto& pBone : ref_skel.bones) {
                if (!pBone || !pBone->bind_rot) continue;
                const auto& r = *pBone->bind_rot;
                result[pBone->name] = glm::quat(r.w(), r.x(), r.y(), r.z());
        }
        return result;
}

// ---------------------------------------------------------------------------
// Op A — skeleton normalization
// ---------------------------------------------------------------------------

static glm::mat4 MakeTRS(const glm::vec3& t, const glm::quat& r, const glm::vec3& s) {
        return glm::translate(glm::mat4(1.f), t) *
                glm::mat4_cast(r) *
                glm::scale(glm::mat4(1.f), s);
}

static float QuatAngleDeg(const glm::quat& a, const glm::quat& b) {
        const float d = std::min(1.0f, std::abs(glm::dot(a, b)));
        return glm::degrees(2.0f * std::acos(d));
}

void NormalizeSkeletonBones(SE::FlatBuffers::SkeletonT& skel, const CanonicalMap& canon) {
        using namespace SE::FlatBuffers;
        const int N = static_cast<int>(skel.bones.size());
        std::vector<glm::mat4> world(N, glm::mat4(1.f));

        int matched = 0, skipped = 0;
        float max_delta = 0.f;
        constexpr float kLogThresholdDeg = 0.1f;

        for (int j = 0; j < N; j++) {
                auto& pBone = skel.bones[j];
                if (!pBone) continue;

                glm::vec3 pos(0.f), scl(1.f);
                glm::quat rot(1.f, 0.f, 0.f, 0.f);

                if (pBone->bind_pos)
                        pos = glm::vec3(pBone->bind_pos->x(), pBone->bind_pos->y(), pBone->bind_pos->z());
                if (pBone->bind_scale)
                        scl = glm::vec3(pBone->bind_scale->x(), pBone->bind_scale->y(), pBone->bind_scale->z());
                if (pBone->bind_rot) {
                        const auto& r = *pBone->bind_rot;
                        rot = glm::quat(r.w(), r.x(), r.y(), r.z());
                }

                auto it = canon.find(pBone->name);
                if (it != canon.end()) {
                        const float delta = QuatAngleDeg(rot, it->second);
                        max_delta = std::max(max_delta, delta);
                        if (delta > kLogThresholdDeg)
                                log_d("  bone[{}] '{}': delta {:.2f} deg", j, pBone->name, delta);
                        rot = it->second;
                        pBone->bind_rot = std::make_unique<Vec4>(rot.x, rot.y, rot.z, rot.w);
                        matched++;
                } else {
                        skipped++;
                }

                const glm::mat4 local = MakeTRS(pos, rot, scl);
                world[j] = (pBone->parent_index == 0xFFFF)
                        ? local
                        : world[pBone->parent_index] * local;

                const glm::mat4 inv = glm::inverse(world[j]);
                pBone->inv_bind_matrix = std::make_unique<ColMat4>(
                                Vec4{inv[0][0], inv[0][1], inv[0][2], inv[0][3]},
                                Vec4{inv[1][0], inv[1][1], inv[1][2], inv[1][3]},
                                Vec4{inv[2][0], inv[2][1], inv[2][2], inv[2][3]},
                                Vec4{inv[3][0], inv[3][1], inv[3][2], inv[3][3]});
        }

        log_i("NormalizeSkeletonBones: {}/{} bones matched canonical, {} skipped, max delta {:.2f} deg",
                        matched, N, skipped, max_delta);
}

// Decompose a stored ColMat4 inverse-bind matrix to a BindSQTT
static std::unique_ptr<SE::FlatBuffers::BindSQTT>
ColMat4ToBindSQT(const SE::FlatBuffers::ColMat4& cm) {
        using namespace SE::FlatBuffers;
        glm::mat4 m;
        m[0] = {cm.col0().x(), cm.col0().y(), cm.col0().z(), cm.col0().w()};
        m[1] = {cm.col1().x(), cm.col1().y(), cm.col1().z(), cm.col1().w()};
        m[2] = {cm.col2().x(), cm.col2().y(), cm.col2().z(), cm.col2().w()};
        m[3] = {cm.col3().x(), cm.col3().y(), cm.col3().z(), cm.col3().w()};

        const glm::vec3 pos(m[3]);
        const glm::vec3 scl(glm::length(glm::vec3(m[0])),
                        glm::length(glm::vec3(m[1])),
                        glm::length(glm::vec3(m[2])));
        const glm::mat3 rot_mat(glm::vec3(m[0]) / scl.x,
                        glm::vec3(m[1]) / scl.y,
                        glm::vec3(m[2]) / scl.z);
        const glm::quat q = glm::quat_cast(rot_mat);

        auto sqt = std::make_unique<BindSQTT>();
        sqt->bind_pos   = std::make_unique<Vec3>(pos.x, pos.y, pos.z);
        sqt->bind_rot   = std::make_unique<Vec4>(q.x, q.y, q.z, q.w);
        sqt->bind_scale = std::make_unique<Vec3>(scl.x, scl.y, scl.z);
        return sqt;
}

static void UpdateNodeInvBinds(SE::FlatBuffers::NodeT& node,
                const std::string& skel_path,
                const SE::FlatBuffers::SkeletonT& skel) {
        using namespace SE::FlatBuffers;
        for (auto& pComp : node.components) {
                if (!pComp) continue;
                auto* pAM = pComp->component.AsAnimatedModel();
                if (!pAM || !pAM->skeleton) continue;
                const auto& holder = *pAM->skeleton;
                if (holder.path != skel_path && holder.name != skel_path) continue;

                const std::size_t k_count = pAM->joints_indexes.size();
                pAM->joints_inv_bind_pose.resize(k_count);
                for (std::size_t k = 0; k < k_count; k++) {
                        const uint16_t j = pAM->joints_indexes[k];
                        if (j >= skel.bones.size() || !skel.bones[j] || !skel.bones[j]->inv_bind_matrix)
                                continue;
                        pAM->joints_inv_bind_pose[k] = ColMat4ToBindSQT(*skel.bones[j]->inv_bind_matrix);
                }
        }
        for (auto& pChild : node.children)
                if (pChild) UpdateNodeInvBinds(*pChild, skel_path, skel);
}

void UpdateSceneInvBinds(SE::FlatBuffers::SceneTreeT& scene,
                const std::string& skel_path,
                const SE::FlatBuffers::SkeletonT& normalized_skel) {
        if (scene.root)
                UpdateNodeInvBinds(*scene.root, skel_path, normalized_skel);
}

// ---------------------------------------------------------------------------
// Op B — clip retargeting
// ---------------------------------------------------------------------------

std::unordered_map<uint16_t, glm::quat>
BuildCorrectionMap(const SE::FlatBuffers::SkeletonT& src_skel, const CanonicalMap& canon) {
        std::unordered_map<uint16_t, glm::quat> result;
        float max_delta = 0.f;
        constexpr float kLogThresholdDeg = 0.1f;

        for (std::size_t j = 0; j < src_skel.bones.size(); j++) {
                const auto& pBone = src_skel.bones[j];
                if (!pBone || !pBone->bind_rot) continue;
                auto it = canon.find(pBone->name);
                if (it == canon.end()) continue;
                const auto& r  = *pBone->bind_rot;
                const glm::quat q_src(r.w(), r.x(), r.y(), r.z());
                const glm::quat correction = it->second * glm::inverse(q_src);
                const float delta = QuatAngleDeg(q_src, it->second);
                max_delta = std::max(max_delta, delta);
                if (delta > kLogThresholdDeg)
                        log_d("  bone[{}] '{}': correction {:.2f} deg", j, pBone->name, delta);
                result[static_cast<uint16_t>(j)] = correction;
        }

        log_i("BuildCorrectionMap: {} corrections, max delta {:.2f} deg", result.size(), max_delta);
        return result;
}

void RetargetClip(SE::FlatBuffers::AnimationClipT& clip,
                const std::unordered_map<uint16_t, glm::quat>& correction,
                const BindPosVec& src_bind_pos) {
        using namespace SE::FlatBuffers;

        // --- Rotation retargeting (targets 3-6) ---
        std::unordered_map<uint16_t, std::array<CurveChannelT*, 4>> rot;
        for (auto& pCh : clip.channels) {
                if (!pCh) continue;
                const uint8_t t = pCh->target;
                if (t < 3 || t > 6) continue;
                if (!correction.count(pCh->bone_index)) continue;
                rot[pCh->bone_index][t - 3] = pCh.get();
        }

        for (auto& [bone_idx, ch] : rot) {
                auto* cX = ch[0]; auto* cY = ch[1]; auto* cZ = ch[2]; auto* cW = ch[3];
                if (!cX || !cY || !cZ || !cW) continue;

                const std::size_t N = cX->values.size();
                if (cY->values.size() != N || cZ->values.size() != N || cW->values.size() != N) continue;

                const glm::quat c = correction.at(bone_idx);

                for (std::size_t k = 0; k < N; k++) {
                        const glm::quat q(cW->values[k], cX->values[k], cY->values[k], cZ->values[k]);
                        const glm::quat qn = c * q;
                        cX->values[k] = qn.x; cY->values[k] = qn.y;
                        cZ->values[k] = qn.z; cW->values[k] = qn.w;
                }

                // Hermite tangents are derivatives — same quaternion rotation applies
                const std::size_t T = cX->tangents.size();
                if (T == 2 * N &&
                                cY->tangents.size() == T && cZ->tangents.size() == T && cW->tangents.size() == T) {
                        for (std::size_t k = 0; k < T; k++) {
                                const glm::quat tq(cW->tangents[k], cX->tangents[k], cY->tangents[k], cZ->tangents[k]);
                                const glm::quat tn = c * tq;
                                cX->tangents[k] = tn.x; cY->tangents[k] = tn.y;
                                cZ->tangents[k] = tn.z; cW->tangents[k] = tn.w;
                        }
                }
        }

        // --- Translation delta conversion (targets 0-2) ---
        // Subtract the source skeleton's bind position so the clip stores movement
        // RELATIVE to the bind pose.  At runtime, SampleClip adds the delta to the
        // target character's bind pose (seeded by InitBindPose), so each character
        // uses its own proportions while still following the animation's intent.
        // Tangents are positional derivatives — they are NOT shifted by a constant.
        for (auto& pCh : clip.channels) {
                if (!pCh || pCh->target > 2) continue;
                const uint16_t bi = pCh->bone_index;
                if (bi >= static_cast<uint16_t>(src_bind_pos.size())) continue;
                const float bind_val = src_bind_pos[bi][pCh->target];
                for (float& v : pCh->values)
                        v -= bind_val;
        }

        clip.delta_translations = true;

        // Store source pelvis height (bone 1) for uniform proportional scaling at runtime.
        clip.src_pelvis_scale = (src_bind_pos.size() > 1) ? glm::length(src_bind_pos[1]) : 0.0f;
}

// Collect unique .seak clip paths from a graph's flat node list
static void CollectClipPaths(const SE::FlatBuffers::AnimationGraphT& graph,
                std::set<std::string>& out_paths) {
        using namespace SE::FlatBuffers;
        for (const auto& pState : graph.states) {
                if (!pState) continue;
                for (const auto& pNode : pState->nodes) {
                        if (!pNode) continue;
                        const auto* cd = pNode->data.AsClipNodeData();
                        if (!cd || !cd->clip || cd->clip->path.empty()) continue;
                        out_paths.insert(cd->clip->path);
                }
        }
}

static void CollectClipPathsFromNode(const SE::FlatBuffers::NodeT& node,
                std::set<std::string>& paths,
                const std::string& resource_root) {
        using namespace SE::FlatBuffers;
        for (const auto& pComp : node.components) {
                if (!pComp) continue;
                const auto* pAnim = pComp->component.AsAnimator();
                if (!pAnim || !pAnim->animation_graph) continue;
                const auto& holder = *pAnim->animation_graph;

                if (holder.graph)
                        CollectClipPaths(*holder.graph, paths);

                if (!holder.path.empty()) {
                        try {
                                auto graph = LoadAnimGraph(resource_root + "/" + holder.path);
                                CollectClipPaths(*graph, paths);
                        } catch (const std::exception& e) {
                                log_e("normalize_skel: skipping graph '{}': {}", holder.path, e.what());
                        }
                }
        }
        for (const auto& pChild : node.children)
                if (pChild) CollectClipPathsFromNode(*pChild, paths, resource_root);
}

void RetargetSceneClips(const SE::FlatBuffers::SceneTreeT& scene,
                const std::unordered_map<uint16_t, glm::quat>& correction,
                const BindPosVec& src_bind_pos,
                const std::string& resource_root) {
        if (!scene.root) return;

        std::set<std::string> clip_paths;
        CollectClipPathsFromNode(*scene.root, clip_paths, resource_root);

        if (clip_paths.empty()) {
                log_i("normalize_skel: no path-referenced clips found in scene");
                return;
        }

        for (const auto& path : clip_paths) {
                try {
                        auto clip = LoadClip(resource_root + "/" + path);
                        if (clip->delta_translations) {
                                log_i("normalize_skel: '{}' already retargeted, skipping", path);
                                continue;
                        }
                        RetargetClip(*clip, correction, src_bind_pos);
                        SaveClip(resource_root + "/" + path, *clip);
                } catch (const std::exception& e) {
                        log_e("normalize_skel: failed to retarget '{}': {}", path, e.what());
                }
        }
}

} // namespace SE::TOOLS::NormSkel
