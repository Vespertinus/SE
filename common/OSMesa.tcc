
namespace SE {


OSMesa::OSMesa(const WindowSettings & oNewSettings) : oSettings(oNewSettings) { 

        pMesaCtx = OSMesaCreateContextExt( oSettings.format, 
                                           oSettings.depth,
                                           oSettings.stencil,
                                           oSettings.accum,
                                           NULL );
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
