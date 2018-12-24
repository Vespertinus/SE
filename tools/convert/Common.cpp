
#include "Common.h"
#include <Util.h>

namespace SE {
namespace TOOLS {

void ImportCtx::FixPath(std::string & sPath) {

        if (!sCutPath) { return; }

        sPath = std::regex_replace(sPath, sCutPath.value(), sReplace);
}

VertexIndex::VertexIndex() : last_index(0) { ;; }

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
