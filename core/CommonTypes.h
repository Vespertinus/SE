#ifndef __COMMON_TYPES_H__
#define __COMMON_TYPES_H__ 1

namespace SE {

enum class TextureUnit : int32_t {
        DIFFUSE         = 0,
        NORMAL          = 1,
        SPECULAR        = 2,
        ENV             = 3,
        SHADOW          = 4,
        BUFFER          = 5,
        RENDER_BUFFER   = 6,
        CUSTOM          = 7,
        //DEPTH
        //etc
        //MAX_TEXTURE_IMAGE_UNITS = 16
        UNKNOWN         = 255
};

struct VertexIndexType {
        static const uint32_t BYTE      = 0;
        static const uint32_t SHORT     = 1;
        static const uint32_t INT       = 2;

        uint32_t        type;
        uint32_t        size;
};

struct UniformUnitInfo {
        enum class Type : uint8_t {
                TRANSFORM       = 0,
                MATERIAL        = 1,
                CAMERA          = 2,
                ANIMATION       = 3,
                OBJECT          = 4,
                LIGHTING        = 5,
                CUSTOM          = 7,

                MAX             = CUSTOM,
                UNKNOWN         = 255
        };

        //Type            id;
        std::string     sName;
        uint16_t        initial_block_cnt;
        //THINK GL_DYNAMIC_DRAW | GL_STREAM_DRAW
};

enum class DepthFunc : uint32_t {
        ALWAYS  = 0,
        EQUAL,
        NOTEQUAL,
        LESS,
        LEQUAL,
        GREATER,
        GEQUAL
};

}
#endif
