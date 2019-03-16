
namespace SE {


OSMesa::OSMesa(const WindowSettings & oNewSettings) : oSettings(oNewSettings) {

        int attribs[] = {
                OSMESA_FORMAT,                  (int)oSettings.format,
                OSMESA_DEPTH_BITS,              oSettings.depth,
                OSMESA_STENCIL_BITS,            oSettings.stencil,
                OSMESA_ACCUM_BITS,              oSettings.accum,
                OSMESA_PROFILE,                 OSMESA_CORE_PROFILE,
                OSMESA_CONTEXT_MAJOR_VERSION,   3,
                OSMESA_CONTEXT_MINOR_VERSION,   3,
                0
        };

        pMesaCtx = OSMesaCreateContextAttribs(attribs, NULL);

        if (!pMesaCtx) {
                throw (std::runtime_error("OSMesa::OSMesa: OSMesaCreateContext failed"));
        }

        oSettings.vRenderBuffer.resize(oSettings.width * oSettings.height * 4 /*RGBA*/ * sizeof(GLubyte));

        if (!OSMesaMakeCurrent( pMesaCtx,
                                &oSettings.vRenderBuffer[0] ,
                                GL_UNSIGNED_BYTE,
                                oSettings.width,
                                oSettings.height) ) {
                throw(std::runtime_error("OSMesa::OSMesa: OSMesaMakeCurrent failed!"));
        }
        log_d("initialized");
}


OSMesa::~OSMesa() noexcept {

        OSMesaDestroyContext( pMesaCtx );
        log_d("destroyed");
}

ret_code_t OSMesa::MakeCurrent(std::vector<GLubyte> & vRenderBuffer) {

        /**
         current implementation max width \ height == 16384
         */

        vRenderBuffer.resize(oSettings.width * oSettings.height * 4 /*RGBA*/ * sizeof(GLubyte));

        if (!OSMesaMakeCurrent( pMesaCtx,
                                &vRenderBuffer[0],
                                GL_UNSIGNED_BYTE,
                                oSettings.width,
                                oSettings.height) ) {
                log_e("OSMesaMakeCurrent failed!");
                return uWRONG_INPUT_DATA;
        }

        oSettings.vRenderBuffer = vRenderBuffer;

        return uSUCCESS;
}

ret_code_t OSMesa::UpdateDimension(const int32_t new_width, const int32_t new_height) {

        oSettings.vRenderBuffer.resize(new_width * new_height * 4 /*RGBA*/ * sizeof(GLubyte));

        if (!OSMesaMakeCurrent( pMesaCtx,
                                &oSettings.vRenderBuffer[0],
                                GL_UNSIGNED_BYTE,
                                new_width,
                                new_height) ) {
                log_e("OSMesaMakeCurrent failed!");
                return uWRONG_INPUT_DATA;
        }

        oSettings.width  = new_width;
        oSettings.height = new_height;

        return uSUCCESS;
}


} //namespace SE
