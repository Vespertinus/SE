
#ifndef __X11WINDOW_H__
#define __X11WINDOW_H__ 1

// OpenGL include
#include <GL/osmesa.h>

namespace SE {

struct WindowSettings {

        uint32_t                format;
        int32_t                 depth,
                                stencil,
                                accum;

        int32_t                 width,
                                height;
        std::vector<GLubyte>  & vRenderBuffer;
};

class OSMesa {


        WindowSettings  oSettings;
        OSMesaContext   pMesaCtx;

        public:

        OSMesa(const WindowSettings & oSettings);
        ~OSMesa() noexcept;

        ret_code_t MakeCurrent(std::vector<GLubyte> & vRenderBuffer);
        ret_code_t UpdateDimension(const int32_t new_width, const int32_t new_height);
};

} // namespace SE

#ifdef SE_IMPL
#include <OSMesa.tcc>
#endif

#endif

