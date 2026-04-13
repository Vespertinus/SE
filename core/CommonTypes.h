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
        EMISSIVE        = 8,   // "EmissiveTex" — per-material emissive texture map
        HDR             = 9,   // "HdrTex"      — HDR accumulation result
        SSAO_TEX        = 10,  // "SSAOTex"     — SSAO blurred result
        NOISE           = 11,  // "NoiseTex"    — SSAO noise texture
        OIT_ACCUM       = 12,  // "OitAccumTex" — OIT weighted accumulation (RGBA16F)
        OIT_REVEALAGE   = 13,  // "OitRevealTex"     — OIT revealage product (R16F)
        PREFILTERED_ENV = 14,  // "PrefilteredEnvTex" — roughness-prefiltered specular env
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

enum class BlendFactor : uint32_t {
        ZERO,
        ONE,
        SRC_COLOR,
        ONE_MINUS_SRC_COLOR,
        DST_COLOR,
        ONE_MINUS_DST_COLOR,
        SRC_ALPHA,
        ONE_MINUS_SRC_ALPHA,
        DST_ALPHA,
        ONE_MINUS_DST_ALPHA
};

enum class CullFace : uint32_t {
        FRONT,
        BACK,
        FRONT_AND_BACK
};

// Unreal-style blend mode — controls how fragment output combines with the framebuffer.
// Opaque/Masked route through the geometry (deferred) pass;
// Translucent/Additive/Modulate/AlphaComposite route through the forward transparent pass.
enum class BlendMode : uint32_t {
        Opaque,          // Fully solid, no blending, depth write ON
        Masked,          // Binary alpha discard (alpha-test), depth write ON
        Translucent,     // Standard alpha-blend: SRC_ALPHA, ONE_MINUS_SRC_ALPHA
        Additive,        // Additive blend: ONE, ONE
        Modulate,        // Multiply/darken: DST_COLOR, ZERO
        AlphaComposite   // Premultiplied alpha: ONE, ONE_MINUS_SRC_ALPHA
};

enum class ClearBuffer : uint32_t {
        COLOR   = 1,
        DEPTH   = 2,
        STENCIL = 4
};

inline ClearBuffer operator|(ClearBuffer a, ClearBuffer b) {
        return static_cast<ClearBuffer>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

enum class VertexAttrib : uint8_t {
        Position     = 0,
        Normal       = 1,
        TexCoord0    = 2,
        TexCoord1    = 3,
        TexCoord2    = 4,
        TexCoord3    = 5,
        Tangent      = 6,
        JointWeights = 7,
        JointIndices = 8,
        Color        = 9,
        Custom0      = 10,
        Custom1      = 11,
        Unknown      = 255
};

inline const char * VertexAttribName(VertexAttrib a) {
        switch (a) {
                case VertexAttrib::Position:     return "Position";
                case VertexAttrib::Normal:       return "Normal";
                case VertexAttrib::TexCoord0:    return "TexCoord0";
                case VertexAttrib::TexCoord1:    return "TexCoord1";
                case VertexAttrib::TexCoord2:    return "TexCoord2";
                case VertexAttrib::TexCoord3:    return "TexCoord3";
                case VertexAttrib::Tangent:      return "Tangent";
                case VertexAttrib::JointWeights: return "JointWeights";
                case VertexAttrib::JointIndices: return "JointIndices";
                case VertexAttrib::Color:        return "Color";
                case VertexAttrib::Custom0:      return "Custom0";
                case VertexAttrib::Custom1:      return "Custom1";
                default:                         return "Unknown";
        }
}

}
#endif
