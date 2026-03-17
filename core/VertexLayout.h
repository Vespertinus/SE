
#ifndef __VERTEX_LAYOUT_H__
#define __VERTEX_LAYOUT_H__ 1

#include <cstdint>
#include <CommonTypes.h>

namespace SE {

struct VertexAttributeDesc {
        VertexAttrib attrib;
        uint32_t     offset;    // byte offset within vertex
        uint32_t     elem_size; // number of float components
};

struct VertexLayout {
        const VertexAttributeDesc * pAttrs;
        uint32_t                    attr_count;
        uint32_t                    stride;   // total bytes per vertex

        // Returns pointer to descriptor for attr, or nullptr
        const VertexAttributeDesc * Find(VertexAttrib attrib) const;

        // Common layouts
        static const VertexLayout & PosNormTanUV(); // 44 B: pos(3)+norm(3)+tan(3)+uv(2)
        static const VertexLayout & PosNormUV();    // 32 B: pos(3)+norm(3)+uv(2)
        static const VertexLayout & PosColor();     // 28 B: pos(3)+color(4)
        static const VertexLayout & PosOnly();      // 12 B: pos(3)
};

inline const VertexAttributeDesc * VertexLayout::Find(VertexAttrib attrib) const {
        for (uint32_t i = 0; i < attr_count; ++i) {
                if (pAttrs[i].attrib == attrib) return &pAttrs[i];
        }
        return nullptr;
}

inline const VertexLayout & VertexLayout::PosNormTanUV() {
        static const VertexAttributeDesc pAttrs[] = {
                { VertexAttrib::Position,  0, 3 },
                { VertexAttrib::Normal,   12, 3 },
                { VertexAttrib::Tangent,  24, 3 },
                { VertexAttrib::TexCoord0,36, 2 },
        };
        static const VertexLayout layout { pAttrs, 4, 44 };
        return layout;
}

inline const VertexLayout & VertexLayout::PosNormUV() {
        static const VertexAttributeDesc pAttrs[] = {
                { VertexAttrib::Position,  0, 3 },
                { VertexAttrib::Normal,   12, 3 },
                { VertexAttrib::TexCoord0,24, 2 },
        };
        static const VertexLayout layout { pAttrs, 3, 32 };
        return layout;
}

inline const VertexLayout & VertexLayout::PosColor() {
        static const VertexAttributeDesc pAttrs[] = {
                { VertexAttrib::Position,  0, 3 },
                { VertexAttrib::Color,    12, 4 },
        };
        static const VertexLayout layout { pAttrs, 2, 28 };
        return layout;
}

inline const VertexLayout & VertexLayout::PosOnly() {
        static const VertexAttributeDesc pAttrs[] = {
                { VertexAttrib::Position, 0, 3 },
        };
        static const VertexLayout layout { pAttrs, 1, 12 };
        return layout;
}

} // namespace SE

#endif
