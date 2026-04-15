
#ifdef SE_IMPL

#include <Skeleton.h>
#include <Logging.h>

#include <AnimationSkeleton_generated.h>
#include <flatbuffers/flatbuffers.h>

#include <algorithm>
#include <cstring>
#include <fstream>
#include <vector>

namespace SE {

// ---------------------------------------------------------------------------
// Path-based constructor
// ---------------------------------------------------------------------------
Skeleton::Skeleton(const std::string& sName, rid_t rid)
        : ResourceHolder(rid, sName) {

        std::ifstream f(sName, std::ios::binary | std::ios::ate);
        if (!f.is_open()) {
                log_e("Skeleton: failed to open '{}'", sName);
                return;
        }
        size_t sz = static_cast<size_t>(f.tellg());
        f.seekg(0);
        std::vector<uint8_t> buf(sz);
        f.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(sz));

        // Verify "SESK" file identifier
        flatbuffers::Verifier verifier(buf.data(), buf.size());
        if (!SE::FlatBuffers::VerifySkeletonBuffer(verifier)) {
                log_e("Skeleton: FlatBuffer verify failed for '{}'", sName);
                return;
        }

        auto* fb = SE::FlatBuffers::GetSkeleton(buf.data());
        LoadFromFB(fb);
        size = static_cast<uint32_t>(sz);
}

// ---------------------------------------------------------------------------
// Inline FlatBuffer constructor
// ---------------------------------------------------------------------------
Skeleton::Skeleton(const std::string& sName, rid_t rid,
                   const SE::FlatBuffers::Skeleton* pFB)
        : ResourceHolder(rid, sName) {

        LoadFromFB(pFB);
}

// ---------------------------------------------------------------------------
// Decode FlatBuffer data into runtime structures
// ---------------------------------------------------------------------------
void Skeleton::LoadFromFB(const SE::FlatBuffers::Skeleton* pFB) {

        if (!pFB || !pFB->bones()) {
                log_e("Skeleton: LoadFromFB: null or empty skeleton for '{}'", sName);
                return;
        }

        const uint32_t boneCount = static_cast<uint32_t>(pFB->bones()->size());
        vBones.reserve(boneCount);

        for (const auto* fbBone : *pFB->bones()) {
                if (!fbBone) continue;

                BoneData bone;

                // Name
                if (fbBone->name()) {
                        std::strncpy(bone.name, fbBone->name()->c_str(), sizeof(bone.name) - 1);
                        bone.name[sizeof(bone.name) - 1] = '\0';
                }

                bone.parentIndex = fbBone->parent_index();

                // Bind pose — position
                if (fbBone->bind_pos()) {
                        bone.bindPos = glm::vec3(fbBone->bind_pos()->x(),
                                                 fbBone->bind_pos()->y(),
                                                 fbBone->bind_pos()->z());
                }

                // Bind pose — rotation (FlatBuffer Vec4 stores x,y,z,w matching glm layout)
                if (fbBone->bind_rot()) {
                        bone.bindRot = glm::quat(fbBone->bind_rot()->w(),  // glm: w,x,y,z ctor
                                                 fbBone->bind_rot()->x(),
                                                 fbBone->bind_rot()->y(),
                                                 fbBone->bind_rot()->z());
                }

                // Bind pose — scale
                if (fbBone->bind_scale()) {
                        bone.bindScale = glm::vec3(fbBone->bind_scale()->x(),
                                                   fbBone->bind_scale()->y(),
                                                   fbBone->bind_scale()->z());
                }

                // Inverse bind matrix — stored as four column Vec4s
                if (fbBone->inv_bind_matrix()) {
                        const auto* m = fbBone->inv_bind_matrix();
                        // glm::mat4 is column-major; construct from column vectors
                        bone.invBindMatrix = glm::mat4(
                                glm::vec4(m->col0().x(), m->col0().y(), m->col0().z(), m->col0().w()),
                                glm::vec4(m->col1().x(), m->col1().y(), m->col1().z(), m->col1().w()),
                                glm::vec4(m->col2().x(), m->col2().y(), m->col2().z(), m->col2().w()),
                                glm::vec4(m->col3().x(), m->col3().y(), m->col3().z(), m->col3().w())
                        );
                }

                vBones.push_back(bone);
        }

        // --- Bone masks ---
        if (pFB->masks()) {
                vMasks.reserve(pFB->masks()->size());
                for (const auto* fbMask : *pFB->masks()) {
                        if (!fbMask || !fbMask->name()) continue;

                        BoneMask mask;
                        mask.name = StrID(fbMask->name()->str());

                        // Initialise all weights to 0 (fully base layer)
                        mask.weights.assign(boneCount, 0.0f);

                        if (fbMask->entries()) {
                                for (const auto* entry : *fbMask->entries()) {
                                        if (!entry) continue;
                                        const uint16_t idx = entry->bone_index();
                                        if (idx < boneCount) {
                                                mask.weights[idx] = entry->weight();
                                        }
                                }
                        }

                        vMasks.push_back(std::move(mask));
                }
        }

        log_d("Skeleton: loaded '{}' bones={} masks={}",
              sName, vBones.size(), vMasks.size());
}

// ---------------------------------------------------------------------------
const Skeleton::BoneMask* Skeleton::FindMask(StrID name) const {

        for (const auto& mask : vMasks) {
                if (mask.name == name) {
                        return &mask;
                }
        }
        return nullptr;
}

// ---------------------------------------------------------------------------
std::string Skeleton::Str() const {
        return fmt::format("Skeleton['{}' bones={} masks={}]",
                           sName, vBones.size(), vMasks.size());
}

} // namespace SE

#endif // SE_IMPL
