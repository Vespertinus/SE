
#include "Common.h"
#include <Util.h>

#include <array>

namespace SE {
namespace TOOLS {

void ImportCtx::FixPath(std::string & sPath) {

        if (!oCutPath) { return; }

        sPath = std::regex_replace(sPath, oCutPath.value(), sReplace);
}

VertexIndex::VertexIndex() : last_index(0) { ;; }

VertexIndex::VertexIndex(uint32_t base_offset) : last_index(base_offset) { ;; }

bool VertexIndex::Get(std::vector<float> & mData, uint32_t & index) {

        bool     res;
        uint64_t key    = Hash64(reinterpret_cast<char *>(mData.data()), mData.size() * sizeof(float));
        auto     it     = mIndex.find(key);

        if (it != mIndex.end()) {
                index = it->second;
                res = false;
        }
        else {
                index = last_index++;
                mIndex[key] = index;
                res = true;
        }

        return res;
}

void VertexIndex::Clear() {

        mIndex.clear();
        last_index = 0;
}

uint32_t VertexIndex::Size() const {
        return last_index;
}

void EnforceQuatContinuity(std::vector<AnimCurveChannel> & vChannels) {

        constexpr uint8_t kRotTarget0 = 3;   // targets 3,4,5,6 = rot x,y,z,w
        constexpr size_t  kRotAxes    = 4;

        // Gather the 4 rotation channels of each bone (bone -> [x,y,z,w])
        std::unordered_map<uint16_t, std::array<AnimCurveChannel *, kRotAxes>> mBones;
        for (auto & ch : vChannels) {
                if (ch.target >= kRotTarget0 && ch.target < kRotTarget0 + kRotAxes) {
                        mBones[ch.bone_index][ch.target - kRotTarget0] = &ch;
                }
        }

        for (auto & [bone, vAxes] : mBones) {

                bool complete = true;
                const size_t key_cnt = vAxes[0] ? vAxes[0]->vValues.size() : 0;
                for (size_t axis = 0; axis < kRotAxes; ++axis) {
                        if (!vAxes[axis] || vAxes[axis]->vValues.size() != key_cnt) {
                                complete = false;
                                break;
                        }
                }
                if (!complete || key_cnt < 2) continue;   // partial rig — leave as-is

                for (size_t k = 1; k < key_cnt; ++k) {

                        float dot = 0.0f;
                        for (size_t axis = 0; axis < kRotAxes; ++axis) {
                                dot += vAxes[axis]->vValues[k - 1] * vAxes[axis]->vValues[k];
                        }
                        if (dot >= 0.0f) continue;

                        // Same rotation, opposite hemisphere — flip key k in place
                        for (size_t axis = 0; axis < kRotAxes; ++axis) {
                                AnimCurveChannel & ch = *vAxes[axis];
                                ch.vValues[k] = -ch.vValues[k];
                                if (ch.vTangents.size() == key_cnt) {
                                        ch.vTangents[k] = -ch.vTangents[k];
                                }
                        }
                }
        }
}

TPackVertexIndex PackVertexIndexInit(const uint32_t index_size, MeshData::TIndexVariant & oIndex) {

        if (index_size <= 255) {
                //oIndex = std::vector<uint8_t>(); //already here
                return PackValue<uint8_t>;
        }
        else if (index_size <= 65535) {
                oIndex = std::vector<uint16_t>();
                return PackValue<uint16_t>;
        }
        else {
                oIndex = std::vector<uint32_t>();
                return PackValue<uint32_t>;
        }
}

}
}
