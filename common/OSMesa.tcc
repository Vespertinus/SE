
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


OSMesa::~OSMesa() throw() { 

        OSMesaDestroyContext( pMesaCtx );
        log_d("destroyed");
}

void OSMesa::MakeCurrent(std::vector<GLubyte> & vRenderBuffer) {

        oSettings.vRenderBuffer = vRenderBuffer;

        if (!OSMesaMakeCurrent( pMesaCtx, 
                                &vRenderBuffer[0] , 
                                GL_UNSIGNED_BYTE, 
                                oSettings.width, 
                                oSettings.height) ) {
                throw(std::runtime_error("OSMesa::MakeCurrent OSMesaMakeCurrent failed!"));
                //TODO rewrite on err code handling
        }
}


} //namespace SE
