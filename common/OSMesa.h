
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
        ~OSMesa() throw();

        void MakeCurrent(std::vector<GLubyte> & vRenderBuffer);

};

} // namespace SE

#include <OSMesa.tcc>

#endif

