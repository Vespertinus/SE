
#include <fstream>
#include <algorithm>

#include <Logging.h>
#include <flatbuffers/flatbuffers.h>
#include <AnimationClip_generated.h>
#include <AnimationSkeleton_generated.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include "FlatBuffersAnimWriter.h"

namespace SE {
namespace TOOLS {

std::tuple<flatbuffers::Offset<SE::FlatBuffers::AnimationClip>, SE::ret_code_t>
SerializeAnimClip(const AnimClipData & oClip, flatbuffers::FlatBufferBuilder & oBuilder) {

        using namespace SE::FlatBuffers;

        std::vector<flatbuffers::Offset<CurveChannel>> vChannelOffsets;
        vChannelOffsets.reserve(oClip.vChannels.size());

        for (auto & ch : oClip.vChannels) {

                auto times_fb  = oBuilder.CreateVector(ch.vTimes);
                auto values_fb = oBuilder.CreateVector(ch.vValues);

                // Map importer format to FlatBuffer CurveFormat
                CurveFormat fbFmt;
                switch (ch.oFormat) {
                case AnimCurveChannel::Format::HermiteF32:  fbFmt = CurveFormat::HermiteF32; break;
                case AnimCurveChannel::Format::LinearF32:   fbFmt = CurveFormat::LinearF32;  break;
                case AnimCurveChannel::Format::ConstantF32: fbFmt = CurveFormat::ConstantF32; break;
                default:                                    fbFmt = CurveFormat::StepF32;    break;
                }

                flatbuffers::Offset<flatbuffers::Vector<float>> tangents_fb = 0;
                if (!ch.vTangents.empty()) {
                        tangents_fb = oBuilder.CreateVector(ch.vTangents);
                }

                auto channel_fb = CreateCurveChannel(
                        oBuilder,
                        ch.bone_index,
                        ch.target,
                        fbFmt,
                        times_fb,
                        values_fb,
                        tangents_fb,
                        0.0f, // quant_min
                        1.0f  // quant_max
                );

                vChannelOffsets.push_back(channel_fb);
        }

        auto channels_fb = oBuilder.CreateVector(vChannelOffsets);

        // Serialize AnimEvents (pre-sorted by time at import)
        std::vector<flatbuffers::Offset<AnimEvent>> vEventOffsets;
        vEventOffsets.reserve(oClip.vEvents.size());
        for (auto & ev : oClip.vEvents) {
                vEventOffsets.push_back(
                        CreateAnimEvent(oBuilder, ev.time, oBuilder.CreateString(ev.sName), ev.value));
        }
        auto events_fb = oBuilder.CreateVector(vEventOffsets);

        auto clip_fb = CreateAnimationClip(
                oBuilder,
                1,              // schema_version
                oClip.duration,
                oClip.looping,
                channels_fb,
                events_fb
        );

        return {clip_fb, uSUCCESS};
}


SE::ret_code_t WriteAnimClip(const std::string & sPath, const AnimClipData & oClip) {

        flatbuffers::FlatBufferBuilder oBuilder(1024 * 1024);

        auto [clip_fb, rc] = SerializeAnimClip(oClip, oBuilder);
        if (rc != uSUCCESS) return rc;

        FinishAnimationClipBuffer(oBuilder, clip_fb);

        // Write to file
        std::ofstream oFile(sPath, std::ios::binary | std::ios::trunc);
        if (!oFile.is_open()) {
                log_e("WriteAnimClip: failed to open output file: '{}'", sPath);
                return uWRITE_FILE_ERROR;
        }

        oFile.write(reinterpret_cast<const char *>(oBuilder.GetBufferPointer()),
                    static_cast<std::streamsize>(oBuilder.GetSize()));

        if (!oFile) {
                log_e("WriteAnimClip: write error for file: '{}'", sPath);
                return uWRITE_FILE_ERROR;
        }

        log_i("WriteAnimClip: wrote '{}' ({} channels, {} events, {:.3f}s)",
              sPath, oClip.vChannels.size(), oClip.vEvents.size(), oClip.duration);
        return uSUCCESS;
}


SE::ret_code_t WriteSkeleton(const std::string & sPath, const Skeleton & oSkel) {

        using namespace SE::FlatBuffers;

        flatbuffers::FlatBufferBuilder oBuilder(64 * 1024);

        std::vector<flatbuffers::Offset<SkeletonBone>> vBoneOffsets;
        vBoneOffsets.reserve(oSkel.vJoints.size());

        for (const auto & jd : oSkel.vJoints) {

                auto name_fb = oBuilder.CreateString(jd.sName);

                // local bind pose
                Vec3 bpos{ jd.oBindLocal.vBindPos.x,  jd.oBindLocal.vBindPos.y,  jd.oBindLocal.vBindPos.z };
                Vec4 brot{ jd.oBindLocal.vBindRot.x,  jd.oBindLocal.vBindRot.y,
                           jd.oBindLocal.vBindRot.z,  jd.oBindLocal.vBindRot.w };
                Vec3 bscl{ jd.oBindLocal.vBindScale.x, jd.oBindLocal.vBindScale.y, jd.oBindLocal.vBindScale.z };

                // Build 4×4 inv-bind matrix.
                // For GLTF: use the exact mat4 from the file (no SQT round-trip).
                // For FBX:  reconstruct from SQT (T * R * S).
                glm::mat4 m;
                if (jd.inv_bind_is_mat4) {
                        m = jd.mInvBindMat4;
                } else {
                        glm::quat q{ jd.oInvBind.vBindRot.w, jd.oInvBind.vBindRot.x,
                                     jd.oInvBind.vBindRot.y, jd.oInvBind.vBindRot.z };
                        m = glm::translate(glm::mat4(1.0f),
                                           glm::vec3(jd.oInvBind.vBindPos.x,
                                                     jd.oInvBind.vBindPos.y,
                                                     jd.oInvBind.vBindPos.z))
                          * glm::mat4_cast(q)
                          * glm::scale(glm::mat4(1.0f),
                                       glm::vec3(jd.oInvBind.vBindScale.x,
                                                 jd.oInvBind.vBindScale.y,
                                                 jd.oInvBind.vBindScale.z));
                }

                // glm::mat4 is column-major; ColMat4 holds 4 Vec4 columns
                ColMat4 cm{
                        Vec4{m[0][0], m[0][1], m[0][2], m[0][3]},
                        Vec4{m[1][0], m[1][1], m[1][2], m[1][3]},
                        Vec4{m[2][0], m[2][1], m[2][2], m[2][3]},
                        Vec4{m[3][0], m[3][1], m[3][2], m[3][3]}
                };

                vBoneOffsets.push_back(
                        CreateSkeletonBone(oBuilder, name_fb, jd.parent_index,
                                           &bpos, &brot, &bscl, &cm));
        }

        auto bones_fb = oBuilder.CreateVector(vBoneOffsets);
        auto skel_fb  = CreateSkeleton(oBuilder, 1 /* schema_version */, bones_fb);
        FinishSkeletonBuffer(oBuilder, skel_fb);

        std::ofstream oFile(sPath, std::ios::binary | std::ios::trunc);
        if (!oFile.is_open()) {
                log_e("WriteSkeleton: failed to open output file: '{}'", sPath);
                return uWRITE_FILE_ERROR;
        }

        oFile.write(reinterpret_cast<const char *>(oBuilder.GetBufferPointer()),
                    static_cast<std::streamsize>(oBuilder.GetSize()));

        if (!oFile) {
                log_e("WriteSkeleton: write error for file: '{}'", sPath);
                return uWRITE_FILE_ERROR;
        }

        log_i("WriteSkeleton: wrote '{}' ({} bones)", sPath, oSkel.vJoints.size());
        return uSUCCESS;
}

}
}
